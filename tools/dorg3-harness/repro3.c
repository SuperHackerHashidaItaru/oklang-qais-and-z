// repro3: dorg3 full-game soak harness for the "invalid call: value is not
// callable" crash in forks_update (reported 2026-10-24 ~23:00).
//
// upgrades over repro2:
//   - array natives are FAITHFUL: real ok_value vectors in ARRAY_MAGIC wrappers,
//     binder-identical checks/messages, and array_foreach actually calls the
//     script closure through ok_call exactly like binder.cpp does.
//   - vector2 math is REAL (add/sub/mul/div/normalize/distance on wrapper x/y)
//     so forks home in on the dorg and collisions actually fire.
//   - random_range is a real inclusive int RNG (std::mt19937-equivalent).
//   - input driver PLAYS THE GAME: clicks PLAY, steers the dorg with WASD to
//     dodge forks (multi-fork waves => multi-iteration array_foreach), dies,
//     clicks Retry, repeats. polls g_screen/g_dorg_dead/g_score each frame.
//   - lifecycle: ok_run -> ok_call(init) -> ok_call(update) xN -> ok_call(shutdown)
//
// build (plain):
//   gcc -std=c99 -w -I<oklang>/src -I<oklang>/include -I. repro3.c \
//       $(ls <oklang>/build-gcc/*.o | grep -v cli_main) -o repro3 -lm
// build (aggressive GC — primary reproducer):
//   gcc -std=c99 -w -I<oklang>/src -I<oklang>/include -I. repro3.c \
//       $(ls <oklang>/build-aggressive/*.o | grep -v cli_main) -o repro3_aggr -lm
// env: OK_FRAMES=<n> (default 2400), OK_SEED=<n> (default 1)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "ok/ok.h"
// internals (harness links oklang objects directly)
#include "okobject.h"

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
  double x, y; // vector2 payload
} v2_wrapper;

// binder's array wrapper: std::vector<ok_value> behind ARRAY_MAGIC
typedef struct {
  unsigned int magic;
  ok_value* vals;
  size_t count, cap;
} arr_wrapper;

static void v2_free(void* p_w) { free(p_w); }
static void arr_free(void* p_w) {
  arr_wrapper* a = (arr_wrapper*)p_w;
  free(a->vals);
  free(a);
}

static ok_value wrap_v2(ok* p_ok, double p_x, double p_y) {
  v2_wrapper* w = malloc(sizeof *w);
  if (!w) return ok_value_null();
  w->magic = VECTOR2_MAGIC; w->x = p_x; w->y = p_y;
  return ok_native_value_new(p_ok, w, v2_free);
}
static ok_value wrap_arr(ok* p_ok) {
  arr_wrapper* w = malloc(sizeof *w);
  if (!w) return ok_value_null();
  w->magic = ARRAY_MAGIC; w->vals = NULL; w->count = 0; w->cap = 0;
  return ok_native_value_new(p_ok, w, arr_free);
}

// binder-style typed check
static void* check_type(ok* p_ok, ok_value p_v, unsigned int p_magic, const char* p_tname) {
  if (!ok_value_is_object(p_v)) {
    char buf[96];
    snprintf(buf, sizeof buf, "expected argument of type '%s'.", p_tname);
    ok_native_error(p_ok, SV(buf));
    return NULL;
  }
  void* ud = ok_value_as_native_value(p_v);
  if (!ud || *(unsigned int*)ud != p_magic) {
    char buf[96];
    snprintf(buf, sizeof buf, "expected argument of type '%s'.", p_tname);
    ok_native_error(p_ok, SV(buf));
    return NULL;
  }
  return ud;
}
static v2_wrapper* check_v2(ok* p_ok, ok_value p_v) {
  return (v2_wrapper*)check_type(p_ok, p_v, VECTOR2_MAGIC, "vector2");
}
static arr_wrapper* check_arr(ok* p_ok, ok_value p_v, const char* p_which) {
  return (arr_wrapper*)check_type(p_ok, p_v, ARRAY_MAGIC, p_which);
}
static ok_value as_number_or(ok_value p_v, double p_fallback) {
  return ok_value_is_number(p_v) ? p_v : ok_value_number(p_fallback);
}

// ---------- harness state ----------
static long g_frame = 0;           // current update frame
static long g_frames_max = 2400;
// scripted input
static double s_pointer_x = 640, s_pointer_y = 350;
static bool s_mb_released = false;
static bool s_key_w = false, s_key_a = false, s_key_s = false, s_key_d = false;
static long s_death_seen_at = -1;  // frame when dorg_dead first observed
static long s_click_play_at = 10;
// stats
static long s_foreach_calls = 0, s_callback_calls = 0, s_forks_pushed = 0;
static long s_array_push_calls = 0, s_array_set_calls = 0, s_deaths = 0;
// array registry (identifies forks/porgs/keys arrays by pointer)
#define MAX_ARRAYS 16
static arr_wrapper* s_arrays[MAX_ARRAYS];
static int s_array_count = 0;
static void register_arr(arr_wrapper* w) {
  for (int i = 0; i < s_array_count; ++i) if (s_arrays[i] == w) return;
  if (s_array_count < MAX_ARRAYS) s_arrays[s_array_count++] = w;
}

