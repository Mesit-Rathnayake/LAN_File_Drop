const sendForm = document.getElementById("sendForm");
const sendStatus = document.getElementById("sendStatus");
const receiveStatus = document.getElementById("receiveStatus");
const logOutput = document.getElementById("logOutput");
const startReceive = document.getElementById("startReceive");
const stopReceive = document.getElementById("stopReceive");
const destInput = document.getElementById("dest");

function setStatus(el, message, isError = false) {
  el.textContent = message;
  el.classList.toggle("error", isError);
}

async function refreshStatus() {
  try {
    const res = await fetch("/api/status");
    const data = await res.json();
    setStatus(
      receiveStatus,
      data.receiverRunning ? "Receiver running" : "Receiver stopped"
    );
  } catch {
    setStatus(receiveStatus, "Status unavailable", true);
  }
}

async function refreshLogs() {
  try {
    const res = await fetch("/api/logs");
    const data = await res.json();
    logOutput.textContent = (data.logs || []).join("\n");
  } catch {
    logOutput.textContent = "";
  }
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

refreshStatus();
refreshLogs();
setInterval(() => {
  refreshStatus();
  refreshLogs();
}, 2000);
