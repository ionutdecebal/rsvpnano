#!/usr/bin/env python3

import argparse
import ctypes
import datetime as dt
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import threading
import time
from pathlib import Path

from compare import parse_log, parse_wpm_sweep, write_summary, write_reader_result


DEFAULT_ENV = "benchmark_waveshare_esp32s3_touch_lcd_349_rev1"
DEFAULT_EPUB_FIXTURE = "companion/testdata/conversion/Dracula-epub.epub"
DEFAULT_MULTILINGUAL_FIXTURE = "companion/testdata/multilingual/multilingual.rsvp"
DEFAULT_VERTICAL_EPUB_FIXTURE = "companion/testdata/multilingual/vertical-cjk.epub"
DEVICE_EPUB_PATH = Path("benchmark/Dracula-epub.epub")
DEVICE_MULTILINGUAL_PATH = Path("benchmark/multilingual.rsvp")
DEVICE_VERTICAL_EPUB_PATH = Path("benchmark/vertical-cjk.epub")
DEVICE_MARKER_PATH = Path("benchmark/.rsvpnano-benchmark")
RUN_READY_PATH = Path("benchmark/.run-ready")
FONT_MANIFEST_PATH = Path("benchmark/fonts.sha256")
RFONT4_FILENAME = "font.rfont4"
ESPRESSIF_USB_ID = "VID:PID=303A:1001"
COPY_BUFFER_SIZE = 1024 * 1024
DEVICE_READER_FIXTURE = Path("benchmark/reader-1000.rsvp")


def platformio_command() -> list[str]:
    # Reuse the installed toolchain; do not require uvx or another download.
    executable = shutil.which("pio") or shutil.which("platformio")
    if executable:
        return [executable]
    home = Path(os.environ.get("PLATFORMIO_CORE_DIR", Path.home() / ".platformio"))
    executable = home / "penv" / ("Scripts/platformio.exe" if os.name == "nt" else "bin/pio")
    if executable.is_file():
        return [str(executable)]
    if shutil.which("uvx"):
        return ["uvx", "platformio"]
    raise RuntimeError("PlatformIO is required: install it or put pio on PATH")


def child_environment() -> dict[str, str]:
    return {**os.environ, "PYTHONIOENCODING": "utf-8", "PYTHONUTF8": "1"}


def run_checked(args: list[str], cwd: Path) -> None:
    print(f"[bench-script] {' '.join(args)}", flush=True)
    subprocess.run(args, cwd=cwd, check=True, env=child_environment())


def platformio_devices(cwd: Path) -> list[dict[str, str]]:
    result = subprocess.run(
        [*platformio_command(), "device", "list", "--json-output"],
        cwd=cwd,
        check=True,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        env=child_environment(),
    )
    return json.loads(result.stdout)


def device_serial(device: dict[str, str]) -> str:
    match = re.search(r"\bSER=([^\s]+)", device.get("hwid", ""))
    return re.sub(r"[^0-9A-Fa-f]", "", match.group(1)).casefold() if match else ""


def platformio_python(cwd: Path) -> str:
    result = subprocess.run(
        [*platformio_command(), "system", "info", "--json-output"],
        cwd=cwd,
        check=True,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        env=child_environment(),
    )
    return json.loads(result.stdout)["python_exe"]["value"]


def monitor_command(python: str, environment: str, port: str, baud: str) -> list[str]:
    return [
        python, "-m", "platformio", "device", "monitor", "-e", environment, "-p", port, "-b", baud,
        "--dtr", "0", "--rts", "0", "-f", "default", "-f", "time", "-f", "esp32_exception_decoder",
    ]


