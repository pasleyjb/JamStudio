#!/usr/bin/env python3
"""Serve the karaoke lyric screen for the Dell on the studio ethernet link.

Jam Studio writes the karaoke flags, the lyrics JSON, and the playhead into
the runtime directory. Pass that directory as --runtime, or set
JAMSTUDIO_RUNTIME. The default is /tmp, which is where the HP build publishes
them. The Dell opens http://<studio-address>:8765.
"""

import json
import os
import shutil
import struct
import subprocess
import sys
import threading
import time
from collections import deque
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


def runtime_directory():
    if "--runtime" in sys.argv:
        index = sys.argv.index("--runtime")
        if index + 1 < len(sys.argv):
            return sys.argv[index + 1]
    return os.environ.get("JAMSTUDIO_RUNTIME", "/tmp")


RUNTIME = runtime_directory()
PROJECT = os.environ.get(
    "JAMSTUDIO_LYRICS_PROJECT",
    "/home/jaypee/Documents/JamStudio/Projects/London calling.jamstudio",
)
LYRICS_JSON = os.path.join(RUNTIME, "jamstudio-lyrics.json")
OPEN_FLAG = os.path.join(RUNTIME, "jamstudio-karaoke.open")
CLOSED_FLAG = os.path.join(RUNTIME, "jamstudio-karaoke.closed")
PLAYHEAD_PATH = os.path.join(RUNTIME, "jamstudio-playhead")
HOST = "0.0.0.0"
PORT = 8765
AUDIO_RATE = 48000
AUDIO_FRAMES = 960  # 20 ms
AUDIO_BYTES = AUDIO_FRAMES * 2 * 2
AUDIO_HEADER = struct.Struct("<4sdBBHI")
MONITOR_SOURCE = "jamstudio_bus_mona"

subscribers = []
subscribers_lock = threading.Lock()
capture_lock = threading.Lock()
capture_thread = None


def lines_from(data):
    lines = []
    for line in data.get("lines", []):
        if not isinstance(line, dict):
            continue
        lines.append(
            {
                "text": (line.get("text") or "").strip(),
                "start": float(line.get("start") or 0.0),
                "end": float(line.get("end") or 0.0),
            }
        )
    return lines


def load_project_lyrics():
    try:
        with open(PROJECT, encoding="utf-8") as handle:
            lyrics = json.load(handle)["embeddedLyrics"]
    except (OSError, ValueError, json.JSONDecodeError, KeyError, TypeError):
        return "", []
    title = (lyrics.get("title") or "").strip() or "London Calling"
    return title, lines_from(lyrics)


PROJECT_TITLE, PROJECT_LINES = load_project_lyrics()


def load_lyrics():
    """Lyrics Jam Studio published for the ethernet display, else the project."""
    try:
        with open(LYRICS_JSON, encoding="utf-8") as handle:
            data = json.load(handle)
    except (OSError, ValueError, json.JSONDecodeError, TypeError):
        return PROJECT_TITLE, PROJECT_LINES

    if not isinstance(data, dict):
        return PROJECT_TITLE, PROJECT_LINES

    lines = lines_from(data)
    title = (data.get("title") or "").strip() or PROJECT_TITLE
    return title, lines


def line_at(seconds, lines):
    """Return previous, current, and next lyric lines for a song position."""
    current = None
    for index, line in enumerate(lines):
        if line["start"] <= seconds:
            current = index
        else:
            break

    def pack(index):
        if index is None or index < 0 or index >= len(lines):
            return None
        line = lines[index]
        if not line["text"]:
            return None
        return {"text": line["text"], "start": line["start"]}

    if current is None:
        nxt = next((i for i, line in enumerate(lines) if line["text"]), None)
        return {"previous": None, "current": None, "next": pack(nxt)}

    # A blank timestamp is a rest. Keep the last sung line as previous
    # and show the next real line as what's coming.
    if not lines[current]["text"]:
        previous = next(
            (i for i in range(current - 1, -1, -1) if lines[i]["text"]),
            None,
        )
        nxt = next(
            (i for i in range(current + 1, len(lines)) if lines[i]["text"]),
            None,
        )
        return {"previous": pack(previous), "current": None, "next": pack(nxt)}

    previous = next(
        (i for i in range(current - 1, -1, -1) if lines[i]["text"]),
        None,
    )
    nxt = next(
        (i for i in range(current + 1, len(lines)) if lines[i]["text"]),
        None,
    )
    return {"previous": pack(previous), "current": pack(current), "next": pack(nxt)}


