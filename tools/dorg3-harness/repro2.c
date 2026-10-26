// repro2: faithful dorg3 embed harness for the "expected argument of type 'music'" bug.
//
// differs from repro.c: natives mimic binder.cpp SEMANTICS — typed native values
// (magic-tagged wrappers via ok_native_value_new), binder-identical type checks
// and error messages, proper bool/number/string return types.
//
// flow: ok_run(dorg3.ok) -> dump music_menu source_info -> ok_call(init) ->
//       inspect music globals (tag bits + wrapper magic) -> ok_call(music_menu) ->
//       ok_call(update) frames (game-loop order: update_menu_screen runs first).
//
// build:
//   gcc -std=c99 -I<oklang>/src -I<oklang>/include -I. repro2.c <oklang-build>/*.o -o repro2
//   ./repro2 /path/to/dorg3/game/assets/scripts/dorg3.ok
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "ok/ok.h"
// internals (harness links oklang objects directly)
#include "okobject.h"
#include "okdebug.h"

#define SV(s) ok_create_string_view(s, OK_STRING_VIEW_CALCULATE_LENGTH, true)

// ---------- fake raylib wrappers (binder.cpp native_wrapper<T> shape) ----------
#define TEXTURE_MAGIC 0x5445585fu
#define VECTOR2_MAGIC 0x56454332u
#define FONT_MAGIC 0x464f4e54u
#define SOUND_MAGIC 0x53554e44u
#define MUSIC_MAGIC 0x4c45565fu
#define ARRAY_MAGIC 0x464f524bu

typedef struct {
  unsigned int magic;
  double x, y; // vector2 payload; unused for other types
} fake_wrapper;

static const char* magic_name(unsigned int m) {
  switch (m) {
  case TEXTURE_MAGIC: return "TEXTURE";
  case VECTOR2_MAGIC: return "VECTOR2";
  case FONT_MAGIC: return "FONT";
  case SOUND_MAGIC: return "SOUND";
  case MUSIC_MAGIC: return "MUSIC";
  case ARRAY_MAGIC: return "ARRAY";
  default: return "?????";
  }
}

static void wrapper_free(void* p_w) { free(p_w); }

static void trace_call(const char* p_name, uint8_t p_argc, const ok_value* p_argv) {
  printf("    [native] %s(", p_name);
  for (uint8_t i = 0; i < p_argc; ++i) {
    uint64_t bits;
    memcpy(&bits, &p_argv[i], sizeof bits);
    printf("%s0x%016llx", i ? ", " : "", (unsigned long long)bits);
  }
  printf(")\n");
  fflush(stdout);
}

static ok_value wrap(ok* p_ok, unsigned int p_magic, double p_x, double p_y) {
  fake_wrapper* w = malloc(sizeof *w);
  if (!w) return ok_value_null();
  w->magic = p_magic;
  w->x = p_x;
  w->y = p_y;
  return ok_native_value_new(p_ok, w, wrapper_free);
}

// binder-style typed check: ok_value_is_object + native value + magic
// prints exactly the binder's error on mismatch, returns NULL
static fake_wrapper* check_type(ok* p_ok, ok_value p_v, unsigned int p_magic, const char* p_tname) {
  if (!ok_value_is_object(p_v)) {
    char buf[96];
    snprintf(buf, sizeof buf, "expected argument of type '%s'.", p_tname);
    ok_native_error(p_ok, SV(buf));
    return NULL;
  }
  fake_wrapper* w = ok_value_as_native_value(p_v);
  if (!w || w->magic != p_magic) {
    char buf[96];
    snprintf(buf, sizeof buf, "expected argument of type '%s'.", p_tname);
    ok_native_error(p_ok, SV(buf));
    return NULL;
  }
  return w;
}

static ok_value as_number_or(ok_value p_v, double p_fallback) {
  return ok_value_is_number(p_v) ? p_v : ok_value_number(p_fallback);
}

