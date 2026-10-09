#!/usr/bin/env python3
"""Serve the karaoke lyric screen for the Dell on the studio ethernet link.

Jam Studio's Display → Karaoke → Dell server (Ethernet) → Open writes
/tmp/jamstudio-karaoke.open and /tmp/jamstudio-lyrics.json. The playhead
file is /tmp/jamstudio-playhead. The Dell opens http://10.42.0.1:8765.
"""

import json
import os
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

PROJECT = "/home/jaypee/Documents/JamStudio/Projects/London calling.jamstudio"
LYRICS_JSON = "/tmp/jamstudio-lyrics.json"
OPEN_FLAG = "/tmp/jamstudio-karaoke.open"
HOST = "0.0.0.0"
PORT = 8765


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
    with open(PROJECT, encoding="utf-8") as handle:
        lyrics = json.load(handle)["embeddedLyrics"]
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
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Karaoke</title>
<style>
  html, body { margin: 0; height: 100%; background: #10141c; color: #eef2f7;
    font-family: "Liberation Sans", "Nimbus Sans", sans-serif; }
  body { display: flex; flex-direction: column; justify-content: center;
    align-items: center; text-align: center; padding: 4vh 6vw; box-sizing: border-box; }
  #prev, #next { font-size: 4.2vh; line-height: 1.25; color: #9aa6b8;
    min-height: 1.3em; max-width: 90vw; }
  #current { font-size: 8.5vh; line-height: 1.15; font-weight: 700;
    color: #ffd34d; margin: 3vh 0; max-width: 92vw; }
  #status { position: fixed; left: 0; right: 0; bottom: 2vh; font-size: 2.2vh;
    color: #5a6575; letter-spacing: 0.04em; }
  #title { position: fixed; top: 2.5vh; font-size: 2.4vh; color: #9aa6b8; }
</style>
</head>
<body>
<div id="title"></div>
<div id="prev"></div>
<div id="current"></div>
<div id="next"></div>
<div id="status">open this page on the Dell</div>
<script>
const title = document.getElementById("title");
const prev = document.getElementById("prev");
const current = document.getElementById("current");
const next = document.getElementById("next");
const status = document.getElementById("status");

function show(state) {
  if (!state.open) {
    title.textContent = "Karaoke";
    prev.textContent = "";
    current.textContent = "Display off";
    next.textContent = "";
    status.textContent = "On the HP: Display → Karaoke → Dell server (Ethernet) → Open";
    return;
  }
  title.textContent = state.title || "";
  prev.textContent = state.previous ? state.previous.text : "";
  if (state.current) {
    current.textContent = state.current.text;
    next.textContent = state.next ? state.next.text : "";
  } else if (state.next) {
    current.textContent = state.next.text;
    next.textContent = "";
  } else if (!state.playing && state.position < 0.05) {
    current.textContent = "Ready";
    next.textContent = "";
  } else {
    current.textContent = "";
    next.textContent = "";
  }
  if (state.playing) {
    status.textContent = "";
  } else if (state.position > 0.2) {
    status.textContent = "Paused";
  } else if (state.live) {
    status.textContent = "Ready";
  } else {
    status.textContent = "Waiting for Jam Studio";
  }
}

async function poll() {
  try {
    const response = await fetch("/state", { cache: "no-store" });
    show(await response.json());
  } catch (error) {
    status.textContent = "Lost the HP";
  }
}
poll();
setInterval(poll, 100);
</script>
</body>
</html>
"""


def current_state():
    """Song position published by Jam Studio, extrapolated while it plays."""
    playing = False
    position = 0.0
    live = False
    title, lines = load_lyrics()

    try:
        playing_text, position_text, sampled_text = open(
            "/tmp/jamstudio-playhead", encoding="utf-8"
        ).read().split()
        sampled = float(sampled_text)
        age = time.time() - sampled
        if 0.0 <= age < 2.0:
            live = True
            playing = playing_text == "1"
            position = float(position_text)
            if playing:
                position += age
    except (OSError, ValueError):
        pass

    position = max(0.0, position)
    return {
        "title": title,
        "position": round(position, 3),
        "playing": playing,
        "live": live,
        # The Dell is the lyric screen. Show lines whenever Jam Studio is
        # running. Display → Karaoke → Close can still blank it.
        "open": os.path.isfile(OPEN_FLAG) or (live and not os.path.isfile("/tmp/jamstudio-karaoke.closed")),
        "audacity": live,
        **line_at(position, lines),
    }


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        path = self.path.split("?", 1)[0]
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
    print("lyric timing ok", len(PROJECT_LINES), "lines", flush=True)


def main():
    self_test()
    server = ThreadingHTTPServer((HOST, PORT), Handler)
    print(f"lyrics on http://10.42.0.1:{PORT}", flush=True)
    server.serve_forever()


if __name__ == "__main__":
    main()
