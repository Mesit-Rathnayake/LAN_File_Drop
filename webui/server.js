const express = require("express");
const multer = require("multer");
const path = require("node:path");
const fs = require("node:fs");
const http = require("node:http");
const { WebSocketServer } = require("ws");
const { spawn, spawnSync } = require("node:child_process");

const app = express();
function getArgValue(name) {
  const prefix = `--${name}=`;
  for (let i = 2; i < process.argv.length; i += 1) {
    const arg = process.argv[i];
    if (arg.startsWith(prefix)) {
      return arg.slice(prefix.length);
    }
    if (arg === `--${name}` && i + 1 < process.argv.length) {
      return process.argv[i + 1];
    }
  }
  return "";
}

function resolvePort() {
  const cliPort = getArgValue("port");
  const envPort = process.env.PORT || process.env.port || process.env.APP_PORT || "";
  const rawPort = cliPort || envPort || "3000";
  const port = Number.parseInt(rawPort, 10);
  return Number.isInteger(port) && port > 0 ? port : 3000;
}

const PORT = resolvePort();
const ROOT = path.join(__dirname, "..");
const UPLOAD_DIR = path.join(__dirname, "uploads");

function resolveBinPath() {
  if (process.env.LANFILEDROP_BIN) {
    return process.env.LANFILEDROP_BIN;
  }

  const candidates = process.platform === "win32"
    ? [path.join(ROOT, "LANFileDrop.exe"), path.join(ROOT, "LANFileDrop")]
    : [path.join(ROOT, "LANFileDrop"), path.join(ROOT, "LANFileDrop.exe")];

  for (const candidate of candidates) {
    if (fs.existsSync(candidate)) {
      return candidate;
    }
  }

  return candidates[0];
}

const BIN = resolveBinPath();

function toWslPath(inputPath) {
  const normalized = inputPath.replaceAll("\\", "/");
  const match = normalized.match(/^([A-Za-z]):\/(.*)$/);
  if (!match) {
    return normalized;
  }
  return `/mnt/${match[1].toLowerCase()}/${match[2]}`;
}

function canUseWslBackend() {
  if (process.platform !== "win32") {
    return false;
  }

  const force = process.env.LANFILEDROP_USE_WSL;
  if (force === "0" || force === "false") {
    return false;
  }

  const linuxBinPath = path.join(ROOT, "LANFileDrop");
  if (!fs.existsSync(linuxBinPath)) {
    return false;
  }

  const probe = spawnSync("wsl", ["--status"], { stdio: "ignore" });
  return !probe.error;
}

const USE_WSL_BACKEND = canUseWslBackend();

function mapArgsForWsl(rawArgs) {
  const mapped = [];
  let consumeAsPath = false;
  for (const arg of rawArgs) {
    if (consumeAsPath) {
      mapped.push(toWslPath(arg));
      consumeAsPath = false;
      continue;
    }

    mapped.push(arg);
    if (arg === "--file" || arg === "--dest") {
      consumeAsPath = true;
    }
  }
  return mapped;
}

function resolveInvocation(rawArgs) {
  if (USE_WSL_BACKEND) {
    return {
      command: "wsl",
      args: ["--cd", toWslPath(ROOT), "./LANFileDrop", ...mapArgsForWsl(rawArgs)],
      cwd: ROOT,
      mode: "wsl",
    };
  }

  return {
    command: BIN,
    args: rawArgs,
    cwd: ROOT,
    mode: "native",
  };
}

if (!fs.existsSync(UPLOAD_DIR)) {
  fs.mkdirSync(UPLOAD_DIR, { recursive: true });
}

const upload = multer({ dest: UPLOAD_DIR });
const logBuffer = [];
const LOG_LIMIT = 200;
let receiverProc = null;
let nextClientId = 1;
const clients = new Map();

function appendLog(line) {
  logBuffer.push(line);
  if (logBuffer.length > LOG_LIMIT) {
    logBuffer.shift();
  }
}

function binExists() {
  if (USE_WSL_BACKEND) {
    return fs.existsSync(path.join(ROOT, "LANFileDrop"));
  }
  return fs.existsSync(BIN);
}

app.use(express.json());
app.use(express.static(path.join(__dirname, "public")));

app.get("/favicon.ico", (req, res) => {
  res.status(204).end();
});

app.get("/api/status", (req, res) => {
  res.json({ receiverRunning: Boolean(receiverProc) });
});

app.get("/api/logs", (req, res) => {
  res.json({ logs: logBuffer });
});

