#!/usr/bin/env python3

import argparse
import datetime as dt
import re
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path


START_RE = re.compile(r"\[bench\] start board=(.+?) id=(.+)$")
METRIC_RE = re.compile(
    r"\[bench\] metric=([^\s]+) ok=([01]) ms=(\d+) bytes=(\d+) rate_kib_s=(\d+)"
)


@dataclass
class MetricRow:
    log: str
    env: str
    board: str
    metric: str
    ok: str
    ms: int
    bytes: int
    rate_kib_s: int


def env_from_log_name(path: Path) -> str:
    return re.sub(r"^\d{8}-\d{6}-", "", path.stem)


def parse_log(path: Path) -> list[MetricRow]:
    rows: list[MetricRow] = []
    board = ""
    env = env_from_log_name(path)
    with path.open("r", encoding="utf-8", errors="replace") as log_file:
        for line in log_file:
            start = START_RE.search(line)
            if start:
                board = start.group(1)
                continue

            metric = METRIC_RE.search(line)
            if not metric:
                continue

            rows.append(
                MetricRow(
                    log=path.name,
                    env=env,
                    board=board,
                    metric=metric.group(1),
                    ok=metric.group(2),
                    ms=int(metric.group(3)),
                    bytes=int(metric.group(4)),
                    rate_kib_s=int(metric.group(5)),
                )
            )
    return rows


def latest_by_env_and_metric(rows: list[MetricRow]) -> list[MetricRow]:
    latest: dict[tuple[str, str], MetricRow] = {}
    for row in sorted(rows, key=lambda item: item.log):
        latest[(row.env, row.metric)] = row
    return sorted(latest.values(), key=lambda item: (item.env, item.metric))


def write_summary(rows: list[MetricRow], output: Path) -> None:
    lines: list[str] = [
        "# Benchmark Summary",
        "",
        f"Generated: {dt.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}",
        "",
    ]

    if not rows:
        lines.append("No benchmark metrics found.")
        output.write_text("\n".join(lines) + "\n", encoding="utf-8")
        return

    lines += [
        "| Log | Env | Board | Metric | OK | ms | bytes | KiB/s |",
        "| --- | --- | --- | --- | ---: | ---: | ---: | ---: |",
    ]
    for row in rows:
        lines.append(
            f"| {row.log} | {row.env} | {row.board} | {row.metric} | {row.ok} | "
            f"{row.ms} | {row.bytes} | {row.rate_kib_s} |"
        )

    lines += [
        "",
        "## Latest Metrics",
        "",
        "| Env | Metric | OK | ms | KiB/s | Log |",
        "| --- | --- | ---: | ---: | ---: | --- |",
    ]
    latest_rows = latest_by_env_and_metric(rows)
    for row in latest_rows:
        lines.append(
            f"| {row.env} | {row.metric} | {row.ok} | {row.ms} | {row.rate_kib_s} | {row.log} |"
        )

    lines += ["", "## Quick Analysis", ""]
    by_metric: dict[str, list[MetricRow]] = defaultdict(list)
    for row in latest_rows:
        by_metric[row.metric].append(row)

    for metric_name in sorted(by_metric):
        successful = [row for row in by_metric[metric_name] if row.ok == "1"]
        if not successful:
            lines.append(f"- `{metric_name}`: no successful samples.")
            continue
        fastest = min(successful, key=lambda item: item.ms)
        slowest = max(successful, key=lambda item: item.ms)
        lines.append(
            f"- `{metric_name}`: fastest `{fastest.env}` at {fastest.ms} ms; "
            f"slowest `{slowest.env}` at {slowest.ms} ms."
        )

    output.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description="Create a Markdown summary from benchmark logs.")
    parser.add_argument("--logs-dir", default=str(Path(__file__).resolve().parent / "logs"))
    parser.add_argument("--output", default=str(Path(__file__).resolve().parent / "summary.md"))
    args = parser.parse_args()

    logs_dir = Path(args.logs_dir)
    output = Path(args.output)
    logs_dir.mkdir(parents=True, exist_ok=True)

    rows: list[MetricRow] = []
    for log_path in sorted(logs_dir.glob("*.log"), key=lambda item: item.stat().st_mtime):
        rows.extend(parse_log(log_path))

    write_summary(rows, output)
    print(f"[bench-script] wrote {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