// per-interval native census (reset at each heartbeat): pinpoints where the
// update chain stops — clear_screen=screen fn, create_color=game screen prologue
// (bg colors), is_key_down=keyboard_update, array_length=forks/keys_update,
// random_range=add_fork/add_porg/emplace_key, foreach=forks/porgs_update.
static long s_c_clrscreen, s_c_color, s_c_key, s_c_arrlen, s_c_rand, s_c_foreach_last;

// ---------- textures / fonts / sounds / music (typed, inert) ----------
static ok_value n_load_texture(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!ok_value_is_string(a[0])) { ok_native_error(p_ok, SV("expected argument of type 'string'.")); return ok_value_null(); }
  v2_wrapper* w = malloc(sizeof *w); w->magic = TEXTURE_MAGIC; w->x = 64; w->y = 64;
  return ok_native_value_new(p_ok, w, v2_free);
}
static ok_value n_load_music(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!ok_value_is_string(a[0])) { ok_native_error(p_ok, SV("expected argument of type 'string'.")); return ok_value_null(); }
  v2_wrapper* w = malloc(sizeof *w); w->magic = MUSIC_MAGIC; w->x = 0; w->y = 0;
  return ok_native_value_new(p_ok, w, v2_free);
}
static ok_value n_load_sound(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!ok_value_is_string(a[0])) { ok_native_error(p_ok, SV("expected argument of type 'string'.")); return ok_value_null(); }
  v2_wrapper* w = malloc(sizeof *w); w->magic = SOUND_MAGIC; w->x = 0; w->y = 0;
  return ok_native_value_new(p_ok, w, v2_free);
}
static ok_value n_load_font(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!ok_value_is_string(a[0])) { ok_native_error(p_ok, SV("expected argument of type 'string'.")); return ok_value_null(); }
  v2_wrapper* w = malloc(sizeof *w); w->magic = FONT_MAGIC; w->x = 0; w->y = 0;
  return ok_native_value_new(p_ok, w, v2_free);
}
static ok_value tex(ok* p_ok) { (void)p_ok; v2_wrapper* w = malloc(sizeof *w); w->magic = TEXTURE_MAGIC; w->x = 64; w->y = 64; return ok_native_value_new(p_ok, w, v2_free); }
static ok_value n_get_default_font(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n; (void)a;
  v2_wrapper* w = malloc(sizeof *w); w->magic = FONT_MAGIC; w->x = 0; w->y = 0;
  return ok_native_value_new(p_ok, w, v2_free);
}
static ok_value n_set_texture_filter(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!check_type(p_ok, a[0], TEXTURE_MAGIC, "texture")) return ok_value_null();
  return ok_value_bool(true);
}
static ok_value n_texture_width(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  v2_wrapper* w = check_type(p_ok, a[0], TEXTURE_MAGIC, "texture");
  return w ? ok_value_number(w->x) : ok_value_null();
}
static ok_value n_texture_height(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  v2_wrapper* w = check_type(p_ok, a[0], TEXTURE_MAGIC, "texture");
  return w ? ok_value_number(w->y) : ok_value_null();
}
static ok_value n_texture_size(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  v2_wrapper* w = check_type(p_ok, a[0], TEXTURE_MAGIC, "texture");
  return w ? wrap_v2(p_ok, w->x, w->y) : ok_value_null();
}
static ok_value n_is_texture_valid(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!check_type(p_ok, a[0], TEXTURE_MAGIC, "texture")) return ok_value_null();
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
  return wrap_v2(p_ok, 32, 16); // deterministic MeasureTextEx stand-in
}
static ok_value n_play_music(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!check_type(p_ok, a[0], MUSIC_MAGIC, "music")) return ok_value_null();
  return ok_value_null();
}
static ok_value n_stop_music(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
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
static ok_value n_is_font_valid(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!check_type(p_ok, a[0], FONT_MAGIC, "font")) return ok_value_null();
  return ok_value_bool(true);
}
static ok_value n_create_color(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)p_ok; (void)n; (void)a; s_c_color++; return ok_value_number(0);
}
static ok_value n_clear_screen(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)p_ok; (void)n; (void)a; s_c_clrscreen++; return ok_value_null();
}
static ok_value n_set_error(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!ok_value_is_string(a[0])) { ok_native_error(p_ok, SV("expected argument of type: 'string'.")); return ok_value_null(); }
  fprintf(stderr, "[harness] set_error called: (string)\n");
  return ok_value_bool(true);
}
static ok_value n_print(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)p_ok; (void)n;
  if (ok_value_is_string(a[0])) {
    ok_cstring_view v = ok_value_as_string(a[0]);
    printf("[script print] %.*s\n", (int)ok_cstring_view_get_length(&v), v.chars);
  }
  return ok_value_null();
}