PAGE = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<title>Karaoke</title>
<style>
  html, body { margin: 0; height: 100%; background: #10141c; color: #eef2f7;
    font-family: -apple-system, "Liberation Sans", "Nimbus Sans", sans-serif;
    overflow: hidden; }
  #viewport { position: fixed; inset: 0; overflow: hidden; touch-action: none;
    -webkit-mask-image: linear-gradient(to bottom, transparent 0%, #000 16%, #000 76%, transparent 92%);
    mask-image: linear-gradient(to bottom, transparent 0%, #000 16%, #000 76%, transparent 92%); }
  #track { position: absolute; left: 6vw; right: 6vw; top: 0; text-align: center;
    will-change: transform; }
  .line { display: block; width: max-content; max-width: none; margin: 0 auto;
    white-space: nowrap; font-size: 6.4vh; line-height: 1.22; font-weight: 700;
    padding: 2.5vh 0; color: rgb(154, 166, 184); opacity: 0.34; }
  #message { position: fixed; inset: 0; display: flex; align-items: center;
    justify-content: center; text-align: center; padding: 0 8vw;
    font-size: 8vh; font-weight: 700; color: #ffd34d; }
  #message[hidden] { display: none; }
  #status { position: fixed; left: 0; right: 0; bottom: 0; text-align: center;
    font-size: 2.2vh; color: #5a6575; letter-spacing: 0.04em;
    padding: 4vh 0 2vh; background: linear-gradient(to top, #10141c 45%, transparent); }
  #title { position: fixed; top: 2.5vh; left: 0; right: 0; text-align: center;
    font-size: 2.4vh; color: #9aa6b8; z-index: 2; }
  #sound { position: fixed; top: 1.8vh; right: 2vw; z-index: 3;
    background: rgba(16, 20, 28, 0.8); color: #9aa6b8;
    border: 1px solid #3a4454; border-radius: 999px;
    font: 700 2vh -apple-system, "Liberation Sans", sans-serif;
    padding: 0.7vh 1.5vw; }
  #sound.on { color: #ffd34d; border-color: #8a6a22; }
  #sync { position: fixed; left: 2vw; bottom: 2.2vh; z-index: 4;
    display: flex; align-items: center; gap: 0.7vw;
    background: rgba(16, 20, 28, 0.88); border: 1px solid #3a4454;
    border-radius: 999px; padding: 0.5vh 0.8vw; }
  #sync button { background: transparent; color: #d5dbe4; border: 0;
    font: 700 2vh -apple-system, "Liberation Sans", sans-serif;
    padding: 0.5vh 0.7vw; }
  #syncRange { width: 14vw; min-width: 90px; accent-color: #ffd34d; }
  #syncLabel { min-width: 8ch; text-align: center;
    font: 700 1.8vh -apple-system, "Liberation Sans", sans-serif; color: #ffd34d; }
</style>
</head>
<body>
<button id="sound" type="button">Sound</button>
<div id="sync">
  <button id="earlier" type="button">Earlier</button>
  <input id="syncRange" type="range" min="-2" max="2" step="0.05" value="0" aria-label="Lyric sync">
  <button id="later" type="button">Later</button>
  <span id="syncLabel">in sync</span>
</div>
<div id="title"></div>
<div id="viewport"><div id="track"></div></div>
<div id="message" hidden></div>
<div id="status">open this page on the Dell</div>
<script>
const title = document.getElementById("title");
const track = document.getElementById("track");
const viewport = document.getElementById("viewport");
const message = document.getElementById("message");
const status = document.getElementById("status");

