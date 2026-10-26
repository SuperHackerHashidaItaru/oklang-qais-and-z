// harness mimicking dorg3 game_screen::init() embed flow:
// ok_create -> ok_run(file) -> ok_global_get("init") -> ok_call(init, 0)
// usage: repro <path-to-script> [variant-build-dir]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ok/ok.h"

static char* load_file(const char* path) {
  FILE* f = fopen(path, "rb");
  if (!f) { perror(path); exit(2); }
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  rewind(f);
  char* buf = malloc((size_t)sz + 1);
  if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) { fprintf(stderr, "short read\n"); exit(2); }
  buf[sz] = '\0';
  fclose(f);
  return buf;
}

// generic stub native: prints its name and returns a number
static ok_value stub_native(ok* p_ok, uint8_t p_argc, const ok_value* p_argv) {
  (void)p_argc; (void)p_argv;
  return ok_value_number(0);
}

static void bind_stub(ok* p_ok, const char* name, uint8_t arity) {
  if (!ok_global_define_native_function(p_ok,
        ok_create_string_view(name, OK_STRING_VIEW_CALCULATE_LENGTH, true),
        arity, stub_native)) {
    fprintf(stderr, "failed to bind stub: %s\n", name);
    exit(3);
  }
}

static void bind_const(ok* p_ok, const char* name) {
  if (!ok_global_define(p_ok,
        ok_create_string_view(name, OK_STRING_VIEW_CALCULATE_LENGTH, true),
        ok_value_number(1), false)) {
    fprintf(stderr, "failed to bind const: %s\n", name);
    exit(3);
  }
}

int main(int argc, char** argv) {
  if (argc < 2) { fprintf(stderr, "usage: repro <script.ok>\n"); return 2; }
  char* code = load_file(argv[1]);

  ok* p_ok = ok_create((ok_specs){0});
  if (!p_ok) { fprintf(stderr, "ok_create failed\n"); return 4; }

  // all 86 natives dorg3's binder registers, stubbed (see stubs.inc, generated from binder.cpp)
#include "stubs.inc"
  // binder value constants (colors, enums, keys...) as dummy numbers
#include "values.inc"

  ok_source src = {.from = OK_FROM_FILE, .code = code, .path = (char*)argv[1]};
  if (ok_run(p_ok, src) != OK) {
    fprintf(stderr, "[harness] ok_run FAILED\n");
    return 5;
  }
  printf("[harness] ok_run OK\n");
  fflush(stdout);
  if (argc > 2) { // --run-only: skip callback fetch/call diagnostics
    printf("[harness] run-only mode, done\n");
    ok_free(p_ok);
    free(code);
    return 0;
  }

  ok_value init;
  if (!ok_global_get(p_ok, ok_create_string_view("init", OK_STRING_VIEW_CALCULATE_LENGTH, true), &init)) {
    fprintf(stderr, "[harness] global 'init' not found\n");
    return 6;
  }
  if (!ok_value_is_object(init)) {
    fprintf(stderr, "[harness] 'init' is not an object\n");
    return 7;
  }
  printf("[harness] calling init with 0 args...\n");

  ok_value result;
  if (!ok_call(p_ok, init, 0, NULL, &result)) {
    fprintf(stderr, "[harness] ok_call(init) FAILED\n");
    return 8;
  }
  printf("[harness] ok_call(init) OK, result is bool: %d\n", ok_value_is_bool(result));

  // mimic the game loop: update(delta) x3 + shutdown — exercises the return path
  ok_value update_cb, shutdown_cb;
  if (ok_global_get(p_ok, ok_create_string_view("update", OK_STRING_VIEW_CALCULATE_LENGTH, true), &update_cb) &&
      ok_global_get(p_ok, ok_create_string_view("shutdown", OK_STRING_VIEW_CALCULATE_LENGTH, true), &shutdown_cb)) {
    for (int frame = 0; frame < 3; ++frame) {
      ok_value delta = ok_value_number(0.016);
      ok_value upd_result;
      if (!ok_call(p_ok, update_cb, 1, &delta, &upd_result)) {
        fprintf(stderr, "[harness] update frame %d FAILED\n", frame);
        return 9;
      }
      printf("[harness] update frame %d OK\n", frame);
    }
    ok_value sh_result;
    if (!ok_call(p_ok, shutdown_cb, 0, NULL, &sh_result)) {
      fprintf(stderr, "[harness] shutdown FAILED\n");
      return 10;
    }
    printf("[harness] shutdown OK\n");
  } else {
    fprintf(stderr, "[harness] couldn't fetch update/shutdown\n");
    return 11;
  }

  ok_free(p_ok);
  free(code);
  return 0;
}
