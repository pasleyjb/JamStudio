#!/usr/bin/env python3
"""Copy one Jam Studio mix bus to a hardware sink on that sink's own clock.

PipeWire module-loopback and pw-link put the headphones or the dock on the
mix clock. The late copy comes back as buzz. This process only moves bytes.
"""

import argparse
import ctypes
import ctypes.util
import fcntl
import os
import signal
import subprocess
import sys
import threading

RATE = 48000
CH = 2
SAMPLE = 4
FRAME = CH * SAMPLE
# 256 samples is 5.3 ms. One block in hand is the whole software cushion.
FRAMES = 256
CHUNK = FRAMES * FRAME
PREBUFFER = 1
MAX_QUEUE = 2


def arm_parent_death():
    libc_name = ctypes.util.find_library("c")
    if not libc_name:
        return
    libc = ctypes.CDLL(libc_name, use_errno=True)
    # PR_SET_PDEATHSIG = 1. Jam Studio is the parent; if it dies, stop the copy
    # so the headphone jack and the dock are not held open on silence.
    libc.prctl(1, signal.SIGTERM)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True)
    parser.add_argument("--sink", required=True)
    parser.add_argument("--tag", required=True)
    args = parser.parse_args()

    os.environ["XDG_RUNTIME_DIR"] = f"/run/user/{os.getuid()}"
    arm_parent_death()
    if os.getppid() == 1:
        return

    log_path = f"/tmp/jamstudio-bridge-{args.tag}.log"
    log = open(log_path, "a", buffering=1)
    pid_path = f"/tmp/jamstudio-bridge-{args.tag}.pid"
    open(pid_path, "w").write(str(os.getpid()))

    children = []

    def stop(*_):
        for proc in children:
            if proc.poll() is None:
                proc.kill()
        try:
            os.remove(pid_path)
        except OSError:
            pass
        os._exit(0)

    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)

    cap_name = f"jamstudio-copy-{args.tag}-cap"
    play_name = f"jamstudio-copy-{args.tag}-play"
    # Capture follows the mix clock. Playback follows the sink clock.
    # Do not mark the capture passive, and do not put the two nodes in one group.
    cap = subprocess.Popen(
        ["pw-cat", "--record", "--raw", "--target", args.source,
         "--format", "f32", "--rate", str(RATE), "--channels", str(CH),
         "--latency", str(FRAMES),
         "-P", f'{{ "node.name": "{cap_name}", "node.latency": "{FRAMES}/{RATE}" }}',
         "-"],
        stdout=subprocess.PIPE, stderr=log)
    play = subprocess.Popen(
        ["pw-cat", "--playback", "--raw", "--target", args.sink,
         "--format", "f32", "--rate", str(RATE), "--channels", str(CH),
         "--latency", str(FRAMES),
         "-P", f'{{ "node.name": "{play_name}", "node.latency": "{FRAMES}/{RATE}" }}',
         "-"],
        stdin=subprocess.PIPE, stderr=log)
    children.extend((cap, play))
    # The default pipe holds about 170 ms. One page is the smallest Linux will keep.
    try:
        fcntl.fcntl(cap.stdout.fileno(), 1031, 4096)   # F_SETPIPE_SZ
        fcntl.fcntl(play.stdin.fileno(), 1031, 4096)
    except OSError as exc:
        log.write(f"pipe size left at default: {exc}\n")
    log.write(f"bridge {args.tag} pid {os.getpid()} {args.source} -> {args.sink}\n")

    queue = []
    cond = threading.Condition()

    def reader():
        while True:
            block = cap.stdout.read(CHUNK)
            with cond:
                if not block:
                    cond.notify_all()
                    break
                queue.append(block)
                if len(queue) > MAX_QUEUE:
                    del queue[:len(queue) - PREBUFFER]
                cond.notify()

    threading.Thread(target=reader, daemon=True).start()
    with cond:
        if len(queue) < PREBUFFER:
            cond.wait(timeout=2.0)

    silence = b"\x00" * CHUNK
    try:
        while play.poll() is None and cap.poll() is None:
            with cond:
                if len(queue) > MAX_QUEUE:
                    del queue[:len(queue) - PREBUFFER]
                if not queue:
                    cond.wait(timeout=0.008)
                block = queue.pop(0) if queue else None
            if block is None:
                block = silence
            play.stdin.write(block)
            play.stdin.flush()
    except (BrokenPipeError, OSError):
        pass
    stop()


if __name__ == "__main__":
    main()
