#!/usr/bin/env python3
"""Publish a completed staging run and retain the newest archive directories."""

import argparse
import re
import shutil
from datetime import datetime, timezone
from pathlib import Path


def read_run_identifier(directory: Path) -> str:
    info = directory / "run-info.txt"

    date_value = None
    commit_value = "unknown"

    if info.is_file():
        content = info.read_text(encoding="utf-8", errors="replace")
        date_match = re.search(r"(?m)^date:\s*(\S+)", content)
        sha_match = re.search(r"(?m)^oklang_ref:.*?\(([0-9a-fA-F]{7,40})\)", content)

        if date_match:
            date_value = date_match.group(1)[:10]
        if sha_match:
            commit_value = sha_match.group(1)[:7]

    if not date_value:
        date_value = datetime.now(timezone.utc).strftime("%Y-%m-%d")

    safe_date = re.sub(r"[^0-9A-Za-z._-]", "-", date_value)
    safe_commit = re.sub(r"[^0-9A-Za-z._-]", "-", commit_value)
    return f"{safe_date}_{safe_commit}"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--results", type=Path, default=Path("results"))
    parser.add_argument(
        "--keep",
        type=int,
        default=3,
        help="Number of historical archive runs to retain (default: 3)",
    )
    args = parser.parse_args()

    if args.keep < 0:
        raise SystemExit("--keep must be zero or greater")

    results = args.results
    staging = results / "_staging"
    latest = results / "latest"
    archive = results / "archive"

    if not staging.is_dir():
        raise SystemExit(f"Staging directory does not exist: {staging}")

    json_files = list(staging.glob("*.json"))
    if not json_files:
        raise SystemExit("Refusing to publish: staging contains no Hyperfine JSON files")

    if not (staging / "run-info.txt").is_file():
        raise SystemExit("Refusing to publish: staging/run-info.txt is missing")

    if not (staging / "plot" / "overview.png").is_file():
        raise SystemExit("Refusing to publish: staging plot/overview.png is missing")

    archive.mkdir(parents=True, exist_ok=True)

    # Move the previous successful latest run into the archive first.
    if latest.exists():
        identifier = read_run_identifier(latest)
        destination = archive / identifier
        suffix = 1

        while destination.exists():
            destination = archive / f"{identifier}_{suffix}"
            suffix += 1

        shutil.move(str(latest), str(destination))
        print(f"Archived previous latest run: {destination}")

    # Publish the complete staging directory as latest.
    shutil.move(str(staging), str(latest))
    print(f"Published completed run: {latest}")

    # Keep the newest N historical runs, ordered by their sortable names.
    archived_runs = sorted(
        (path for path in archive.iterdir() if path.is_dir()),
        key=lambda path: path.name,
        reverse=True,
    )

    for old_run in archived_runs[args.keep:]:
        shutil.rmtree(old_run)
        print(f"Pruned old archive: {old_run}")

    print(f"Retained {min(len(archived_runs), args.keep)} archive run(s).")


if __name__ == "__main__":
    main()