def reader_monitor_command(python: str, port: str, baud: str) -> list[str]:
    # Open once, then request the test on that same connection. This prevents
    # missing results while USB re-enumerates after the fixture-copy reboot.
    script = """import serial, sys, time
p = serial.Serial()
p.port, p.baudrate, p.timeout = sys.argv[1], int(sys.argv[2]), .2
p.write_timeout = 1
p.dtr = p.rts = False
p.open()
started, last_request, recent = False, 0, ''
try:
    while True:
        if not started and time.monotonic() - last_request >= 1:
            p.write(b'reader-start\\n'); p.flush(); last_request = time.monotonic()
        line = p.readline().decode('utf-8', errors='replace')
        if line:
            print(line, end='', flush=True)
            recent = (recent + line)[-512:]
            if '[bench] start ' in recent: started = True
            if '[bench] done' in recent: break
finally:
    p.close()
"""
    return [python, "-u", "-c", script, port, baud]


def enter_bootloader(cwd: Path, port: str) -> None:
    # TinyUSB CDC treats a 1200-baud open/close as a request to enter the ROM bootloader.
    script = (
        "import serial,sys,time\n"
        "try:\n"
        " p=serial.Serial(sys.argv[1],1200);time.sleep(.2);p.close()\n"
        "except serial.SerialException:\n"
        " pass\n"
    )
    run_checked([platformio_python(cwd), "-c", script, port], cwd)


def select_serial_port(devices: list[dict[str, str]], requested: str = "", serial: str = "") -> str:
    if serial:
        matches = [device["port"] for device in devices if device_serial(device) == serial]
        if len(matches) == 1:
            return matches[0]
    if requested and any(device.get("port", "").casefold() == requested.casefold() for device in devices):
        return requested
    if requested and not serial:
        return ""
    matches = [device["port"] for device in devices if ESPRESSIF_USB_ID in device.get("hwid", "")]
    if len(matches) == 1:
        return matches[0]
    if not requested and len(devices) == 1:
        return devices[0]["port"]
    return ""


def wait_for_serial_port(cwd: Path, requested: str, serial: str, timeout: float) -> str:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            port = select_serial_port(platformio_devices(cwd), requested, serial)
            if port:
                return port
        except (json.JSONDecodeError, subprocess.CalledProcessError):
            pass
        time.sleep(0.5)
    raise TimeoutError("benchmark serial port did not appear")


def mounted_roots() -> list[Path]:
    if os.name == "nt":
        mask = ctypes.windll.kernel32.GetLogicalDrives()
        return [Path(f"{chr(ord('A') + index)}:\\") for index in range(26) if mask & (1 << index)]

    roots: list[Path] = []
    for parent in (Path("/Volumes"), Path("/media"), Path("/run/media")):
        if not parent.is_dir():
            continue
        # Only inspect mount roots, not every book and font on every disk.
        roots.extend(path for pattern in ("*", "*/*") for path in parent.glob(pattern) if path.is_dir())
    return roots


def find_benchmark_volume(roots: list[Path]) -> Path | None:
    matches = [root for root in roots if (root / DEVICE_MARKER_PATH).is_file()]
    if len(matches) > 1:
        raise RuntimeError(f"multiple RSVP Nano benchmark volumes found: {', '.join(map(str, matches))}")
    return matches[0] if matches else None


def file_sha256(path: Path) -> str:
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def prepare_reader_volume(root: Path) -> None:
    if not (root / DEVICE_MARKER_PATH).is_file():
        raise RuntimeError("Not an RSVP benchmark volume")
    vocabulary = ("a", "the", "river", "quietly", "reading", "window", "beautiful", "understand", "wonderful", "light", "remember", "chapter")
    words = [f"{vocabulary[i % len(vocabulary)]}{i + 1:04d}" for i in range(1000)]
    content = "@rsvp 1\n@title USB reader speed test\n@language en\n@direction ltr\n@chapter USB reader test\n\n"
    content += "\n".join(" ".join(words[i:i + 20]) for i in range(0, 1000, 20)) + "\n"
    for path, text in ((root / DEVICE_READER_FIXTURE, content), (root / RUN_READY_PATH, "reader-only\n")):
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("w", encoding="utf-8") as output:
            output.write(text)
            output.flush()
            os.fsync(output.fileno())
    print("[bench-script] 1,000-word reader fixture ready; existing books and fonts untouched", flush=True)