// ---------- math (REAL semantics) ----------
static unsigned int s_rng_state = 1;
static void seed_rng(unsigned int s) { s_rng_state = s ? s : 1; }
static unsigned int next_u32(void) { // mt19937 stand-in: xorshift32
  s_rng_state ^= s_rng_state << 13;
  s_rng_state ^= s_rng_state >> 17;
  s_rng_state ^= s_rng_state << 5;
  return s_rng_state;
}
static ok_value n_random_range(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)p_ok; (void)n; s_c_rand++;
  if (!ok_value_is_number(a[0])) { ok_native_error(p_ok, SV("expected 1st argument of type 'number'.")); return ok_value_null(); }
  if (!ok_value_is_number(a[1])) { ok_native_error(p_ok, SV("expected 2nd argument of type 'number'.")); return ok_value_null(); }
  long long lo = (long long)ok_value_as_number(a[0]);
  long long hi = (long long)ok_value_as_number(a[1]);
  if (hi < lo) { long long t = lo; lo = hi; hi = t; }
  return ok_value_number((double)(lo + (long long)(next_u32() % (unsigned long long)(hi - lo + 1))));
}
static ok_value n_sin(ok* p_ok, uint8_t n, const ok_value* a) { (void)p_ok; (void)n; return ok_value_number(sin(ok_value_as_number(as_number_or(a[0], 0)))); }
static ok_value n_cos(ok* p_ok, uint8_t n, const ok_value* a) { (void)p_ok; (void)n; return ok_value_number(cos(ok_value_as_number(as_number_or(a[0], 0)))); }
static ok_value n_tan(ok* p_ok, uint8_t n, const ok_value* a) { (void)p_ok; (void)n; return ok_value_number(tan(ok_value_as_number(as_number_or(a[0], 0)))); }
static ok_value n_atan(ok* p_ok, uint8_t n, const ok_value* a) { (void)p_ok; (void)n; return ok_value_number(atan2(ok_value_as_number(as_number_or(a[0], 0)), ok_value_as_number(as_number_or(a[1], 0)))); }
static ok_value n_sqrt(ok* p_ok, uint8_t n, const ok_value* a) { (void)p_ok; (void)n; return ok_value_number(sqrt(ok_value_as_number(as_number_or(a[0], 0)))); }
static ok_value n_modulo(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)p_ok; (void)n;
  double x = ok_value_as_number(as_number_or(a[0], 0));
  double y = ok_value_as_number(as_number_or(a[1], 1));
  return ok_value_number(y == 0 ? 0 : fmod(x, y));
}
static ok_value n_length(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)p_ok; (void)n;
  double x = ok_value_as_number(as_number_or(a[0], 0));
  double y = ok_value_as_number(as_number_or(a[1], 0));
  return ok_value_number(sqrt(x * x + y * y));
}
static ok_value n_min(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)p_ok; (void)n;
  double x = ok_value_as_number(as_number_or(a[0], 0)), y = ok_value_as_number(as_number_or(a[1], 0));
  return ok_value_number(x < y ? x : y);
}
static ok_value n_max(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)p_ok; (void)n;
  double x = ok_value_as_number(as_number_or(a[0], 0)), y = ok_value_as_number(as_number_or(a[1], 0));
  return ok_value_number(x > y ? x : y);
}