// ---------- typed natives on the init / music path ----------
static ok_value n_load_texture(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!ok_value_is_string(a[0])) {
    ok_native_error(p_ok, SV("expected argument of type 'string'."));
    return ok_value_null();
  }
  return wrap(p_ok, TEXTURE_MAGIC, 64, 64);
}
static ok_value n_load_music(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!ok_value_is_string(a[0])) {
    ok_native_error(p_ok, SV("expected argument of type 'string'."));
    return ok_value_null();
  }
  return wrap(p_ok, MUSIC_MAGIC, 0, 0);
}
static ok_value n_load_sound(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!ok_value_is_string(a[0])) {
    ok_native_error(p_ok, SV("expected argument of type 'string'."));
    return ok_value_null();
  }
  return wrap(p_ok, SOUND_MAGIC, 0, 0);
}
static ok_value n_load_font(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!ok_value_is_string(a[0])) {
    ok_native_error(p_ok, SV("expected argument of type 'string'."));
    return ok_value_null();
  }
  return wrap(p_ok, FONT_MAGIC, 0, 0);
}
static ok_value n_set_texture_filter(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!check_type(p_ok, a[0], TEXTURE_MAGIC, "texture")) return ok_value_null();
  return ok_value_bool(true);
}
static ok_value n_texture_width(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!check_type(p_ok, a[0], TEXTURE_MAGIC, "texture")) return ok_value_null();
  return ok_value_number(64);
}
static ok_value n_texture_height(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!check_type(p_ok, a[0], TEXTURE_MAGIC, "texture")) return ok_value_null();
  return ok_value_number(64);
}
static ok_value n_texture_size(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!check_type(p_ok, a[0], TEXTURE_MAGIC, "texture")) return ok_value_null();
  return wrap(p_ok, VECTOR2_MAGIC, 64, 64);
}
static ok_value n_is_texture_valid(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!check_type(p_ok, a[0], TEXTURE_MAGIC, "texture")) return ok_value_null();
  return ok_value_bool(true);
}
static ok_value n_play_music(ok* p_ok, uint8_t n, const ok_value* a) {
  trace_call("play_music", n, a);
  if (!check_type(p_ok, a[0], MUSIC_MAGIC, "music")) return ok_value_null();
  return ok_value_null();
}
static ok_value n_stop_music(ok* p_ok, uint8_t n, const ok_value* a) {
  trace_call("stop_music", n, a);
  if (!check_type(p_ok, a[0], MUSIC_MAGIC, "music")) return ok_value_null();
  return ok_value_null();
}
static ok_value n_update_music(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!check_type(p_ok, a[0], MUSIC_MAGIC, "music")) return ok_value_null();
  return ok_value_null();
}
static ok_value n_is_music_playing(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!check_type(p_ok, a[0], MUSIC_MAGIC, "music")) return ok_value_null();
  return ok_value_bool(false);
}
static ok_value n_is_music_valid(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!check_type(p_ok, a[0], MUSIC_MAGIC, "music")) return ok_value_null();
  return ok_value_bool(true);
}
static ok_value n_play_sound(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!check_type(p_ok, a[0], SOUND_MAGIC, "sound")) return ok_value_null();
  return ok_value_null();
}
static ok_value n_stop_sound(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!check_type(p_ok, a[0], SOUND_MAGIC, "sound")) return ok_value_null();
  return ok_value_null();
}
static ok_value n_is_sound_playing(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!check_type(p_ok, a[0], SOUND_MAGIC, "sound")) return ok_value_null();
  return ok_value_bool(false);
}
static ok_value n_is_sound_valid(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!check_type(p_ok, a[0], SOUND_MAGIC, "sound")) return ok_value_null();
  return ok_value_bool(true);
}
static ok_value n_get_default_font(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n; (void)a;
  return wrap(p_ok, FONT_MAGIC, 0, 0);
}
static ok_value n_is_font_valid(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!check_type(p_ok, a[0], FONT_MAGIC, "font")) return ok_value_null();
  return ok_value_bool(true);
}
static ok_value n_draw_text(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!check_type(p_ok, a[0], FONT_MAGIC, "font")) return ok_value_null();
  return ok_value_null();
}
static ok_value n_measure_text(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!check_type(p_ok, a[0], FONT_MAGIC, "font")) return ok_value_null();
  return wrap(p_ok, VECTOR2_MAGIC, 32, 16); // MeasureTextEx -> Vector2
}
// vector2 family
static ok_value v2bin(ok* p_ok, const ok_value* a, unsigned int magic, double x, double y) {
  if (!check_type(p_ok, a[0], magic, "vector2")) return ok_value_null();
  if (!check_type(p_ok, a[1], magic, "vector2")) return ok_value_null();
  return wrap(p_ok, VECTOR2_MAGIC, x, y);
}
static ok_value n_create_vector2(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  return wrap(p_ok, VECTOR2_MAGIC, ok_value_as_number(as_number_or(a[0], 0)), ok_value_as_number(as_number_or(a[1], 0)));
}
static ok_value n_vector2_x(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  fake_wrapper* w = check_type(p_ok, a[0], VECTOR2_MAGIC, "vector2");
  return w ? ok_value_number(w->x) : ok_value_null();
}
static ok_value n_vector2_y(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  fake_wrapper* w = check_type(p_ok, a[0], VECTOR2_MAGIC, "vector2");
  return w ? ok_value_number(w->y) : ok_value_null();
}
static ok_value n_vector2_add(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n; return v2bin(p_ok, a, VECTOR2_MAGIC, 0, 0);
}
static ok_value n_vector2_subtract(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n; return v2bin(p_ok, a, VECTOR2_MAGIC, 0, 0);
}
static ok_value n_vector2_multiply(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n; return v2bin(p_ok, a, VECTOR2_MAGIC, 0, 0);
}
static ok_value n_vector2_divide(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n; return v2bin(p_ok, a, VECTOR2_MAGIC, 0, 0);
}
static ok_value n_vector2_normalize(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!check_type(p_ok, a[0], VECTOR2_MAGIC, "vector2")) return ok_value_null();
  return wrap(p_ok, VECTOR2_MAGIC, 1, 0);
}
static ok_value n_vector2_distance(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!check_type(p_ok, a[0], VECTOR2_MAGIC, "vector2")) return ok_value_null();
  if (!check_type(p_ok, a[1], VECTOR2_MAGIC, "vector2")) return ok_value_null();
  return ok_value_number(1);
}
static ok_value n_vector2_distance_sqr(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!check_type(p_ok, a[0], VECTOR2_MAGIC, "vector2")) return ok_value_null();
  if (!check_type(p_ok, a[1], VECTOR2_MAGIC, "vector2")) return ok_value_null();
  return ok_value_number(1);
}
static ok_value n_vector2_length(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!check_type(p_ok, a[0], VECTOR2_MAGIC, "vector2")) return ok_value_null();
  return ok_value_number(1);
}
static ok_value n_vector2_equals(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!check_type(p_ok, a[0], VECTOR2_MAGIC, "vector2")) return ok_value_null();
  if (!check_type(p_ok, a[1], VECTOR2_MAGIC, "vector2")) return ok_value_null();
  return ok_value_bool(false);
}
static ok_value n_get_logical_mouse(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n; (void)a; return wrap(p_ok, VECTOR2_MAGIC, 100, 100);
}
static ok_value n_get_logical_pointer(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n; (void)a; return wrap(p_ok, VECTOR2_MAGIC, 100, 100);
}
static ok_value n_get_mouse_delta(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n; (void)a; return wrap(p_ok, VECTOR2_MAGIC, 0, 0);
}
static ok_value n_get_mouse_wheel_delta(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n; (void)a; return wrap(p_ok, VECTOR2_MAGIC, 0, 0);
}
// string check + passthrough (str_concat / load_file / to_str family)
static ok_value n_str_concat(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!ok_value_is_string(a[0]) || !ok_value_is_string(a[1])) {
    ok_native_error(p_ok, SV("expected argument of type 'string'."));
    return ok_value_null();
  }
  return ok_value_string(p_ok, SV("concat"));
}
static ok_value n_to_str(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!ok_value_is_number(a[0])) {
    ok_native_error(p_ok, SV("expected argument of type: 'number'."));
    return ok_value_null();
  }
  return ok_value_string(p_ok, SV("0.000000"));
}
static ok_value n_to_str_no_decimal(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!ok_value_is_number(a[0])) {
    ok_native_error(p_ok, SV("expected argument of type: 'number'."));
    return ok_value_null();
  }
  return ok_value_string(p_ok, SV("0"));
}
static ok_value n_number_from_str(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!ok_value_is_string(a[0])) {
    ok_native_error(p_ok, SV("expected argument of type: 'string'."));
    return ok_value_null();
  }
  return ok_value_number(0);
}
static ok_value n_load_file(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!ok_value_is_string(a[0])) {
    ok_native_error(p_ok, SV("expected argument of type: 'string'."));
    return ok_value_null();
  }
  return ok_value_string(p_ok, SV(""));
}
static ok_value n_set_error(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!ok_value_is_string(a[0])) {
    ok_native_error(p_ok, SV("expected argument of type: 'string'."));
    return ok_value_null();
  }
  printf("    [native] set_error called!\n");
  return ok_value_bool(true);
}
// math
static ok_value n_random_range(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)p_ok; (void)n;
  return as_number_or(a[0], 1); // deterministic
}
static ok_value n_length(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)p_ok; (void)n;
  double x = ok_value_as_number(as_number_or(a[0], 0));
  double y = ok_value_as_number(as_number_or(a[1], 0));
  return ok_value_number(sqrt(x * x + y * y));
}
static ok_value n_sin(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)p_ok; (void)n; return ok_value_number(sin(ok_value_as_number(as_number_or(a[0], 0))));
}
static ok_value n_cos(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)p_ok; (void)n; return ok_value_number(cos(ok_value_as_number(as_number_or(a[0], 0))));
}
static ok_value n_tan(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)p_ok; (void)n; return ok_value_number(tan(ok_value_as_number(as_number_or(a[0], 0))));
}
static ok_value n_atan(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)p_ok; (void)n;
  return ok_value_number(atan2(ok_value_as_number(as_number_or(a[0], 0)), ok_value_as_number(as_number_or(a[1], 0))));
}
static ok_value n_sqrt(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)p_ok; (void)n; return ok_value_number(sqrt(ok_value_as_number(as_number_or(a[0], 0))));
}
static ok_value n_modulo(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)p_ok; (void)n;
  double x = ok_value_as_number(as_number_or(a[0], 0));
  double y = ok_value_as_number(as_number_or(a[1], 1));
  return ok_value_number(y == 0 ? 0 : fmod(x, y));
}
// misc numbers/bools
static ok_value n_zero_num(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)p_ok; (void)n; (void)a; return ok_value_number(0);
}
static ok_value n_one_num(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)p_ok; (void)n; (void)a; return ok_value_number(1);
}
static ok_value n_true(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)p_ok; (void)n; (void)a; return ok_value_bool(true);
}
static ok_value n_false(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)p_ok; (void)n; (void)a; return ok_value_bool(false);
}
static ok_value n_null(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)p_ok; (void)n; (void)a; return ok_value_null();
}
static ok_value n_create_array(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n; (void)a; return wrap(p_ok, ARRAY_MAGIC, 0, 0);
}
static ok_value n_save_to_file(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)p_ok; (void)n; (void)a; return ok_value_bool(true);
}