def catalog_fonts(fonts_root: Path) -> list[tuple[Path, Path]]:
    catalog = json.loads((fonts_root / "index.json").read_text(encoding="utf-8"))
    return [
        (fonts_root / entry["file"], Path(entry["id"]) / RFONT4_FILENAME)
        for entry in catalog
    ]


def wait_for_benchmark_volume(sd_root: str, timeout: float) -> Path:
    explicit = Path(sd_root).resolve() if sd_root else None
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        roots = [explicit] if explicit else mounted_roots()
        found = find_benchmark_volume([root for root in roots if root is not None])
        if found:
            return found
        time.sleep(0.5)
    raise TimeoutError("benchmark storage volume did not appear")


def prepare_volume(
    root: Path,
    epub_fixture: Path,
    multilingual_fixture: Path,
    vertical_epub_fixture: Path | None = None,
    fonts_root: Path | None = None,
) -> Path:
    if fonts_root is not None:
        sources = catalog_fonts(fonts_root)
        source_hashes = [(source, target, file_sha256(source)) for source, target in sources]
        manifest = "".join(
            f"{target.as_posix()}\t{digest}\n"
            for source, target, digest in source_hashes
        )
        manifest_path = root / FONT_MANIFEST_PATH
        installed_manifest = manifest_path.read_text(encoding="ascii") if manifest_path.is_file() else ""
        if manifest != installed_manifest:
            copied = 0
            for index, (source, target, digest) in enumerate(source_hashes, start=1):
                target_font = root / "fonts" / target
                unchanged = (
                    target_font.is_file()
                    and target_font.stat().st_size == source.stat().st_size
                    and file_sha256(target_font) == digest
                )
                if unchanged:
                    continue
                size_mib = source.stat().st_size / (1024 * 1024)
                print(
                    f"[bench-script] syncing font {index}/{len(sources)} "
                    f"{source.parent.name} ({size_mib:.1f} MiB)",
                    flush=True,
                )
                target_font.parent.mkdir(parents=True, exist_ok=True)
                with source.open("rb") as source_file, target_font.open("wb") as output:
                    shutil.copyfileobj(source_file, output, COPY_BUFFER_SIZE)
                    output.flush()
                    os.fsync(output.fileno())
                copied += 1
                published_path = root / "fonts" / source.relative_to(fonts_root)
                if published_path != target_font and published_path.is_file():
                    published_path.unlink()
                    published_path.parent.rmdir()
            manifest_path.parent.mkdir(parents=True, exist_ok=True)
            with manifest_path.open("w", encoding="ascii") as output:
                output.write(manifest)
                output.flush()
                os.fsync(output.fileno())
            print(f"[bench-script] synced {copied}/{len(sources)} fonts to device", flush=True)

    fixtures = [(epub_fixture, DEVICE_EPUB_PATH), (multilingual_fixture, DEVICE_MULTILINGUAL_PATH)]
    if vertical_epub_fixture is not None:
        fixtures.append((vertical_epub_fixture, DEVICE_VERTICAL_EPUB_PATH))
    for source_path, device_path in fixtures:
        target = root / device_path
        target.parent.mkdir(parents=True, exist_ok=True)
        unchanged = (
            target.is_file()
            and target.stat().st_size == source_path.stat().st_size
            and file_sha256(target) == file_sha256(source_path)
        )
        if unchanged:
            continue
        with source_path.open("rb") as source, target.open("wb") as output:
            shutil.copyfileobj(source, output, COPY_BUFFER_SIZE)
            output.flush()
            os.fsync(output.fileno())
    with (root / RUN_READY_PATH).open("w", encoding="ascii") as ready:
        ready.write("ready\n")
        ready.flush()
        os.fsync(ready.fileno())
    print("[bench-script] benchmark fixtures ready", flush=True)
    return root / DEVICE_EPUB_PATH