let sung = [];
let centers = [];
let builtSig = "";
let showing = false;
let anchor = null;
let viewH = 0;
let hotA = -1;
let hotB = -1;

let soundOn = false;
let heard = null;
let lead = 0;
try { lead = Number(localStorage.getItem("jamstudio-lyric-lead")) || 0; } catch (e) {}

function songTime(now) {
  // lead > 0 pulls the words earlier. The monitor is still a little late,
  // so this is how the page is lined up with what you hear.
  let t = 0;
  if (anchor)
    t = anchor.playing ? anchor.position + (now - anchor.at) / 1000 : anchor.position;
  return Math.max(0, t + lead);
}

function showLead() {
  const range = document.getElementById("syncRange");
  const label = document.getElementById("syncLabel");
  range.value = String(lead);
  if (Math.abs(lead) < 0.025)
    label.textContent = "in sync";
  else if (lead > 0)
    label.textContent = "+" + lead.toFixed(2) + "s early";
  else
    label.textContent = Math.abs(lead).toFixed(2) + "s late";
}

function setLead(next) {
  lead = Math.max(-2, Math.min(2, Math.round(next * 20) / 20));
  try { localStorage.setItem("jamstudio-lyric-lead", String(lead)); } catch (e) {}
  showLead();
}

function noteClock(state) {
  const now = performance.now();
  const serverPos = Number(state.position) || 0;
  const playing = !!state.playing;
  if (!anchor || anchor.playing !== playing) {
    anchor = { position: serverPos, at: now, playing: playing };
    return;
  }
  const predicted = playing ? anchor.position + (now - anchor.at) / 1000 : anchor.position;
  const err = serverPos - predicted;
  // A seek still snaps. A late poll must not, or the line hops backward.
  if (Math.abs(err) > 0.45) {
    anchor = { position: serverPos, at: now, playing: playing };
    return;
  }
  const bleed = Math.max(-0.006, Math.min(0.006, err));
  anchor = { position: predicted + bleed, at: now, playing: playing };
}

// Hold the sung line, then drift into the next one. The drift stays
// shorter than the gap so the handoff cannot skip ahead.
function focusIndex(t) {
  const n = sung.length;
  if (n === 0) return 0;
  let i = 0;
  for (let k = 0; k < n; k++) {
    if (sung[k].start <= t) i = k;
    else break;
  }
  if (i >= n - 1) return i;
  const dur = sung[i + 1].start - sung[i].start;
  if (!(dur > 0)) return i;
  let glide = dur * 0.62;
  if (glide < 0.85) glide = 0.85;
  if (glide > 1.35) glide = 1.35;
  if (glide > dur * 0.92) glide = dur * 0.92;
  const glideStart = sung[i + 1].start - glide;
  if (t <= glideStart) return i;
  const u = (t - glideStart) / glide;
  return i + u * u * (3 - 2 * u);
}

function centerFor(index) {
  const last = centers.length - 1;
  if (last < 0) return 0;
  const i = Math.max(0, Math.min(last, Math.floor(index)));
  const f = Math.max(0, Math.min(1, index - i));
  const a = centers[i];
  const b = centers[Math.min(last, i + 1)];
  return a + (b - a) * f;
}

function fitLines() {
  const limit = track.clientWidth;
  const nodes = track.children;
  for (let i = 0; i < nodes.length; i++) {
    const node = nodes[i];
    node.style.fontSize = "6.4vh";
    const width = node.scrollWidth;
    if (width > limit && limit > 0) {
      const px = parseFloat(getComputedStyle(node).fontSize);
      node.style.fontSize = (px * limit / width) + "px";
    }
  }
}

function measure() {
  fitLines();
  centers = [];
  const nodes = track.children;
  for (let i = 0; i < nodes.length; i++)
    centers.push(nodes[i].offsetTop + nodes[i].offsetHeight / 2);
}

