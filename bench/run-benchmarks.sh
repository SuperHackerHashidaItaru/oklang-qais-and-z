
#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Preserve original argument convention.
RUN_DIR="${1:-$ROOT/benchmarks}"
OUT_DIR="${2:-$ROOT/results}"

OK_REGISTER="${OK_REGISTER:-$PWD/ok}"
OK_STACK="${OK_STACK:-}"
WREN="${WREN:-}"
RUNS="${RUNS:-10}"
WARMUP="${WARMUP:-2}"
STACK_COMPAT_FILE="${STACK_COMPAT_FILE:-$ROOT/stack-compatible.txt}"
CLASSES_BENCH_FILE="${CLASSES_BENCH_FILE:-$ROOT/classes-benches.txt}"

# Dependencies
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

if [[ -n "$WREN" && ! -x "$WREN" ]]; then
  echo "error: wren executable is not executable: $WREN" >&2
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

# Load stack-compatible benchmark names.
declare -A STACK_COMPAT=()

if [[ -f "$STACK_COMPAT_FILE" ]]; then
  while IFS= read -r line || [[ -n "$line" ]]; do
    line="${line%%#*}"
    line="${line//[[:space:]]/}"

    [[ -n "$line" ]] && STACK_COMPAT["$line"]=1
  done < "$STACK_COMPAT_FILE"
fi

# Load classes-only benchmark names.
declare -A CLASSES_BENCH=()

if [[ -f "$CLASSES_BENCH_FILE" ]]; then
  while IFS= read -r line || [[ -n "$line" ]]; do
    line="${line%%#*}"
    line="${line//[[:space:]]/}"

    [[ -n "$line" ]] && CLASSES_BENCH["$line"]=1
  done < "$CLASSES_BENCH_FILE"
fi

# Probe the register binary for class syntax once: the classes benches are
# skipped until the benched build speaks classes.
CLASSES_OK=0
PROBE="$(mktemp "${TMPDIR:-/tmp}/ok-classes-probe.XXXXXX.ok")"
printf 'class Probe { pub fu p() { return 1; } }\nprint(Probe().p());\n' > "$PROBE"
if "$OK_REGISTER" "$PROBE" >/dev/null 2>&1; then
  CLASSES_OK=1
fi
rm -f "$PROBE"

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

  # Optional stack VM.
  if [[ -n "$OK_STACK" && -n "${STACK_COMPAT[$name]:-}" ]]; then
    printf -v command '%q %q' "$OK_STACK" "$source.ok"
    commands+=("$command")
    names+=("ok-stack")
  fi

  # Register VM.
  printf -v command '%q %q' "$OK_REGISTER" "$source.ok"
  commands+=("$command")
  names+=("ok-register")

  # Lua.
  printf -v command '%q %q' "$(command -v lua)" "$source.lua"
  commands+=("$command")
  names+=("lua")

  # Python.
  printf -v command '%q %q' "$(command -v python3)" "$source.py"
  commands+=("$command")
  names+=("python3")

  # Wren (optional, per benchmark: only when a .wren port exists).
  if [[ -n "$WREN" && -f "$source.wren" ]]; then
    printf -v command '%q %q' "$WREN" "$source.wren"
    commands+=("$command")
    names+=("wren")
  fi

  local i

  for ((i=0; i<${#commands[@]}; i++)); do
    hf_args+=(
      --command-name "${names[$i]}"
      "${commands[$i]}"
    )
  done

  hyperfine "${hf_args[@]}"
}

# Automatically discover complete benchmark triplets.
shopt -s nullglob
ok_files=("$RUN_DIR"/*.ok)
shopt -u nullglob

if (( ${#ok_files[@]} == 0 )); then
  echo "error: no .ok files found in $RUN_DIR" >&2
  exit 1
fi

discovered=()
skipped=()

for ok_file in "${ok_files[@]}"; do
  name="$(basename "$ok_file" .ok)"

  if [[ -n "${CLASSES_BENCH[$name]:-}" ]] && (( CLASSES_OK == 0 )); then
    skipped+=("$name (needs the classes build)")
    continue
  fi

  if [[ -f "$RUN_DIR/$name.lua" && -f "$RUN_DIR/$name.py" ]]; then
    discovered+=("$name")
  else
    missing=()

    [[ -f "$RUN_DIR/$name.lua" ]] || missing+=("$name.lua")
    [[ -f "$RUN_DIR/$name.py" ]] || missing+=("$name.py")

    skipped+=("$name (missing: ${missing[*]})")
  fi
done

if (( ${#skipped[@]} > 0 )); then
  echo "Ignoring incomplete benchmark triplets:"

  printf '  - %s\n' "${skipped[@]}"
fi

if (( ${#discovered[@]} == 0 )); then
  echo "error: no complete .ok/.lua/.py benchmark triplets found" >&2
  exit 1
fi

echo
echo "Discovered ${#discovered[@]} benchmarks:"

printf '  - %s\n' "${discovered[@]}"

# Execute every discovered benchmark.
for name in "${discovered[@]}"; do
  run_one "$name"
done

echo
echo "Done. Reports are in: $OUT_DIR"
echo "Note: stack VM is only included for compatible benchmarks when OK_STACK is set."
echo "Note: wren is included for benchmarks with a .wren port when WREN is set."
echo "Note: classes-only benchmarks are skipped when the register build does not speak classes."