def release_benchmark_storage(cwd: Path, port: str) -> None:
    script = (
        "import serial,sys,time\n"
        "p=serial.Serial();p.port=sys.argv[1];p.baudrate=115200;p.dtr=False;p.rts=False;p.open()\n"
        "p.write(b'run\\n');p.flush();time.sleep(.2);p.close()\n"
    )
    run_checked([platformio_python(cwd), "-c", script, port], cwd)


def wait_for_eject(root: Path, timeout: float) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if not root.exists():
            return
        time.sleep(0.25)
    raise TimeoutError(f"benchmark storage volume did not eject: {root}")


def monitor_to_log(args: list[str], cwd: Path, log_path: Path, timeout: float) -> None:
    print(f"[bench-script] monitoring log={log_path}", flush=True)
    timed_out = threading.Event()
    with log_path.open("w", encoding="utf-8", errors="replace") as log_file:
        process = subprocess.Popen(
            args,
            cwd=cwd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
            env=child_environment(),
        )
        assert process.stdout is not None

        def stop_on_timeout() -> None:
            timed_out.set()
            process.terminate()

        timer = threading.Timer(timeout, stop_on_timeout)
        timer.start()
        done = False
        try:
            for line in process.stdout:
                print(line, end="")
                log_file.write(line)
                log_file.flush()
                if "[bench] done" in line:
                    done = True
                    break
        finally:
            timer.cancel()
            if process.poll() is None:
                process.terminate()
            process.wait(timeout=5)

    if timed_out.is_set():
        raise TimeoutError(f"benchmark did not finish within {timeout:g} seconds")
    if not done:
        raise RuntimeError("serial monitor ended before the benchmark completion marker")


def write_results(log_path: Path, logs_dir: Path) -> Path:
    rows = parse_log(log_path)
    if not rows:
        raise RuntimeError(f"benchmark completed without metrics: {log_path}")
    result_path = log_path.with_suffix(".md")
    write_summary(rows, result_path, parse_wpm_sweep(log_path))
    all_rows = []
    all_wpm_rows = []
    for path in sorted(logs_dir.glob("*.log"), key=lambda item: item.stat().st_mtime):
        all_rows.extend(parse_log(path))
        all_wpm_rows.extend(parse_wpm_sweep(path))
    write_summary(all_rows, logs_dir / "summary.md", all_wpm_rows)
    print(f"[bench-script] wrote results={result_path}", flush=True)
    print(f"[bench-script] wrote summary={logs_dir / 'summary.md'}", flush=True)
    return result_path


