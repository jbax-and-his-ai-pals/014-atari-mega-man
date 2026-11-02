#!/usr/bin/env python3
"""Run a ROM in Stella for N frames, save a PNG of the emulated TIA image, exit.

Usage: python tools/snap.py build/game.bin --frames 120 --out build/shot.png

Mechanism: Stella starts in its debugger (-debug 1) and runs
<basedir>/autoexec.script, which this tool writes (and removes afterwards):
    frame N / saveSnap / exitRom
The snapshot goes to a private temp dir (-snapsavedir) and is copied to --out.
Only the Stella process started here is ever killed.

Limitations: the PNG is the plain TIA image (320 px wide, ~228 tall); the
-dev.stats overlay is NOT drawn into it, and the debugger's scanline count
cannot be exported to a file/stdout in Stella 7.0c, so none is reported.
"""
import argparse, os, shutil, struct, subprocess, sys, tempfile, time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("rom")
    ap.add_argument("--frames", type=int, default=120)
    ap.add_argument("--out", default=os.path.join(ROOT, "build", "shot.png"))
    ap.add_argument("--stella", default=os.path.join(ROOT, "toolchain", "stella", "Stella.exe"))
    ap.add_argument("--basedir", default=os.path.join(ROOT, "toolchain", "stella-config"))
    ap.add_argument("--timeout", type=float, default=30.0)
    a = ap.parse_args()

    rom = os.path.abspath(a.rom)
    out = os.path.abspath(a.out)
    if not os.path.isfile(rom):
        sys.exit("snap: ROM not found: " + rom)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    os.makedirs(a.basedir, exist_ok=True)

    script = os.path.join(a.basedir, "autoexec.script")
    backup = None
    if os.path.exists(script):
        backup = open(script, "rb").read()
    tmp = tempfile.mkdtemp(prefix="stellasnap_")
    proc = None
    try:
        with open(script, "w") as f:
            f.write("frame %d\nsaveSnap\nexitRom\n" % max(1, a.frames))
        cmd = [a.stella, "-basedir", a.basedir, "-debug", "1",
               "-dev.settings", "1", "-dev.thumb.trapfatal", "1",
               "-snapsavedir", tmp, rom]
        proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        deadline = time.time() + a.timeout
        png = None
        while time.time() < deadline:
            pngs = [p for p in os.listdir(tmp) if p.lower().endswith(".png")]
            if pngs and proc.poll() is not None or (pngs and time.time() > deadline - 1e9 and _settled(tmp, pngs)):
                png = os.path.join(tmp, pngs[0])
                break
            if proc.poll() is not None:
                break
            time.sleep(0.1)
        if png is None:
            sys.exit("snap: no screenshot produced (rc=%s)" % proc.poll())
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            pass
        shutil.copyfile(png, out)
    finally:
        if proc is not None and proc.poll() is None:
            proc.kill()
            proc.wait()
        if backup is not None:
            open(script, "wb").write(backup)
        elif os.path.exists(script):
            os.remove(script)
        shutil.rmtree(tmp, ignore_errors=True)

    d = open(out, "rb").read(24)
    w, h = struct.unpack(">II", d[16:24])
    print("%s  (%dx%d, after %d frames; scanline count unavailable)" % (out, w, h, a.frames))


def _settled(tmp, pngs):
    p = os.path.join(tmp, pngs[0])
    try:
        s1 = os.path.getsize(p)
        time.sleep(0.3)
        return s1 > 0 and s1 == os.path.getsize(p)
    except OSError:
        return False


if __name__ == "__main__":
    main()