function restyle(node, s) {
  const r = Math.round(154 + (255 - 154) * s);
  const g = Math.round(166 + (211 - 166) * s);
  const b = Math.round(184 + (77 - 184) * s);
  const color = "rgb(" + r + "," + g + "," + b + ")";
  const opacity = (0.34 + 0.66 * s).toFixed(3);
  if (node._c !== color) {
    node._c = color;
    node.style.color = color;
  }
  if (node._o !== opacity) {
    node._o = opacity;
    node.style.opacity = opacity;
  }
}

function emphasis(distance) {
  const u = Math.max(0, 1 - distance);
  return u * u * (3 - 2 * u);
}

function paint(focus) {
  if (!centers.length) return;
  if (!(viewH > 0)) viewH = viewport.clientHeight;
  const shift = Math.round(viewH * 0.5 - centerFor(focus));
  const transform = "translate3d(0," + shift + "px,0)";
  if (track._t !== transform) {
    track._t = transform;
    track.style.transform = transform;
  }
  const nodes = track.children;
  const last = nodes.length - 1;
  if (last < 0) return;
  const a = Math.max(0, Math.min(last, Math.floor(focus)));
  const b = Math.min(last, a + 1);
  if (hotA !== a && hotA !== b && hotA >= 0 && hotA <= last)
    restyle(nodes[hotA], 0);
  if (hotB !== a && hotB !== b && hotB >= 0 && hotB <= last && hotB !== hotA)
    restyle(nodes[hotB], 0);
  restyle(nodes[a], emphasis(Math.abs(a - focus)));
  if (b !== a)
    restyle(nodes[b], emphasis(Math.abs(b - focus)));
  hotA = a;
  hotB = b;
}

function setStatus(state) {
  if (!state.open) {
    status.textContent = "On the HP: Display → Karaoke → Dell server (Ethernet) → Open";
  } else if (state.playing) {
    status.textContent = "";
  } else if (state.position > 0.2) {
    status.textContent = "Paused";
  } else if (state.live) {
    status.textContent = "Ready";
  } else {
    status.textContent = "Waiting for Jam Studio";
  }
}

function show(state) {
  setStatus(state);
  if (!state.open) {
    showing = false;
    anchor = null;
    builtSig = "";
    sung = [];
    centers = [];
    hotA = -1;
    hotB = -1;
    track._t = "";
    track.textContent = "";
    track.style.transform = "";
    title.textContent = "Karaoke";
    message.hidden = false;
    message.textContent = "Display off";
    return;
  }
  title.textContent = state.title || "";
  noteClock(state);
  const lines = state.lines || [];
  let sig = state.title || "";
  for (let i = 0; i < lines.length; i++)
    sig += "\\n" + lines[i].start + "|" + (lines[i].text || "");
  if (sig !== builtSig) {
    builtSig = sig;
    track.textContent = "";
    sung = [];
    for (let i = 0; i < lines.length; i++) {
      const text = (lines[i].text || "").trim();
      if (!text) continue;
      sung.push({ text: text, start: Number(lines[i].start) || 0 });
      const div = document.createElement("div");
      div.className = "line";
      div.textContent = text;
      track.appendChild(div);
    }
    centers = [];
    hotA = -1;
    hotB = -1;
    track._t = "";
    viewH = 0;
  }
  if (!sung.length) {
    showing = false;
    message.hidden = false;
    message.textContent = state.position > 0.2 ? "" : "Ready";
    return;
  }
  showing = true;
  message.hidden = true;
}

function frame(now) {
  if (showing) {
    if (centers.length !== track.children.length) measure();
    paint(focusIndex(songTime(now)));
  }
  requestAnimationFrame(frame);
}

const soundButton = document.getElementById("sound");
const AUDIO_RATE = 48000;
const AUDIO_HEADER = 20;
let audioCtx = null;
let pullAbort = null;
let inbox = new Uint8Array(0);
let nextTime = 0;
let queueSong = null;
let pending = [];
let prebuffer = 0.08;

function stopSources() {
  for (let i = 0; i < pending.length; i++) {
    try { pending[i].src.stop(); } catch (e) {}
  }
  pending = [];
  nextTime = 0;
  queueSong = null;
}