// ---------- vector2 (REAL math) ----------
static ok_value n_create_vector2(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  return wrap_v2(p_ok, ok_value_as_number(as_number_or(a[0], 0)), ok_value_as_number(as_number_or(a[1], 0)));
}
static ok_value n_vector2_x(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n; v2_wrapper* w = check_v2(p_ok, a[0]); return w ? ok_value_number(w->x) : ok_value_null();
}
static ok_value n_vector2_y(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n; v2_wrapper* w = check_v2(p_ok, a[0]); return w ? ok_value_number(w->y) : ok_value_null();
}
static ok_value n_vector2_add(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n; v2_wrapper* x = check_v2(p_ok, a[0]); v2_wrapper* y = check_v2(p_ok, a[1]);
  return (x && y) ? wrap_v2(p_ok, x->x + y->x, x->y + y->y) : ok_value_null();
}
static ok_value n_vector2_subtract(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n; v2_wrapper* x = check_v2(p_ok, a[0]); v2_wrapper* y = check_v2(p_ok, a[1]);
  return (x && y) ? wrap_v2(p_ok, x->x - y->x, x->y - y->y) : ok_value_null();
}
static ok_value n_vector2_multiply(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n; v2_wrapper* x = check_v2(p_ok, a[0]); v2_wrapper* y = check_v2(p_ok, a[1]);
  return (x && y) ? wrap_v2(p_ok, x->x * y->x, x->y * y->y) : ok_value_null();
}
static ok_value n_vector2_divide(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n; v2_wrapper* x = check_v2(p_ok, a[0]); v2_wrapper* y = check_v2(p_ok, a[1]);
  if (!x || !y) return ok_value_null();
  double dx = y->x == 0 ? 0 : x->x / y->x;
  double dy = y->y == 0 ? 0 : x->y / y->y;
  return wrap_v2(p_ok, dx, dy);
}
static ok_value n_vector2_normalize(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n; v2_wrapper* w = check_v2(p_ok, a[0]); if (!w) return ok_value_null();
  double len = sqrt(w->x * w->x + w->y * w->y);
  if (len == 0) return wrap_v2(p_ok, 0, 0);
  return wrap_v2(p_ok, w->x / len, w->y / len);
}
static ok_value n_vector2_distance(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n; v2_wrapper* x = check_v2(p_ok, a[0]); v2_wrapper* y = check_v2(p_ok, a[1]);
  if (!x || !y) return ok_value_null();
  double dx = x->x - y->x, dy = x->y - y->y;
  return ok_value_number(sqrt(dx * dx + dy * dy));
}
static ok_value n_vector2_distance_sqr(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n; v2_wrapper* x = check_v2(p_ok, a[0]); v2_wrapper* y = check_v2(p_ok, a[1]);
  if (!x || !y) return ok_value_null();
  double dx = x->x - y->x, dy = x->y - y->y;
  return ok_value_number(dx * dx + dy * dy);
}
static ok_value n_vector2_length(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n; v2_wrapper* w = check_v2(p_ok, a[0]); if (!w) return ok_value_null();
  return ok_value_number(sqrt(w->x * w->x + w->y * w->y));
}
static ok_value n_vector2_equals(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n; v2_wrapper* x = check_v2(p_ok, a[0]); v2_wrapper* y = check_v2(p_ok, a[1]);
  if (!x || !y) return ok_value_null();
  return ok_value_bool(x->x == y->x && x->y == y->y);
}

// ---------- arrays (binder-faithful, with the callback loop) ----------
static ok_value n_create_array(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n; (void)a; return wrap_arr(p_ok);
}
static ok_value n_array_push(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  arr_wrapper* w = check_arr(p_ok, a[0], "array"); if (!w) return ok_value_null();
  if (ok_value_is_object(a[1])) {
    ok_native_error(p_ok, SV("array only supports primitive types."));
    return ok_value_null();
  }
  if (w->count == w->cap) {
    size_t nc = w->cap ? w->cap * 2 : 8;
    ok_value* nv = realloc(w->vals, nc * sizeof(ok_value));
    if (!nv) { ok_native_error(p_ok, SV("out of memory.")); return ok_value_null(); }
    w->vals = nv; w->cap = nc;
  }
  w->vals[w->count++] = a[1];
  s_array_push_calls++;
  register_arr(w);
  if (w->count > s_forks_pushed) s_forks_pushed = w->count; // rough peak tracker
  return ok_value_null();
}
static ok_value n_array_length(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n; s_c_arrlen++;
  arr_wrapper* w = check_arr(p_ok, a[0], "array"); if (!w) return ok_value_null();
  return ok_value_number((double)w->count);
}
static ok_value n_array_clear(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  arr_wrapper* w = check_arr(p_ok, a[0], "array"); if (!w) return ok_value_null();
  w->count = 0;
  return ok_value_null();
}
static ok_value n_array_at(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  arr_wrapper* w = check_arr(p_ok, a[0], "array"); if (!w) return ok_value_null();
  if (!ok_value_is_number(a[1])) { ok_native_error(p_ok, SV("expected 2nd argument of type 'number'")); return ok_value_null(); }
  double i = ok_value_as_number(a[1]);
  if (i >= (double)w->count || i < 0) return ok_value_null();
  return w->vals[(size_t)i];
}
static ok_value n_array_set(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  arr_wrapper* w = check_arr(p_ok, a[0], "array"); if (!w) return ok_value_null();
  if (!ok_value_is_number(a[1])) { ok_native_error(p_ok, SV("expected 2nd argument of type 'number'")); return ok_value_null(); }
  if (ok_value_is_object(a[2])) { ok_native_error(p_ok, SV("array only supports primitive types.")); return ok_value_null(); }
  double i = ok_value_as_number(a[1]);
  if (i >= (double)w->count || i < 0) return ok_value_null();
  w->vals[(size_t)i] = a[2];
  s_array_set_calls++;
  return w->vals[(size_t)i];
}
static ok_value n_array_pop(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  arr_wrapper* w = check_arr(p_ok, a[0], "array"); if (!w) return ok_value_null();
  if (w->count) { w->count--; return ok_value_bool(true); }
  return ok_value_bool(false);
}
static ok_value n_array_remove(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  arr_wrapper* w = check_arr(p_ok, a[0], "array"); if (!w) return ok_value_null();
  if (!ok_value_is_number(a[1])) { ok_native_error(p_ok, SV("expected 2nd argument of type 'number'")); return ok_value_null(); }
  if (!ok_value_is_number(a[2])) { ok_native_error(p_ok, SV("expected 3rd argument of type 'number'")); return ok_value_null(); }
  size_t start = (size_t)ok_value_as_number(a[1]);
  size_t end = (size_t)ok_value_as_number(a[2]);
  if (start > end || end > w->count) { ok_native_error(p_ok, SV("invalid remove")); return ok_value_null(); }
  memmove(w->vals + start, w->vals + end + 1, (w->count - end - 1) * sizeof(ok_value));
  w->count -= (end - start + 1);
  return ok_value_null();
}
// THE callback native — verbatim port of binder.cpp array_foreach
static ok_value n_array_foreach(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  arr_wrapper* w = check_arr(p_ok, a[0], "array"); if (!w) return ok_value_null();
  if (!ok_value_is_object(a[1])) {
    ok_native_error(p_ok, SV("expected 2nd argument of type 'function'"));
    return ok_value_null();
  }
  s_foreach_calls++;
  for (size_t i = 0; i < w->count; ++i) {
    ok_value value = w->vals[i]; // copy, like `auto value : wrapped`
    ok_value res;
    s_callback_calls++;
    if (!ok_call(p_ok, a[1], 1, &value, &res)) {
      return ok_value_null();
    }
    if (ok_value_is_null(res)) continue;
    if (!ok_value_is_bool(res)) return res;
    if (!ok_value_as_bool(res)) break;
  }
  return ok_value_null();
}