// binder value constants (colors/enums/keys) as dummy numbers — exact binder list
static const char* CONSTS[] = {
  "pi", "red", "green", "blue", "white", "black", "raywhite",
  "screen_prop_width", "screen_prop_height", "screen_prop_scale", "screen_prop_filter",
  "screen_scale_letterbox", "screen_scale_preserve_width", "screen_scale_preserve_height",
  "texture_filter_nearest", "texture_filter_linear",
  "mouse_button_left", "mouse_button_right", "mouse_button_middle",
  "key_w", "key_a", "key_s", "key_d",
};

// ---------- registration table (all 86, faithful arities) ----------
typedef struct {
  const char* name;
  uint8_t arity;
  ok_native_fn fn;
} native_def;

static const native_def NATIVES[] = {
  {"set_error", 1, n_set_error},
  {"clear_screen", 1, n_null},
  {"load_texture", 1, n_load_texture},
  {"set_texture_filter", 2, n_set_texture_filter},
  {"is_texture_valid", 1, n_is_texture_valid},
  {"texture_size", 1, n_texture_size},
  {"texture_width", 1, n_texture_width},
  {"texture_height", 1, n_texture_height},
  {"draw_texture", 6, n_null},
  {"draw_texture_pro", 13, n_null},
  {"draw_text", 10, n_draw_text},
  {"measure_text", 4, n_measure_text},
  {"load_font", 1, n_load_font},
  {"get_default_font", 0, n_get_default_font},
  {"is_font_valid", 1, n_is_font_valid},
  {"draw_rect", 8, n_null},
  {"draw_circle", 4, n_null},
  {"create_color", 4, n_zero_num},
  {"create_vector2", 2, n_create_vector2},
  {"vector2_x", 1, n_vector2_x},
  {"vector2_y", 1, n_vector2_y},
  {"vector2_add", 2, n_vector2_add},
  {"vector2_subtract", 2, n_vector2_subtract},
  {"vector2_multiply", 2, n_vector2_multiply},
  {"vector2_divide", 2, n_vector2_divide},
  {"vector2_equals", 2, n_vector2_equals},
  {"vector2_normalize", 1, n_vector2_normalize},
  {"vector2_distance", 2, n_vector2_distance},
  {"vector2_distance_sqr", 2, n_vector2_distance_sqr},
  {"vector2_length", 1, n_vector2_length},
  {"sin", 1, n_sin},
  {"cos", 1, n_cos},
  {"tan", 1, n_tan},
  {"atan", 2, n_atan},
  {"sqrt", 1, n_sqrt},
  {"modulo", 2, n_modulo},
  {"length", 2, n_length},
  {"random_range", 2, n_random_range},
  {"number_from_str", 1, n_number_from_str},
  {"str_concat", 2, n_str_concat},
  {"to_str", 1, n_to_str},
  {"to_str_no_decimal", 1, n_to_str_no_decimal},
  {"load_file", 1, n_load_file},
  {"save_to_file", 2, n_save_to_file},
  {"load_sound", 1, n_load_sound},
  {"play_sound", 1, n_play_sound},
  {"stop_sound", 1, n_stop_sound},
  {"is_sound_playing", 1, n_is_sound_playing},
  {"is_sound_valid", 1, n_is_sound_valid},
  {"load_music", 1, n_load_music},
  {"play_music", 1, n_play_music},
  {"update_music", 1, n_update_music},
  {"stop_music", 1, n_stop_music},
  {"is_music_playing", 1, n_is_music_playing},
  {"is_music_valid", 1, n_is_music_valid},
  {"get_logical_mouse", 0, n_get_logical_mouse},
  {"get_logical_mouse_x", 0, n_one_num},
  {"get_logical_mouse_y", 0, n_one_num},
  {"get_logical_pointer", 0, n_get_logical_pointer},
  {"get_logical_pointer_x", 0, n_one_num},
  {"get_logical_pointer_y", 0, n_one_num},
  {"get_mouse_delta", 0, n_get_mouse_delta},
  {"get_mouse_delta_x", 0, n_zero_num},
  {"get_mouse_delta_y", 0, n_zero_num},
  {"get_mouse_wheel_delta", 0, n_get_mouse_wheel_delta},
  {"get_mouse_wheel_delta_x", 0, n_zero_num},
  {"get_mouse_wheel_delta_y", 0, n_zero_num},
  {"get_screen_prop", 1, n_one_num},
  {"set_screen_prop", 2, n_true},
  {"is_key_down", 1, n_false},
  {"is_key_up", 1, n_true},
  {"is_mouse_button_down", 1, n_false},
  {"is_mouse_button_pressed", 1, n_false},
  {"is_mouse_button_released", 1, n_false},
  {"is_mouse_button_up", 1, n_true},
  {"is_mouse_on_screen", 0, n_true},
  {"is_pointer_active", 0, n_false},
  {"create_array", 0, n_create_array},
  {"array_at", 2, n_null},
  {"array_clear", 1, n_null},
  {"array_foreach", 2, n_null},
  {"array_length", 1, n_zero_num},
  {"array_pop", 1, n_false},
  {"array_push", 2, n_null},
  {"array_remove", 3, n_null},
  {"array_set", 3, n_null},
};