function playPacket(packet) {
  const ctx = audioCtx;
  if (!ctx) return;
  if (soundButton.textContent !== "Sound on")
    setSoundLabel("Sound on", true);
  if (!packet.playing) {
    stopSources();
    heard = { playing: false, position: packet.position, ctx: ctx };
    return;
  }
  const samples = packet.frames;
  const buf = ctx.createBuffer(2, samples, AUDIO_RATE);
  const left = buf.getChannelData(0);
  const right = buf.getChannelData(1);
  const pcm = packet.pcm;
  for (let i = 0; i < samples; i++) {
    left[i] = pcm[i * 2] / 32768;
    right[i] = pcm[i * 2 + 1] / 32768;
  }
  const dur = buf.duration;
  const now = ctx.currentTime;
  pending = pending.filter(function (item) { return item.end > now; });
  let resync = queueSong === null || Math.abs(packet.position - queueSong) > 0.15;
  if (!resync && nextTime < now + 0.02) {
    resync = true;
    prebuffer = Math.min(0.16, prebuffer + 0.02);
  }
  // A burst of old chunks would schedule a bar of delay. Skip to this chunk.
  if (!resync && nextTime > now + 0.12)
    resync = true;
  if (resync) {
    stopSources();
    nextTime = now + prebuffer;
    heard = {
      playing: true,
      ctx: ctx,
      originCtx: nextTime,
      originSong: packet.position
    };
  }
  const src = ctx.createBufferSource();
  src.buffer = buf;
  src.connect(ctx.destination);
  src.start(nextTime);
  pending.push({ src: src, end: nextTime + dur });
  nextTime += dur;
  queueSong = packet.position + dur;
}

function drainInbox() {
  let offset = 0;
  while (inbox.length - offset >= AUDIO_HEADER) {
    const view = new DataView(inbox.buffer, inbox.byteOffset + offset, inbox.length - offset);
    const magicOk = view.getUint8(0) === 0x4a && view.getUint8(1) === 0x53
      && view.getUint8(2) === 0x41 && view.getUint8(3) === 0x31;
    const frames = magicOk ? view.getUint32(16, true) : 0;
    const bytes = AUDIO_HEADER + frames * 4;
    if (!magicOk || frames < 1 || frames > AUDIO_RATE || inbox.length - offset < bytes) {
      if (!magicOk || frames < 1 || frames > AUDIO_RATE) {
        offset += 1;
        continue;
      }
      break;
    }
    const pcm = new Int16Array(frames * 2);
    for (let i = 0; i < pcm.length; i++)
      pcm[i] = view.getInt16(AUDIO_HEADER + i * 2, true);
    playPacket({
      position: view.getFloat64(4, true),
      playing: view.getUint8(12) === 1,
      frames: frames,
      pcm: pcm
    });
    offset += bytes;
  }
  if (offset)
    inbox = inbox.subarray(offset);
}

function takeBytes(chunk) {
  if (!inbox.length) {
    inbox = chunk;
  } else {
    const merged = new Uint8Array(inbox.length + chunk.length);
    merged.set(inbox, 0);
    merged.set(chunk, inbox.length);
    inbox = merged;
  }
  drainInbox();
}

async function pullAudio(ctx) {
  pullAbort = new AbortController();
  const response = await fetch("/audio", { cache: "no-store", signal: pullAbort.signal });
  if (!response.ok || !response.body)
    throw new Error("no audio");
  const reader = response.body.getReader();
  while (soundOn) {
    const piece = await reader.read();
    if (piece.done) break;
    takeBytes(piece.value);
  }
  reader.cancel();
}

function setSoundLabel(text, on) {
  soundButton.textContent = text;
  soundButton.classList.toggle("on", !!on);
}

