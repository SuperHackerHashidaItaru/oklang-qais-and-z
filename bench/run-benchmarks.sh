#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OK_REGISTER="${OK_REGISTER:-$PWD/ok}"
OK_STACK="${OK_STACK:-}"
RUNS="${RUNS:-10}"
WARMUP="${WARMUP:-2}"

if ! command -v hyperfine >/dev/null 2>&1; then echo "error: hyperfine is required" >&2; exit 1; fi
if ! command -v lua >/dev/null 2>&1; then echo "error: lua is required" >&2; exit 1; fi
if ! command -v python3 >/dev/null 2>&1; then echo "error: python3 is required" >&2; exit 1; fi
if [[ ! -x "$OK_REGISTER" ]]; then echo "error: register-VM executable is not executable: $OK_REGISTER" >&2; exit 1; fi
if [[ -n "$OK_STACK" && ! -x "$OK_STACK" ]]; then echo "error: stack-VM executable is not executable: $OK_STACK" >&2; exit 1; fi

legacy_benchmarks=(loop_counter arithmetic_sum branch_toggle local_pressure global_access function_calls closure_upvalue recursion_fib)
new_benchmarks=(numeric_for_counter numeric_for_sum numeric_for_step array_index_sum array_for_in_sum array_for_in_sum_indexed array_push_growth array_literal_alloc assign_add_plain assign_add_compound assign_sub_plain assign_sub_compound assign_mul_plain assign_mul_compound assign_div_plain assign_div_compound array_subscript_add_plain array_subscript_add_compound array_subscript_variable_add_plain array_subscript_variable_add_compound global_array_subscript_add_plain global_array_subscript_add_compound)

run_one() {
  local name="$1"
  local include_stack="$2"
  echo
  echo "===== $name ====="
  local -a commands=()
  local -a names=()
  local -a hf_args=(--shell=none --warmup "$WARMUP" --runs "$RUNS"
    --export-json "$ROOT/results/$name.json"
    --export-markdown "$ROOT/results/$name.md")
  local command

  if [[ "$include_stack" == yes && -n "$OK_STACK" ]]; then
    printf -v command '%q %q' "$OK_STACK" "$ROOT/$name.ok"
    commands+=("$command"); names+=("ok-stack")
  fi
  printf -v command '%q %q' "$OK_REGISTER" "$ROOT/$name.ok"
  commands+=("$command"); names+=("ok-register")
  printf -v command '%q %q' "$(command -v lua)" "$ROOT/$name.lua"
  commands+=("$command"); names+=("lua")
  printf -v command '%q %q' "$(command -v python3)" "$ROOT/$name.py"
  commands+=("$command"); names+=("python3")

  local i
  for ((i=0; i<${#commands[@]}; i++)); do
    hf_args+=(--command-name "${names[$i]}" "${commands[$i]}")
  done
  hyperfine "${hf_args[@]}"
}

mkdir -p "$ROOT/results"
for name in "${legacy_benchmarks[@]}"; do run_one "$name" yes; done
for name in "${new_benchmarks[@]}"; do run_one "$name" no; done

echo
echo "Done. Reports are in: $ROOT/results"
echo "Note: the stack VM is intentionally omitted from array, numeric-for, and compound-assignment tests."