// ---------- strings / files ----------
static ok_value n_str_concat(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!ok_value_is_string(a[0]) || !ok_value_is_string(a[1])) {
    ok_native_error(p_ok, SV("expected argument of type 'string'."));
    return ok_value_null();
  }
  ok_cstring_view x = ok_value_as_string(a[0]);
  ok_cstring_view y = ok_value_as_string(a[1]);
  char buf[512];
  snprintf(buf, sizeof buf, "%.*s%.*s",
           (int)ok_cstring_view_get_length(&x), x.chars,
           (int)ok_cstring_view_get_length(&y), y.chars);
  return ok_value_string(p_ok, SV(buf));
}
static ok_value n_to_str(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!ok_value_is_number(a[0])) { ok_native_error(p_ok, SV("expected argument of type: 'number'.")); return ok_value_null(); }
  char buf[64];
  snprintf(buf, sizeof buf, "%g", ok_value_as_number(a[0]));
  return ok_value_string(p_ok, SV(buf));
}
static ok_value n_to_str_no_decimal(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!ok_value_is_number(a[0])) { ok_native_error(p_ok, SV("expected argument of type: 'number'.")); return ok_value_null(); }
  char buf[64];
  snprintf(buf, sizeof buf, "%lld", (long long)ok_value_as_number(a[0]));
  return ok_value_string(p_ok, SV(buf));
}
static ok_value n_number_from_str(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!ok_value_is_string(a[0])) { ok_native_error(p_ok, SV("expected argument of type: 'string'.")); return ok_value_null(); }
  ok_cstring_view v = ok_value_as_string(a[0]);
  char buf[64];
  uint64_t len = ok_cstring_view_get_length(&v);
  if (len > 63) len = 63;
  snprintf(buf, sizeof buf, "%.*s", (int)len, v.chars);
  char* end = NULL;
  double d = strtod(buf, &end);
  if (end == buf || (end && *end != '\0')) return ok_value_null();
  return ok_value_number(d);
}
static ok_value n_load_file(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n;
  if (!ok_value_is_string(a[0])) { ok_native_error(p_ok, SV("expected argument of type: 'string'.")); return ok_value_null(); }
  return ok_value_string(p_ok, SV("42")); // a high score, why not
}
static ok_value n_save_to_file(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)p_ok; (void)n; (void)a; return ok_value_bool(true);
}

