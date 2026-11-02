#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

RUN_DIR="${1:-$ROOT/benchmarks}"
OUT_DIR="${2:-$ROOT/results/_staging}"

OK_REGISTER="${OK_REGISTER:-$PWD/ok}"
OK_STACK="${OK_STACK:-}"
RUNS="${RUNS:-10}"
WARMUP="${WARMUP:-2}"
STACK_COMPAT_FILE="${STACK_COMPAT_FILE:-$ROOT/stack-compatible.txt}"

for dependency in hyperfine lua python3; do
  if ! command -v "$dependency" >/dev/null 2>&1; then
    echo "error: $dependency is required" >&2
    exit 1
  fi
done

if [[ ! -x "$OK_REGISTER" ]]; then
  echo "error: register-VM executable is not executable: $OK_REGISTER" >&2
  exit 1
fi

if [[ -n "$OK_STACK" && ! -x "$OK_STACK" ]]; then
  echo "error: stack-VM executable is not executable: $OK_STACK" >&2
  exit 1
fi

if [[ ! -d "$RUN_DIR" ]]; then
  echo "error: benchmark directory does not exist: $RUN_DIR" >&2
  exit 1
fi

if ! [[ "$RUNS" =~ ^[1-9][0-9]*$ ]]; then
  echo "error: RUNS must be a positive integer" >&2
  exit 1
fi

if ! [[ "$WARMUP" =~ ^[0-9]+$ ]]; then
  echo "error: WARMUP must be a non-negative integer" >&2
  exit 1
fi

mkdir -p "$OUT_DIR"

# Load optional stack-VM compatibility metadata.
# One benchmark basename per line; blank lines and comments are ignored.
declare -A STACK_COMPAT=()
if [[ -f "$STACK_COMPAT_FILE" ]]; then
  while IFS= read -r line || [[ -n "$line" ]]; do
    line="${line%%#*}"
    line="${line//[[:space:]]/}"
    [[ -n "$line" ]] && STACK_COMPAT["$line"]=1
  done < "$STACK_COMPAT_FILE"
fi

run_one() {
  local name="$1"
  local source="$RUN_DIR/$name"

  echo
  echo "===== $name ====="

  local -a commands=()
  local -a names=()
  local -a hf_args=(
    --shell=none
    --warmup "$WARMUP"
    --runs "$RUNS"
    --export-json "$OUT_DIR/$name.json"
    --export-markdown "$OUT_DIR/$name.md"
  )
  local command

  if [[ -n "$OK_STACK" && -n "${STACK_COMPAT[$name]:-}" ]]; then
    printf -v command '%q %q' "$OK_STACK" "$source.ok"
    commands+=("$command")
    names+=("ok-stack")
  fi

  printf -v command '%q %q' "$OK_REGISTER" "$source.ok"
  commands+=("$command")
  names+=("ok-register")

  printf -v command '%q %q' "$(command -v lua)" "$source.lua"
  commands+=("$command")
  names+=("lua")

  printf -v command '%q %q' "$(command -v python3)" "$source.py"
  commands+=("$command")
  names+=("python3")

  local i
  for ((i=0; i<${#commands[@]}; i++)); do
    hf_args+=(--command-name "${names[$i]}" "${commands[$i]}")
  done

  hyperfine "${hf_args[@]}"
}

shopt -s nullglob
ok_files=("$RUN_DIR"/*.ok)
shopt -u nullglob

if (( ${#ok_files[@]} == 0 )); then
  echo "error: no .ok benchmark files found in $RUN_DIR" >&2
  exit 1
fi

# Discover benchmarks from .ok files. Each benchmark must have matching
# .lua and .py implementations; no benchmark names are hardcoded here.
for ok_file in "${ok_files[@]}"; do
  name="$(basename "$ok_file" .ok)"

  for extension in lua py; do
    if [[ ! -f "$RUN_DIR/$name.$extension" ]]; then
      echo "error: missing $RUN_DIR/$name.$extension for benchmark '$name'" >&2
      exit 1
    fi
  done

  run_one "$name"
done

echo
echo "Done. Reports are in: $OUT_DIR"
