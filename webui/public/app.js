const sendForm = document.getElementById("sendForm");
const sendStatus = document.getElementById("sendStatus");
const receiveStatus = document.getElementById("receiveStatus");
const logOutput = document.getElementById("logOutput");
const startReceive = document.getElementById("startReceive");
const stopReceive = document.getElementById("stopReceive");
const destInput = document.getElementById("dest");
const mirrorRoomInput = document.getElementById("mirrorRoom");
const mirrorStatus = document.getElementById("mirrorStatus");
const mirrorVideo = document.getElementById("mirrorVideo");
const startShare = document.getElementById("startShare");
const stopShare = document.getElementById("stopShare");
const joinViewer = document.getElementById("joinViewer");
const leaveViewer = document.getElementById("leaveViewer");

const rtcConfig = {
  iceServers: [{ urls: "stun:stun.l.google.com:19302" }],
};

let ws = null;
let clientId = "";
let joinedRoom = "";
let mirrorRole = "none";
let localStream = null;
let viewerPeer = null;
const hostPeers = new Map();

function setStatus(el, message, isError = false) {
  el.textContent = message;
  el.classList.toggle("error", isError);
}

function setMirrorStatus(message, isError = false) {
  setStatus(mirrorStatus, message, isError);
}

function wsUrl() {
  const protocol = globalThis.location.protocol === "https:" ? "wss:" : "ws:";
  return `${protocol}//${globalThis.location.host}/ws`;
}

function ensureSocket() {
  if (ws?.readyState === WebSocket.OPEN) {
    return Promise.resolve();
  }

  return new Promise((resolve, reject) => {
    ws = new WebSocket(wsUrl());

    ws.onopen = () => {
      resolve();
    };

    ws.onerror = () => reject(new Error("Signaling socket error"));

    ws.onmessage = async (event) => {
      let message;
      try {
        message = JSON.parse(event.data);
      } catch {
        return;
      }

      if (message.type === "welcome") {
        clientId = message.clientId;
        return;
      }

      if (message.type === "joined") {
        joinedRoom = message.room;
        mirrorRole = message.role;
        setMirrorStatus(`Joined room '${joinedRoom}' as ${mirrorRole}`);
        return;
      }

      if (message.type === "peer-joined" && mirrorRole === "host" && localStream) {
        await createHostPeer(message.peerId);
        return;
      }

      if (message.type === "peer-left") {
        if (hostPeers.has(message.peerId)) {
          hostPeers.get(message.peerId).close();
          hostPeers.delete(message.peerId);
          setMirrorStatus(`Viewer ${message.peerId} left`);
        }
      }

      if (message.type === "signal") {
        await handleSignal(message);
      }
    };
  });
}

function sendSignal(target, signalType, data) {
  if (ws?.readyState !== WebSocket.OPEN) {
    return;
  }
  ws.send(
    JSON.stringify({
      type: "signal",
      target,
      signalType,
      data,
    })
  );
}

function leaveRoom() {
  if (ws?.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify({ type: "leave" }));
  }
  joinedRoom = "";
}

async function joinRoom(role) {
  await ensureSocket();
  const room = (mirrorRoomInput.value || "default").trim() || "default";
  ws.send(
    JSON.stringify({
      type: "join",
      room,
      role,
    })
  );
}

function cleanupHostPeers() {
  for (const pc of hostPeers.values()) {
    pc.close();
  }
  hostPeers.clear();
}

function cleanupViewerPeer() {
  if (viewerPeer) {
    viewerPeer.close();
    viewerPeer = null;
  }
  mirrorVideo.srcObject = null;
}

function stopLocalStream() {
  if (!localStream) {
    return;
  }
  localStream.getTracks().forEach((track) => track.stop());
  localStream = null;
}

async function createHostPeer(peerId) {
  const pc = new RTCPeerConnection(rtcConfig);
  hostPeers.set(peerId, pc);

  localStream.getTracks().forEach((track) => {
    pc.addTrack(track, localStream);
  });

  pc.onicecandidate = (event) => {
    if (event.candidate) {
      sendSignal(peerId, "candidate", event.candidate);
    }
  };

  const offer = await pc.createOffer();
  await pc.setLocalDescription(offer);
  sendSignal(peerId, "offer", offer);
  setMirrorStatus(`Streaming to viewer ${peerId}`);
}

async function ensureViewerPeer(remotePeerId) {
  if (viewerPeer) {
    return viewerPeer;
  }
  const pc = new RTCPeerConnection(rtcConfig);
  viewerPeer = pc;

  pc.onicecandidate = (event) => {
    if (event.candidate) {
      sendSignal(remotePeerId, "candidate", event.candidate);
    }
  };

  pc.ontrack = (event) => {
    mirrorVideo.srcObject = event.streams[0];
    setMirrorStatus("Receiving mirrored screen");
  };

  return pc;
}