// ---------- input natives (driven by the script below main) ----------
static ok_value n_get_logical_pointer(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n; (void)a; return wrap_v2(p_ok, s_pointer_x, s_pointer_y);
}
static ok_value n_get_logical_pointer_x(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n; (void)a; (void)p_ok; return ok_value_number(s_pointer_x);
}
static ok_value n_get_logical_pointer_y(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n; (void)a; (void)p_ok; return ok_value_number(s_pointer_y);
}
static ok_value n_get_logical_mouse(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n; (void)a; return wrap_v2(p_ok, s_pointer_x, s_pointer_y);
}
static ok_value n_get_logical_mouse_x(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n; (void)a; (void)p_ok; return ok_value_number(s_pointer_x);
}
static ok_value n_get_logical_mouse_y(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n; (void)a; (void)p_ok; return ok_value_number(s_pointer_y);
}
static ok_value n_get_mouse_delta(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n; (void)a; return wrap_v2(p_ok, 0, 0);
}
static ok_value n_get_mouse_wheel_delta(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)n; (void)a; return wrap_v2(p_ok, 0, 0);
}
static ok_value n_zero_num(ok* p_ok, uint8_t n, const ok_value* a) { (void)p_ok; (void)n; (void)a; return ok_value_number(0); }
static ok_value n_one_num(ok* p_ok, uint8_t n, const ok_value* a) { (void)p_ok; (void)n; (void)a; return ok_value_number(1); }
static ok_value n_true(ok* p_ok, uint8_t n, const ok_value* a) { (void)p_ok; (void)n; (void)a; return ok_value_bool(true); }
static ok_value n_false(ok* p_ok, uint8_t n, const ok_value* a) { (void)p_ok; (void)n; (void)a; return ok_value_bool(false); }
static ok_value n_null(ok* p_ok, uint8_t n, const ok_value* a) { (void)p_ok; (void)n; (void)a; return ok_value_null(); }
// raylib key codes so is_key_down can tell them apart
#define KEY_W 87
#define KEY_A 65
#define KEY_S 83
#define KEY_D 68
static ok_value n_is_key_down(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)p_ok; (void)n; s_c_key++;
  if (!ok_value_is_number(a[0])) return ok_value_bool(false);
  long long k = (long long)ok_value_as_number(a[0]);
  if (k == KEY_W) return ok_value_bool(s_key_w);
  if (k == KEY_A) return ok_value_bool(s_key_a);
  if (k == KEY_S) return ok_value_bool(s_key_s);
  if (k == KEY_D) return ok_value_bool(s_key_d);
  return ok_value_bool(false);
}
static ok_value n_is_key_up(ok* p_ok, uint8_t n, const ok_value* a) {
  (void)p_ok; (void)n;
  ok_value d = n_is_key_down(p_ok, n, a);
  return ok_value_bool(!ok_value_as_bool(d));
}
static ok_value n_is_mouse_button_down(ok* p_ok, uint8_t n, const ok_value* a) { (void)p_ok; (void)n; (void)a; return ok_value_bool(false); }
static ok_value n_is_mouse_button_pressed(ok* p_ok, uint8_t n, const ok_value* a) { (void)p_ok; (void)n; (void)a; return ok_value_bool(false); }
static ok_value n_is_mouse_button_released(ok* p_ok, uint8_t n, const ok_value* a) { (void)p_ok; (void)n; (void)a; return ok_value_bool(s_mb_released); }
static ok_value n_is_mouse_button_up(ok* p_ok, uint8_t n, const ok_value* a) { (void)p_ok; (void)n; (void)a; return ok_value_bool(!s_mb_released); }
static ok_value n_is_mouse_on_screen(ok* p_ok, uint8_t n, const ok_value* a) { (void)p_ok; (void)n; (void)a; return ok_value_bool(true); }
static ok_value n_is_pointer_active(ok* p_ok, uint8_t n, const ok_value* a) { (void)p_ok; (void)n; (void)a; return ok_value_bool(false); }

// binder value constants — raylib-faithful where the script branches on them
typedef struct { const char* name; double value; } const_def;
static const const_def CONSTS[] = {
  {"pi", 3.14159265358979},
  {"red", 1}, {"green", 2}, {"blue", 3}, {"white", 4}, {"black", 5}, {"raywhite", 6},
  {"screen_prop_width", 1280}, {"screen_prop_height", 720}, {"screen_prop_scale", 1}, {"screen_prop_filter", 1},
  {"screen_scale_letterbox", 0}, {"screen_scale_preserve_width", 1}, {"screen_scale_preserve_height", 2},
  {"texture_filter_nearest", 0}, {"texture_filter_linear", 1},
  {"mouse_button_left", 0}, {"mouse_button_right", 1}, {"mouse_button_middle", 2},
  {"key_w", KEY_W}, {"key_a", KEY_A}, {"key_s", KEY_S}, {"key_d", KEY_D},
};