def main() -> int:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")

    parser = argparse.ArgumentParser(description="Prepare SD, build, upload, run, and archive benchmark firmware.")
    parser.add_argument("--env", default=DEFAULT_ENV, help="PlatformIO benchmark environment")
    parser.add_argument("--port", default="", help="Upload/monitor serial port, for example COM3")
    parser.add_argument("--baud", default="115200", help="Monitor baud rate")
    parser.add_argument("--runs", type=int, default=1, help="Number of complete benchmark runs")
    parser.add_argument("--reader-only", action="store_true", help="Two one-minute 1000 WPM tests of the modern reader; no font sync or conversion suite")
    parser.add_argument("--no-upload", action="store_true", help="Use benchmark firmware already on the device")
    parser.add_argument("--manual-eject", action="store_true", help="Wait for the user to safely eject the device")
    parser.add_argument("--storage-timeout", type=float, default=120, help="Seconds to wait for USB storage")
    parser.add_argument("--run-timeout", type=float, default=900, help="Seconds to wait for one benchmark run")
    parser.add_argument("--epub-fixture", default=DEFAULT_EPUB_FIXTURE, help="Host EPUB benchmark fixture")
    parser.add_argument(
        "--multilingual-fixture",
        default=DEFAULT_MULTILINGUAL_FIXTURE,
        help="Host multilingual RSVP benchmark fixture",
    )
    parser.add_argument(
        "--vertical-epub-fixture",
        default=DEFAULT_VERTICAL_EPUB_FIXTURE,
        help="Host vertical CJK EPUB benchmark fixture",
    )
    parser.add_argument("--sd-root", default="", help="Explicit mounted benchmark storage root")
    args = parser.parse_args()
    if args.runs < 1:
        parser.error("--runs must be at least 1")

    benchmark_dir = Path(__file__).resolve().parent
    repo_root = benchmark_dir.parent
    logs_dir = benchmark_dir / "logs"
    logs_dir.mkdir(parents=True, exist_ok=True)
    epub_fixture = (repo_root / args.epub_fixture).resolve()
    if not args.reader_only and not epub_fixture.is_file():
        raise FileNotFoundError(f"EPUB fixture missing: {epub_fixture}")
    multilingual_fixture = (repo_root / args.multilingual_fixture).resolve()
    if not args.reader_only and not multilingual_fixture.is_file():
        raise FileNotFoundError(f"multilingual fixture missing: {multilingual_fixture}")
    vertical_epub_fixture = (repo_root / args.vertical_epub_fixture).resolve()
    if not args.reader_only and not vertical_epub_fixture.is_file():
        raise FileNotFoundError(f"vertical EPUB fixture missing: {vertical_epub_fixture}")

    devices = platformio_devices(repo_root)
    initial_port = select_serial_port(devices, args.port)
    identity = next((device_serial(device) for device in devices if device.get("port") == initial_port), "")

    if not args.no_upload:
        if not initial_port:
            raise RuntimeError("could not identify the RSVP Nano serial port")
        print("[bench-script] Installing temporary diagnostic firmware; reinstall your reader firmware after testing.", flush=True)
        run_checked([*platformio_command(), "run", "-e", args.env], repo_root)
        enter_bootloader(repo_root, initial_port)
        time.sleep(1)
        bootloader_port = wait_for_serial_port(repo_root, initial_port, identity, 15)
        upload_args = [
            *platformio_command(), "run", "-e", args.env, "-t", "upload", "--upload-port", bootloader_port,
        ]
        run_checked(upload_args, repo_root)

    all_passed = True
    for run_index in range(1, args.runs + 1):
        print(f"[bench-script] waiting for benchmark storage run={run_index}/{args.runs}", flush=True)
        volume = wait_for_benchmark_volume(args.sd_root, args.storage_timeout)
        if args.reader_only:
            prepare_reader_volume(volume)
        else:
            prepare_volume(volume, epub_fixture, multilingual_fixture, vertical_epub_fixture, repo_root / "fonts")
        if args.manual_eject:
            input(f"Safely eject {volume}, then press Enter: ")
        else:
            storage_port = wait_for_serial_port(repo_root, args.port or initial_port, identity, 15)
            release_benchmark_storage(repo_root, storage_port)
        wait_for_eject(volume, 30)

        port = wait_for_serial_port(repo_root, args.port or initial_port, identity, 30)
        timestamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
        suffix = f"-run{run_index}" if args.runs > 1 else ""
        log_path = logs_dir / f"{timestamp}-{args.env}{suffix}.log"
        python = platformio_python(repo_root)
        monitor_args = (reader_monitor_command(python, port, args.baud) if args.reader_only
                        else monitor_command(python, args.env, port, args.baud))
        monitor_to_log(monitor_args, repo_root, log_path, args.run_timeout)
        if args.reader_only:
            passed = write_reader_result(log_path)
            all_passed = passed and all_passed
            if parse_log(log_path):
                write_results(log_path, logs_dir)
        else:
            write_results(log_path, logs_dir)

    return 0 if all_passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
