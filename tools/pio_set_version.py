Import("env")

import json
import os
import re
import subprocess
from pathlib import Path


PROJECT_DIR = Path(env.subst("$PROJECT_DIR"))
SHORT_SHA_LENGTH = 12


def run_git(*args: str) -> str:
    return subprocess.check_output(
        ["git", *args],
        cwd=PROJECT_DIR,
        text=True,
        stderr=subprocess.DEVNULL,
    ).strip()


def detect_version() -> str:
    selected_tag = os.environ.get("RSVP_FIRMWARE_TAG", "").strip()

    try:
        head_sha = run_git("rev-parse", "HEAD")
        short_sha = run_git("rev-parse", f"--short={SHORT_SHA_LENGTH}", "HEAD")
    except (subprocess.CalledProcessError, FileNotFoundError):
        return "dev"

    if selected_tag:
        try:
            tag_sha = run_git("rev-list", "-n", "1", selected_tag)
        except subprocess.CalledProcessError as error:
            raise RuntimeError(f"Firmware tag does not exist: {selected_tag}") from error

        if tag_sha != head_sha:
            raise RuntimeError(
                f"Firmware tag {selected_tag} points at {tag_sha}, "
                f"but the build is using {head_sha}"
            )

        tag = selected_tag
    else:
        try:
            tag = run_git("describe", "--tags", "--abbrev=0")
        except subprocess.CalledProcessError:
            tag = "dev"

    version = f"{tag}+{short_sha}"

    try:
        if run_git("status", "--porcelain", "--untracked-files=no"):
            version += ".dirty"
    except subprocess.CalledProcessError:
        pass

    label = os.environ.get("RSVP_BUILD_LABEL", "").strip()
    if label:
        if not re.fullmatch(r"[a-z0-9][a-z0-9.-]{0,31}", label):
            raise RuntimeError("RSVP_BUILD_LABEL must be a short lowercase build identifier")
        version += "." + label
    return version


version = detect_version()
generated_dir = Path(env.subst("$BUILD_DIR")) / "generated"
generated_header = generated_dir / "FirmwareVersion.generated.h"
contents = f"#pragma once\n\ninline constexpr char kFirmwareVersion[] = {json.dumps(version)};\n"

generated_dir.mkdir(parents=True, exist_ok=True)
if not generated_header.exists() or generated_header.read_text(encoding="utf-8") != contents:
    generated_header.write_text(contents, encoding="utf-8")

env.AppendUnique(CPPPATH=[str(generated_dir)])

# Arduino's platform.txt supplies this flag, but the PlatformIO build does not.
# Without it set_arduino_panic_handler() registers a callback that is never called.
env.AppendUnique(LINKFLAGS=["-Wl,--wrap=esp_panic_handler"])

print(f"Firmware version: {version}")
