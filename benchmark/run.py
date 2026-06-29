#!/usr/bin/env python3

import argparse
import datetime as dt
import shutil
import subprocess
from pathlib import Path


DEFAULT_ENV = "benchmark_waveshare_esp32s3"
DEFAULT_EPUB_FIXTURE = "RSVPNanoCompanion/testdata/conversion/Dracula-epub.epub"
DEVICE_EPUB_PATH = "/benchmark/Dracula-epub.epub"


def run_checked(args: list[str], cwd: Path) -> None:
    print(f"[bench-script] {' '.join(args)}", flush=True)
    subprocess.run(args, cwd=cwd, check=True)


def monitor_to_log(args: list[str], cwd: Path, log_path: Path) -> int:
    print(f"[bench-script] monitoring log={log_path}", flush=True)
    print("[bench-script] press Ctrl+C to stop the monitor", flush=True)
    with log_path.open("w", encoding="utf-8", errors="replace") as log_file:
        process = subprocess.Popen(
            args,
            cwd=cwd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
        assert process.stdout is not None
        try:
            for line in process.stdout:
                print(line, end="")
                log_file.write(line)
                log_file.flush()
        except KeyboardInterrupt:
            process.terminate()
            return process.wait()
        return process.wait()


def main() -> int:
    parser = argparse.ArgumentParser(description="Build, upload, and monitor benchmark firmware.")
    parser.add_argument("--env", default=DEFAULT_ENV, help="PlatformIO benchmark environment")
    parser.add_argument("--port", default="", help="Upload/monitor serial port, for example COM7")
    parser.add_argument("--baud", default="115200", help="Monitor baud rate")
    parser.add_argument(
        "--epub-fixture",
        default=DEFAULT_EPUB_FIXTURE,
        help="Host EPUB fixture that should be copied to the SD benchmark folder",
    )
    parser.add_argument(
        "--sd-root",
        default="",
        help="Mounted SD card root. When set, the Dracula EPUB is copied to /benchmark before upload.",
    )
    args = parser.parse_args()

    benchmark_dir = Path(__file__).resolve().parent
    repo_root = benchmark_dir.parent
    logs_dir = benchmark_dir / "logs"
    logs_dir.mkdir(parents=True, exist_ok=True)

    timestamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    log_path = logs_dir / f"{timestamp}-{args.env}.log"
    epub_fixture = (repo_root / args.epub_fixture).resolve()
    if epub_fixture.exists():
        print(
            "[bench-script] EPUB fixture ready: "
            f"{epub_fixture} ({epub_fixture.stat().st_size} bytes)",
            flush=True,
        )
        if args.sd_root:
            sd_root = Path(args.sd_root).resolve()
            target_dir = sd_root / DEVICE_EPUB_PATH.strip("/").rsplit("/", 1)[0]
            target_dir.mkdir(parents=True, exist_ok=True)
            target_path = target_dir / Path(DEVICE_EPUB_PATH).name
            shutil.copy2(epub_fixture, target_path)
            print(f"[bench-script] copied EPUB to SD: {target_path}", flush=True)
        else:
            print(
                f"[bench-script] copy it to SD as {DEVICE_EPUB_PATH} before starting the on-device run",
                flush=True,
            )
    else:
        print(f"[bench-script] EPUB fixture missing: {epub_fixture}", flush=True)
    print("[bench-script] device will wait for touch/button input before benchmarks", flush=True)

    upload_args = ["uvx", "platformio", "run", "-e", args.env, "-t", "upload"]
    if args.port:
        upload_args += ["--upload-port", args.port]
    run_checked(upload_args, repo_root)

    monitor_args = [
        "uvx",
        "platformio",
        "device",
        "monitor",
        "-e",
        args.env,
        "-b",
        args.baud,
        "-f",
        "default",
        "-f",
        "time",
        "-f",
        "esp32_exception_decoder",
    ]
    if args.port:
        monitor_args += ["-p", args.port]

    return monitor_to_log(monitor_args, repo_root, log_path)


if __name__ == "__main__":
    raise SystemExit(main())