// ---------- diagnostics ----------
static void inspect_global(ok* p_ok, const char* p_name) {
  ok_value v;
  if (!ok_global_get(p_ok, SV(p_name), &v)) {
    printf("  %-22s NOT FOUND\n", p_name);
    return;
  }
  uint64_t bits;
  memcpy(&bits, &v, sizeof bits);
  printf("  %-22s raw=0x%016llx num=%d null=%d bool=%d obj=%d", p_name,
         (unsigned long long)bits, ok_value_is_number(v), ok_value_is_null(v),
         ok_value_is_bool(v), ok_value_is_object(v));
  if (ok_value_is_object(v)) {
    object* o = VALUE_AS_OBJECT(v);
    printf(" type=%u", (unsigned)object_get_type(o));
    if (object_get_type(o) == OBJ_NATIVE_VALUE) {
      fake_wrapper* w = ok_value_as_native_value(v);
      printf(" magic=0x%08x (%s)", w ? w->magic : 0, w ? magic_name(w->magic) : "null-ud");
    }
  }
  printf("\n");
}

static void dump_source_info(ok* p_ok, const char* p_name) {
  ok_value v;
  if (!ok_global_get(p_ok, SV(p_name), &v) || !ok_value_is_object(v)) {
    printf("[dbg] %s: not an object\n", p_name);
    return;
  }
  object* o = VALUE_AS_OBJECT(v);
  if (object_get_type(o) != OBJ_CLOSURE) {
    printf("[dbg] %s: not a closure (type %u)\n", p_name, (unsigned)object_get_type(o));
    return;
  }
  object_closure* clo = (object_closure*)o;
  const chunk* ch = &clo->function->chunk;
  printf("[dbg] %s: %u code words, %u source_info entries\n", p_name, ch->code.count,
         ch->source_info.count);
  uint32_t acc = 0;
  for (uint32_t i = 0; i < ch->source_info.count; ++i) {
    const ok_line_info_repeated* info = &ch->source_info.data[i];
    printf("    instr [%4u..%4u)  line %-4u offset %u%s\n", acc, acc + info->reps,
           info->line_info.line, info->line_info.offset,
           (info->line_info.line == 254) ? "   <-- line 254" : "");
    acc += info->reps;
  }
  disassembler d;
  disassembler_init(&d, (disassembler_specs){.chunk = ch, .globals_store = NULL});
  debug_disassemble_chunk(&d, p_name);
  disassembler_deinit(&d);
}

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