typedef struct { const char* name; uint8_t arity; ok_native_fn fn; } native_def;
static const native_def NATIVES[] = {
  {"set_error", 1, n_set_error},
  {"clear_screen", 1, n_clear_screen},
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
  {"create_color", 4, n_create_color},
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
  {"get_logical_mouse_x", 0, n_get_logical_mouse_x},
  {"get_logical_mouse_y", 0, n_get_logical_mouse_y},
  {"get_logical_pointer", 0, n_get_logical_pointer},
  {"get_logical_pointer_x", 0, n_get_logical_pointer_x},
  {"get_logical_pointer_y", 0, n_get_logical_pointer_y},
  {"get_mouse_delta", 0, n_get_mouse_delta},
  {"get_mouse_delta_x", 0, n_zero_num},
  {"get_mouse_delta_y", 0, n_zero_num},
  {"get_mouse_wheel_delta", 0, n_get_mouse_wheel_delta},
  {"get_mouse_wheel_delta_x", 0, n_zero_num},
  {"get_mouse_wheel_delta_y", 0, n_zero_num},
  {"get_screen_prop", 1, n_one_num},
  {"set_screen_prop", 2, n_true},
  {"is_key_down", 1, n_is_key_down},
  {"is_key_up", 1, n_is_key_up},
  {"is_mouse_button_down", 1, n_is_mouse_button_down},
  {"is_mouse_button_pressed", 1, n_is_mouse_button_pressed},
  {"is_mouse_button_released", 1, n_is_mouse_button_released},
  {"is_mouse_button_up", 1, n_is_mouse_button_up},
  {"is_mouse_on_screen", 0, n_is_mouse_on_screen},
  {"is_pointer_active", 0, n_is_pointer_active},
  {"create_array", 0, n_create_array},
  {"array_at", 2, n_array_at},
  {"array_clear", 1, n_array_clear},
  {"array_foreach", 2, n_array_foreach},
  {"array_length", 1, n_array_length},
  {"array_pop", 1, n_array_pop},
  {"array_push", 2, n_array_push},
  {"array_remove", 3, n_array_remove},
  {"array_set", 3, n_array_set},
};

// ---------- diagnostics ----------
static double gnum(ok* p_ok, const char* p_name, double p_def) {
  ok_value v;
  if (!ok_global_get(p_ok, SV(p_name), &v)) return p_def;
  return ok_value_is_number(v) ? ok_value_as_number(v) : p_def;
}
// bools are TAGGED, not numbers — gnum() is blind to them (the "frozen game"
// red herring: the dorg was dead on the death screen while the driver read 0)
static int gbool(ok* p_ok, const char* p_name, int p_def) {
  ok_value v;
  if (!ok_global_get(p_ok, SV(p_name), &v)) return p_def;
  return ok_value_is_bool(v) ? (ok_value_as_bool(v) ? 1 : 0) : p_def;
}
static const char* screen_name(double s) {
  if (s == 0) return "menu";
  if (s == 1) return "game";
  if (s == 2) return "settings";
  if (s == 3) return "credits";
  return "?";
}