app.post("/api/receive", (req, res) => {
  if (receiverProc) {
    return res.status(409).json({ error: "Receiver already running" });
  }
  if (!binExists()) {
    return res.status(500).json({ error: "LANFileDrop binary not found" });
  }

  const dest = req.body?.dest || ".";
  const launch = resolveInvocation(["--receive", "--dest", dest]);
  appendLog(`[receive] backend=${launch.mode} cmd=${launch.command}`);
  receiverProc = spawn(launch.command, launch.args, { cwd: launch.cwd });

  receiverProc.stdout.on("data", (chunk) => {
    appendLog(`[receive] ${chunk.toString().trim()}`);
  });
  receiverProc.stderr.on("data", (chunk) => {
    appendLog(`[receive][err] ${chunk.toString().trim()}`);
  });
  receiverProc.on("close", (code) => {
    appendLog(`[receive] exited with code ${code}`);
    receiverProc = null;
  });

  return res.json({ ok: true });
});

app.post("/api/receive/stop", (req, res) => {
  if (!receiverProc) {
    return res.json({ ok: true, alreadyStopped: true });
  }
  receiverProc.kill("SIGTERM");
  return res.json({ ok: true });
});

app.post("/api/send", upload.array("files"), (req, res) => {
  if (!binExists()) {
    return res.status(500).json({ error: "LANFileDrop binary not found" });
  }
  const ip = req.body?.ip || "";
  if (!ip) {
    return res.status(400).json({ error: "IP address is required" });
  }
  if (!req.files || req.files.length === 0) {
    return res.status(400).json({ error: "At least one file is required" });
  }

  const args = ["--send", "--ip", ip];
  req.files.forEach((file) => {
    args.push("--file", file.path);
  });

  const launch = resolveInvocation(args);
  appendLog(`[send] backend=${launch.mode} cmd=${launch.command}`);
  const sendProc = spawn(launch.command, launch.args, { cwd: launch.cwd });
  let output = "";
  let errorOutput = "";

  sendProc.stdout.on("data", (chunk) => {
    const text = chunk.toString();
    output += text;
    appendLog(`[send] ${text.trim()}`);
  });
  sendProc.stderr.on("data", (chunk) => {
    const text = chunk.toString();
    errorOutput += text;
    appendLog(`[send][err] ${text.trim()}`);
  });

  sendProc.on("close", (code) => {
    req.files.forEach((file) => {
      fs.unlink(file.path, () => {});
    });

    if (code === 0) {
      return res.json({ ok: true, output });
    }
    return res.status(500).json({ error: "Send failed", output, errorOutput });
  });
});

const server = http.createServer(app);
const wss = new WebSocketServer({ server, path: "/ws" });

function safeSend(socket, payload) {
  if (socket?.readyState === 1) {
    socket.send(JSON.stringify(payload));
  }
}

function broadcastToRoom(room, payload, excludeId = null) {
  for (const [id, client] of clients.entries()) {
    if (excludeId && id === excludeId) {
      continue;
    }
    if (client.room === room) {
      safeSend(client.socket, payload);
    }
  }
}

wss.on("connection", (socket) => {
  const clientId = `c${nextClientId++}`;
  clients.set(clientId, {
    socket,
    room: null,
    role: "viewer",
  });

  safeSend(socket, { type: "welcome", clientId });

  socket.on("message", (raw) => {
    let message;
    try {
      message = JSON.parse(raw.toString());
    } catch {
      return;
    }

    const client = clients.get(clientId);
    if (!client || !message?.type) {
      return;
    }

    if (message.type === "join") {
      const room = (message.room || "default").toString();
      const role = message.role === "host" ? "host" : "viewer";
      client.room = room;
      client.role = role;
      safeSend(socket, { type: "joined", room, role, clientId });
      broadcastToRoom(room, { type: "peer-joined", peerId: clientId, role }, clientId);
      return;
    }

    if (message.type === "signal") {
      const targetId = message.target;
      const target = clients.get(targetId);
      if (!target || !client.room || target.room !== client.room) {
        return;
      }
      safeSend(target.socket, {
        type: "signal",
        from: clientId,
        signalType: message.signalType,
        data: message.data,
      });
      return;
    }

    if (message.type === "leave") {
      if (client.room) {
        broadcastToRoom(client.room, { type: "peer-left", peerId: clientId }, clientId);
      }
      client.room = null;
      client.role = "viewer";
    }
  });

  socket.on("close", () => {
    const client = clients.get(clientId);
    if (client?.room) {
      broadcastToRoom(client.room, { type: "peer-left", peerId: clientId }, clientId);
    }
    clients.delete(clientId);
  });
});

server.listen(PORT, () => {
  console.log(`Web UI running at http://localhost:${PORT}`);
});