int main(int argc, char** argv) {
  if (argc < 2) { fprintf(stderr, "usage: repro2 <dorg3.ok>\n"); return 2; }
  char* code = load_file(argv[1]);

  ok* p_ok = ok_create((ok_specs){0});
  if (!p_ok) { fprintf(stderr, "ok_create failed\n"); return 4; }

  for (size_t i = 0; i < sizeof NATIVES / sizeof NATIVES[0]; ++i) {
    if (!ok_global_define_native_function(p_ok, SV(NATIVES[i].name), NATIVES[i].arity, NATIVES[i].fn)) {
      fprintf(stderr, "failed to bind: %s\n", NATIVES[i].name);
      return 3;
    }
  }
  for (size_t i = 0; i < sizeof CONSTS / sizeof CONSTS[0]; ++i) {
    if (!ok_global_define(p_ok, SV(CONSTS[i]), ok_value_number(1), false)) {
      fprintf(stderr, "failed to bind const: %s\n", CONSTS[i]);
      return 3;
    }
  }

  ok_source src = {.from = OK_FROM_FILE, .code = code, .path = argv[1]};
  if (ok_run(p_ok, src) != OK) {
    fprintf(stderr, "[harness] ok_run FAILED\n");
    return 5;
  }
  printf("[harness] ok_run OK\n"); fflush(stdout);

  dump_source_info(p_ok, "music_menu");

  ok_value init;
  if (!ok_global_get(p_ok, SV("init"), &init)) { fprintf(stderr, "no init\n"); return 6; }
  ok_value result;
  if (!ok_call(p_ok, init, 0, NULL, &result)) {
    fprintf(stderr, "[harness] ok_call(init) FAILED\n");
    return 8;
  }
  printf("[harness] ok_call(init) OK (bool=%d)\n", ok_value_is_bool(result)); fflush(stdout);

  printf("[harness] music globals after init:\n");
  inspect_global(p_ok, "g_c_ballerina");
  inspect_global(p_ok, "g_c_project_32_1");
  inspect_global(p_ok, "g_c_project_23");
  inspect_global(p_ok, "g_c_project_24");
  inspect_global(p_ok, "g_current_music");
  inspect_global(p_ok, "g_c_ballerina_texture");
  fflush(stdout);

  // the exact failing call in the game: update -> music_menu -> stop_music(g_c_ballerina)
  ok_value music_menu;
  if (ok_global_get(p_ok, SV("music_menu"), &music_menu)) {
    printf("[harness] calling music_menu()...\n"); fflush(stdout);
    ok_value r;
    if (!ok_call(p_ok, music_menu, 0, NULL, &r)) {
      fprintf(stderr, "[harness] *** ok_call(music_menu) FAILED — REPRODUCED ***\n");
    } else {
      printf("[harness] ok_call(music_menu) OK\n");
    }
  }
  fflush(stdout);

  printf("[harness] music globals after music_menu:\n");
  inspect_global(p_ok, "g_c_ballerina");
  inspect_global(p_ok, "g_current_music");
  fflush(stdout);

  // game-loop order: update(delta) — update_menu_screen runs before music_menu
  ok_value update_cb;
  if (ok_global_get(p_ok, SV("update"), &update_cb)) {
    for (int frame = 0; frame < 3; ++frame) {
      ok_value delta = ok_value_number(0.016);
      ok_value r;
      printf("[harness] --- update frame %d ---\n", frame); fflush(stdout);
      if (!ok_call(p_ok, update_cb, 1, &delta, &r)) {
        fprintf(stderr, "[harness] update frame %d FAILED\n", frame);
        break;
      }
      printf("[harness] update frame %d OK\n", frame); fflush(stdout);
    }
  }

  ok_free(p_ok);
  free(code);
  return 0;
}