static char* load_file_path(const char* path) {
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

// per-frame input script: menu click -> play -> dodge/stand cycles so forks
// actually catch the dorg (death -> retry -> restart exercises game_end,
// forks_end/porgs_end/keys_end, wave reset, save_high_score).
static void drive_input(ok* p_ok) {
  double screen = gnum(p_ok, "g_screen", 0);
  int dead = gbool(p_ok, "g_dorg_dead", 0);

  s_mb_released = false;
  s_key_w = s_key_a = s_key_s = s_key_d = false;

  if (screen == 0) { // menu: hover PLAY, click once at s_click_play_at
    s_pointer_x = 640; s_pointer_y = 350;
    if (g_frame == s_click_play_at) s_mb_released = true;
    return;
  }
  if (screen == 1 && dead) { // death screen: click Retry after 2.5s
    if (s_death_seen_at < 0) s_death_seen_at = g_frame;
    s_pointer_x = 520; s_pointer_y = 446;
    if (g_frame == s_death_seen_at + 150) {
      s_mb_released = true;
      s_deaths++;
    }
    return;
  }
  if (screen == 1) { // playing: 10s dodge circle, then 7s standing duck
    s_death_seen_at = -1;
    long t = g_frame - s_click_play_at;
    if (t < 0) t = 0;
    long cycle = t % 1020; // 17s per dodge+stand cycle
    if (cycle < 600) {
      long phase = (cycle / 60) % 4; // 1s each direction
      if (phase == 0) s_key_w = true;
      else if (phase == 1) s_key_d = true;
      else if (phase == 2) s_key_s = true;
      else s_key_a = true;
    } // else: stand still — let a fork land
    return;
  }
  // settings/credits: click nothing, wander back via menu is fine (shouldn't happen)
  s_pointer_x = 10; s_pointer_y = 10;
}

int main(int argc, char** argv) {
  if (argc < 2) { fprintf(stderr, "usage: repro3 <dorg3.ok>\n"); return 2; }
  const char* fr = getenv("OK_FRAMES");
  if (fr) g_frames_max = atol(fr);
  const char* sd = getenv("OK_SEED");
  seed_rng(sd ? (unsigned int)strtoul(sd, NULL, 10) : 1u);

  char* code = load_file_path(argv[1]);
  ok* p_ok = ok_create((ok_specs){0});
  if (!p_ok) { fprintf(stderr, "ok_create failed\n"); return 4; }

  for (size_t i = 0; i < sizeof NATIVES / sizeof NATIVES[0]; ++i) {
    if (!ok_global_define_native_function(p_ok, SV(NATIVES[i].name), NATIVES[i].arity, NATIVES[i].fn)) {
      fprintf(stderr, "failed to bind: %s\n", NATIVES[i].name); return 3;
    }
  }
  for (size_t i = 0; i < sizeof CONSTS / sizeof CONSTS[0]; ++i) {
    if (!ok_global_define(p_ok, SV(CONSTS[i].name), ok_value_number(CONSTS[i].value), false)) {
      fprintf(stderr, "failed to bind const: %s\n", CONSTS[i].name); return 3;
    }
  }

  ok_source src = {.from = OK_FROM_FILE, .code = code, .path = argv[1]};
  if (ok_run(p_ok, src) != OK) { fprintf(stderr, "[harness] ok_run FAILED\n"); return 5; }
  printf("[harness] ok_run OK\n"); fflush(stdout);

  ok_value init;
  if (!ok_global_get(p_ok, SV("init"), &init)) { fprintf(stderr, "no init\n"); return 6; }
  ok_value result;
  if (!ok_call(p_ok, init, 0, NULL, &result)) { fprintf(stderr, "[harness] ok_call(init) FAILED\n"); return 8; }
  printf("[harness] ok_call(init) OK\n"); fflush(stdout);

  ok_value update_cb, shutdown_cb;
  if (!ok_global_get(p_ok, SV("update"), &update_cb)) { fprintf(stderr, "no update\n"); return 6; }
  bool have_shutdown = ok_global_get(p_ok, SV("shutdown"), &shutdown_cb);

  long ok_frames = 0;
  for (g_frame = 0; g_frame < g_frames_max; ++g_frame) {
    drive_input(p_ok);
    ok_value delta = ok_value_number(1.0 / 60.0);
    ok_value r;
    if (!ok_call(p_ok, update_cb, 1, &delta, &r)) {
      fprintf(stderr, "\n[harness] *** REPRODUCED: update frame %ld FAILED ***\n", g_frame);
      fflush(stderr);
      return 7;
    }
    ok_frames++;
    if (g_frame % 120 == 0) {
      printf("[frame %4ld] screen=%s dead=%d score=%g wave_state=%g dorg=(%g,%g) elapsed=%g | foreach=%ld callbacks=%ld peakforks=%ld deaths=%ld arrays=[",
             g_frame, screen_name(gnum(p_ok, "g_screen", 0)),
             (int)gbool(p_ok, "g_dorg_dead", 0), gnum(p_ok, "g_score", 0),
             gnum(p_ok, "g_wave_state", -1), gnum(p_ok, "g_dorg_pos_x", 0), gnum(p_ok, "g_dorg_pos_y", 0),
             gnum(p_ok, "g_elapsed", -1),
             s_foreach_calls, s_callback_calls, s_forks_pushed, s_deaths);
      for (int i = 0; i < s_array_count; ++i) printf("%zu%s", s_arrays[i]->count, i + 1 < s_array_count ? "," : "");
      printf("] census[clr=%ld color=%ld key=%ld arrlen=%ld rand=%ld foreach=%ld]\n",
             s_c_clrscreen, s_c_color, s_c_key, s_c_arrlen, s_c_rand, s_foreach_calls - s_c_foreach_last);
      s_c_clrscreen = s_c_color = s_c_key = s_c_arrlen = s_c_rand = 0;
      s_c_foreach_last = s_foreach_calls;
      fflush(stdout);
    }
  }

  if (have_shutdown) {
    ok_value sr;
    if (!ok_call(p_ok, shutdown_cb, 0, NULL, &sr)) {
      fprintf(stderr, "[harness] ok_call(shutdown) FAILED\n"); return 9;
    }
    printf("[harness] ok_call(shutdown) OK\n");
  }

  printf("[harness] SOAK CLEAN: %ld frames | foreach=%ld callbacks=%ld peakforks=%ld deaths=%ld\n",
         ok_frames, s_foreach_calls, s_callback_calls, s_forks_pushed, s_deaths);
  fflush(stdout);

  ok_free(p_ok);
  free(code);
  return 0;
}