async function toggleSound() {
  if (soundOn) {
    soundOn = false;
    heard = null;
    if (pullAbort) pullAbort.abort();
    stopSources();
    if (audioCtx) audioCtx.close();
    audioCtx = null;
    inbox = new Uint8Array(0);
    setSoundLabel("Sound", false);
    return;
  }
  soundOn = true;
  setSoundLabel("Sound…", true);
  if (navigator.audioSession)
    navigator.audioSession.type = "playback";
  const ctx = new AudioContext({ sampleRate: AUDIO_RATE });
  audioCtx = ctx;
  try {
    await ctx.resume();
    await pullAudio(ctx);
    if (soundOn) {
      soundOn = false;
      heard = null;
      stopSources();
      setSoundLabel("No sound", false);
    }
  } catch (error) {
    if (soundOn) {
      soundOn = false;
      heard = null;
      stopSources();
      setSoundLabel("No sound", false);
    }
  }
}
soundButton.addEventListener("click", toggleSound);
document.getElementById("earlier").addEventListener("click", function () { setLead(lead + 0.1); });
document.getElementById("later").addEventListener("click", function () { setLead(lead - 0.1); });
document.getElementById("syncRange").addEventListener("input", function (event) {
  setLead(Number(event.target.value));
});
showLead();

let pollBusy = false;
async function poll() {
  if (pollBusy) return;
  pollBusy = true;
  try {
    const response = await fetch("/state", { cache: "no-store" });
    show(await response.json());
  } catch (error) {
    status.textContent = "Lost the HP";
  } finally {
    pollBusy = false;
  }
}
poll();
setInterval(poll, 100);
requestAnimationFrame(frame);
window.addEventListener("resize", function () { centers = []; viewH = 0; });
if (document.fonts && document.fonts.ready)
  document.fonts.ready.then(function () { centers = []; viewH = 0; });