async function handleSignal(message) {
  const { from, signalType, data } = message;

  if (mirrorRole === "host") {
    const pc = hostPeers.get(from);
    if (!pc) {
      return;
    }
    if (signalType === "answer") {
      await pc.setRemoteDescription(new RTCSessionDescription(data));
    }
    if (signalType === "candidate") {
      await pc.addIceCandidate(new RTCIceCandidate(data));
    }
    return;
  }

  if (mirrorRole === "viewer") {
    const pc = await ensureViewerPeer(from);
    if (signalType === "offer") {
      await pc.setRemoteDescription(new RTCSessionDescription(data));
      const answer = await pc.createAnswer();
      await pc.setLocalDescription(answer);
      sendSignal(from, "answer", answer);
    }
    if (signalType === "candidate") {
      await pc.addIceCandidate(new RTCIceCandidate(data));
    }
  }
}

async function startHostSharing() {
  try {
    stopViewerMode();
    localStream = await navigator.mediaDevices.getDisplayMedia({
      video: true,
      audio: true,
    });
    await joinRoom("host");
    setMirrorStatus("Screen share started. Waiting for viewers...");

    const [videoTrack] = localStream.getVideoTracks();
    if (videoTrack) {
      videoTrack.onended = () => {
        stopHostSharing();
      };
    }
  } catch {
    setMirrorStatus("Failed to start screen sharing", true);
  }
}

function stopHostSharing() {
  leaveRoom();
  cleanupHostPeers();
  stopLocalStream();
  if (mirrorRole === "host") {
    mirrorRole = "none";
  }
  setMirrorStatus("Screen sharing stopped");
}

async function startViewerMode() {
  try {
    stopHostSharing();
    cleanupViewerPeer();
    await joinRoom("viewer");
    setMirrorStatus("Joined as viewer. Waiting for host stream...");
  } catch {
    setMirrorStatus("Failed to join as viewer", true);
  }
}

function stopViewerMode() {
  leaveRoom();
  cleanupViewerPeer();
  if (mirrorRole === "viewer") {
    mirrorRole = "none";
  }
  setMirrorStatus("Viewer stopped");
}

function refreshStatus() {
  return fetch("/api/status")
    .then((res) => res.json())
    .then((data) => {
      setStatus(
        receiveStatus,
        data.receiverRunning ? "Receiver running" : "Receiver stopped"
      );
    })
    .catch(() => {
      setStatus(receiveStatus, "Status unavailable", true);
    });
}

function refreshLogs() {
  return fetch("/api/logs")
    .then((res) => res.json())
    .then((data) => {
      logOutput.textContent = (data.logs || []).join("\n");
    })
    .catch(() => {
      logOutput.textContent = "";
    });
}

sendForm.addEventListener("submit", async (e) => {
  e.preventDefault();
  setStatus(sendStatus, "Sending...");

  const formData = new FormData(sendForm);
  try {
    const res = await fetch("/api/send", {
      method: "POST",
      body: formData,
    });
    const data = await res.json();
    if (!res.ok) {
      setStatus(sendStatus, data.error || "Send failed", true);
      return;
    }
    setStatus(sendStatus, "Send complete");
  } catch {
    setStatus(sendStatus, "Send failed", true);
  }
});

startReceive.addEventListener("click", async () => {
  setStatus(receiveStatus, "Starting receiver...");
  try {
    const res = await fetch("/api/receive", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ dest: destInput.value.trim() }),
    });
    const data = await res.json();
    if (!res.ok) {
      setStatus(receiveStatus, data.error || "Start failed", true);
      return;
    }
    setStatus(receiveStatus, "Receiver running");
  } catch {
    setStatus(receiveStatus, "Start failed", true);
  }
});

stopReceive.addEventListener("click", async () => {
  setStatus(receiveStatus, "Stopping receiver...");
  try {
    const res = await fetch("/api/receive/stop", { method: "POST" });
    const data = await res.json();
    if (!res.ok) {
      setStatus(receiveStatus, data.error || "Stop failed", true);
      return;
    }
    setStatus(receiveStatus, "Receiver stopped");
  } catch {
    setStatus(receiveStatus, "Stop failed", true);
  }
});

startShare.addEventListener("click", async () => {
  await startHostSharing();
});

stopShare.addEventListener("click", () => {
  stopHostSharing();
});

joinViewer.addEventListener("click", async () => {
  await startViewerMode();
});

leaveViewer.addEventListener("click", () => {
  stopViewerMode();
});

setMirrorStatus("Mirror idle");
setInterval(() => {
  refreshStatus();
  refreshLogs();
}, 2000);

refreshStatus();
refreshLogs();
