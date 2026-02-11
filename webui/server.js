const express = require("express");
const multer = require("multer");
const path = require("path");
const fs = require("fs");
const { spawn } = require("child_process");

const app = express();
const PORT = process.env.PORT || 3000;
const ROOT = path.join(__dirname, "..");
const BIN = process.env.LANFILEDROP_BIN || path.join(ROOT, "LANFileDrop");
const UPLOAD_DIR = path.join(__dirname, "uploads");

if (!fs.existsSync(UPLOAD_DIR)) {
  fs.mkdirSync(UPLOAD_DIR, { recursive: true });
}

const upload = multer({ dest: UPLOAD_DIR });
const logBuffer = [];
const LOG_LIMIT = 200;
let receiverProc = null;

function appendLog(line) {
  logBuffer.push(line);
  if (logBuffer.length > LOG_LIMIT) {
    logBuffer.shift();
  }
}

function binExists() {
  return fs.existsSync(BIN);
}

app.use(express.json());
app.use(express.static(path.join(__dirname, "public")));

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

  const dest = (req.body && req.body.dest) ? req.body.dest : ".";
  const args = ["--receive", "--dest", dest];
  receiverProc = spawn(BIN, args, { cwd: ROOT });

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
    return res.status(400).json({ error: "Receiver is not running" });
  }
  receiverProc.kill("SIGTERM");
  return res.json({ ok: true });
});

app.post("/api/send", upload.array("files"), (req, res) => {
  if (!binExists()) {
    return res.status(500).json({ error: "LANFileDrop binary not found" });
  }
  const ip = req.body && req.body.ip ? req.body.ip : "";
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

  const sendProc = spawn(BIN, args, { cwd: ROOT });
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

app.listen(PORT, () => {
  console.log(`Web UI running at http://localhost:${PORT}`);
});
