"""Download and unpack the pinned toolchain into ./toolchain (Windows x64).

Usage:  py -3 tools/setup_toolchain.py [--force]

Everything lands under toolchain/<name>/ with the archive's top-level folder
stripped, so paths stay short. Archives are cached in toolchain/_downloads/
and checked against pinned SHA-256 hashes. Re-running skips tools that are
already unpacked unless --force is given.
"""

import hashlib
import shutil
import sys
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOOLCHAIN = ROOT / "toolchain"
DOWNLOADS = TOOLCHAIN / "_downloads"

TOOLS = [
    {
        "name": "dasm",
        "version": "2.20.17",
        "url": "https://github.com/dasm-assembler/dasm/releases/download/v2.20.17/dasm-v2.20.17-windows.zip",
        "sha256": "7bb4b51816ca19a6062c2b54c3210fbb4edcb0a4bde42815341a802ea729049a",
    },
    {
        "name": "arm",
        "version": "xPack GCC 15.2.1-1.1",
        "url": "https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/download/v15.2.1-1.1/xpack-arm-none-eabi-gcc-15.2.1-1.1-win32-x64.zip",
        "sha256": "bae6a3d1667697ce750c3b13d6d26d80973ecedc2cc87bf04869e83447fd93ea",
    },
    {
        "name": "make",
        "version": "xPack windows-build-tools 4.4.1-3",
        "url": "https://github.com/xpack-dev-tools/windows-build-tools-xpack/releases/download/v4.4.1-3/xpack-windows-build-tools-4.4.1-3-win32-x64.zip",
        "sha256": "113d4dfdbbc56dc9b865c9f75d38cd0da82f0d7094187e6f7a803fe6eef1d218",
    },
    {
        "name": "stella",
        "version": "7.0c",
        "url": "https://github.com/stella-emu/stella/releases/download/7.0/Stella-7.0c-windows.zip",
        # The release publishes no checksum; this is the hash of the first download.
        "sha256": "44102ccf8af74b021cf143db346e561c5d6ed2a2e91d79ee6d3c99d7c42f4653",
    },
]


def sha256_of(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def download(url: str, dest: Path) -> None:
    tmp = dest.with_suffix(dest.suffix + ".part")
    with urllib.request.urlopen(url) as resp, tmp.open("wb") as out:
        total = int(resp.headers.get("Content-Length") or 0)
        done = 0
        last_pct = -10
        while chunk := resp.read(1 << 20):
            out.write(chunk)
            done += len(chunk)
            if total:
                pct = done * 100 // total
                if pct >= last_pct + 10:
                    print(f"    {pct:3d}%  ({done >> 20} / {total >> 20} MB)", flush=True)
                    last_pct = pct
    tmp.replace(dest)


def common_prefix(names: list[str]) -> str:
    """Return the single top-level folder shared by every entry, or ''."""
    tops = {n.split("/", 1)[0] for n in names if n.strip("/")}
    if len(tops) == 1 and all("/" in n for n in names if n.strip("/")):
        return tops.pop() + "/"
    return ""


def extract(archive: Path, dest: Path) -> int:
    with zipfile.ZipFile(archive) as z:
        infos = z.infolist()
        prefix = common_prefix([i.filename for i in infos])
        count = 0
        for info in infos:
            rel = info.filename[len(prefix):]
            if not rel:
                continue
            target = (dest / rel).resolve()
            if dest.resolve() not in target.parents and target != dest.resolve():
                raise RuntimeError(f"unsafe path in archive: {info.filename}")
            if info.is_dir():
                target.mkdir(parents=True, exist_ok=True)
                continue
            target.parent.mkdir(parents=True, exist_ok=True)
            with z.open(info) as src, target.open("wb") as out:
                shutil.copyfileobj(src, out)
            count += 1
        expected = sum(1 for i in infos if not i.is_dir() and i.filename[len(prefix):])
    if count != expected:
        raise RuntimeError(f"extracted {count} files, archive has {expected}")
    return count


def main() -> int:
    force = "--force" in sys.argv
    DOWNLOADS.mkdir(parents=True, exist_ok=True)
    for tool in TOOLS:
        name = tool["name"]
        dest = TOOLCHAIN / name
        marker = dest / ".installed"
        print(f"[{name}] {tool['version']}")
        if marker.exists() and not force:
            print("    already installed")
            continue

        archive = DOWNLOADS / tool["url"].rsplit("/", 1)[1]
        if not archive.exists():
            print(f"    downloading {tool['url']}")
            download(tool["url"], archive)

        digest = sha256_of(archive)
        if tool["sha256"] and digest != tool["sha256"]:
            archive.unlink()
            print(f"    CHECKSUM MISMATCH (got {digest}); deleted download")
            return 1
        print(f"    sha256 {digest}" + ("  ok" if tool["sha256"] else "  (not pinned)"))

        if dest.exists():
            shutil.rmtree(dest)
        dest.mkdir(parents=True)
        n = extract(archive, dest)
        marker.write_text(f"{tool['version']}\n{digest}\n")
        print(f"    unpacked {n} files to {dest.relative_to(ROOT)}")
    print("done")
    return 0


if __name__ == "__main__":
    sys.exit(main())