</script>
</body>
</html>
"""


# Last good sample. Jam Studio deletes the playhead file before renaming
# the new one into place, so one read can miss. Reporting that miss as
# position 0 sent the page back to the first line.
_hold = None


def read_playhead():
    """Song position published by Jam Studio, extrapolated while it plays."""
    global _hold
    for attempt in range(4):
        try:
            playing_text, position_text, sampled_text = open(
                PLAYHEAD_PATH, encoding="utf-8"
            ).read().split()
            sampled = float(sampled_text)
            age = time.time() - sampled
            if -0.05 <= age < 2.0:
                _hold = (float(position_text), sampled, playing_text == "1")
                break
        except (OSError, ValueError):
            if attempt < 3:
                time.sleep(0.001)
    hold = _hold
    if hold is None:
        return 0.0, False, False
    position, sampled, playing = hold
    age = time.time() - sampled
    if age >= 2.0:
        return 0.0, False, False
    if playing and age > 0.0:
        position += age
    return max(0.0, position), playing, True


def pack_packet(position, playing, live, pcm):
    """One 20 ms Mon A chunk stamped with the song position of its first sample."""
    frames = len(pcm) // 4
    header = AUDIO_HEADER.pack(
        b"JSA1",
        float(position),
        1 if playing else 0,
        1 if live else 0,
        0,
        frames,
    )
    return header + pcm


def ensure_capture():
    global capture_thread
    with capture_lock:
        if capture_thread and capture_thread.is_alive():
            return
        capture_thread = threading.Thread(target=capture_loop, name="mona-audio", daemon=True)
        capture_thread.start()


def capture_loop():
    """Record Jam Studio's Mon A bus while a lyric screen is listening.

    pw-record exists on the HP. Other platforms keep the lyric page up and
    skip this stream.
    """
    if shutil.which("pw-record") is None:
        return

    env = os.environ.copy()
    env.setdefault("XDG_RUNTIME_DIR", f"/run/user/{os.getuid()}")
    command = [
        "pw-record",
        "--target", MONITOR_SOURCE,
        "--rate", str(AUDIO_RATE),
        "--channels", "2",
        "--format", "s16",
        "--raw",
        "--latency", "20ms",
        "-",
    ]
    while True:
        with subscribers_lock:
            if not subscribers:
                return
        try:
            proc = subprocess.Popen(
                command,
                stdout=subprocess.PIPE,
                stderr=subprocess.DEVNULL,
                env=env,
            )
        except OSError:
            return
        try:
            while True:
                with subscribers_lock:
                    if not subscribers:
                        return
                pcm = proc.stdout.read(AUDIO_BYTES)
                if len(pcm) < AUDIO_BYTES:
                    break
                position, playing, live = read_playhead()
                if playing:
                    position = max(0.0, position - (AUDIO_FRAMES / AUDIO_RATE))
                packet = pack_packet(position, playing, live, pcm)
                with subscribers_lock:
                    targets = list(subscribers)
                for listener in targets:
                    with listener.cv:
                        listener.queue.append(packet)
                        listener.cv.notify()
        finally:
            proc.kill()
            proc.wait()
        time.sleep(0.4)


class Listener:
    def __init__(self):
        self.queue = deque(maxlen=4)
        self.cv = threading.Condition()
        self.alive = True


def current_state():
    """Song position published by Jam Studio, extrapolated while it plays."""
    position, playing, live = read_playhead()
    title, lines = load_lyrics()
    return {
        "title": title,
        "position": round(position, 3),
        "playing": playing,
        "live": live,
        # The Dell is the lyric screen. Show lines whenever Jam Studio is
        # running. Display → Karaoke → Close can still blank it.
        "open": os.path.isfile(OPEN_FLAG) or (live and not os.path.isfile(CLOSED_FLAG)),
        "audacity": live,
        "lines": lines,
        **line_at(position, lines),
    }


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        path = self.path.split("?", 1)[0]
        if path == "/audio":
            self.stream_audio()
            return
        if path == "/state":
            body = json.dumps(current_state()).encode()
            mime = "application/json"
        elif path in ("/", "/lyrics"):
            body = PAGE.encode()
            mime = "text/html; charset=utf-8"
        else:
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header("Content-Type", mime)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def stream_audio(self):
        listener = Listener()
        with subscribers_lock:
            subscribers.append(listener)
        ensure_capture()
        try:
            self.protocol_version = "HTTP/1.1"
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Cache-Control", "no-store")
            self.send_header("Transfer-Encoding", "chunked")
            self.send_header("Connection", "close")
            self.end_headers()
            while listener.alive:
                with listener.cv:
                    if not listener.queue:
                        listener.cv.wait(timeout=1.0)
                    packet = listener.queue.popleft() if listener.queue else None
                if packet is None:
                    continue
                frame = f"{len(packet):X}\r\n".encode() + packet + b"\r\n"
                self.wfile.write(frame)
                self.wfile.flush()
        except (BrokenPipeError, ConnectionResetError, TimeoutError, OSError):
            pass
        finally:
            listener.alive = False
            with subscribers_lock:
                if listener in subscribers:
                    subscribers.remove(listener)

    def log_message(self, fmt, *args):
        return


def self_test():
    early = line_at(0, PROJECT_LINES)
    assert early["current"] is None
    assert early["next"]["text"].startswith("London calling to the faraway")
    first = line_at(21.6, PROJECT_LINES)
    assert first["current"]["text"].startswith("London calling to the faraway")
    second = line_at(24.0, PROJECT_LINES)
    assert second["current"]["text"].startswith("Now war is declared")
    assert second["previous"]["text"].startswith("London calling to the faraway")
    end = line_at(192, PROJECT_LINES)
    assert end["current"]["text"].startswith("I never felt")
    packet = pack_packet(21.54, True, True, b"\x00\x01" * (AUDIO_FRAMES * 2))
    magic, position, playing, live, _pad, frames = AUDIO_HEADER.unpack(packet[: AUDIO_HEADER.size])
    assert magic == b"JSA1"
    assert abs(position - 21.54) < 1e-6
    assert playing == 1 and live == 1 and frames == AUDIO_FRAMES
    assert len(packet) == AUDIO_HEADER.size + AUDIO_BYTES
    print("lyric timing ok", len(PROJECT_LINES), "lines", flush=True)


def main():
    self_test()
    server = ThreadingHTTPServer((HOST, PORT), Handler)
    print(f"lyrics on http://10.42.0.1:{PORT}", flush=True)
    server.serve_forever()


if __name__ == "__main__":
    main()
