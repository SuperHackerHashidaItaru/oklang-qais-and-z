# spec: maps v2 — open addressing (post-halloween, first item)

replaces oktable entirely. current design (dynamic array per bucket) pays an
allocation per distinct bucket, pointer-chases on every probe, and makes the
hash quality of hashed_string load-bearing in the worst way. the rewrite is
~300 lines and removes a whole failure class.

## layout

```c
typedef struct {
  allocators* alloc;
  uint32_t count;        // live entries
  uint32_t tombstones;   // 0 forever in v1 (backward-shift delete)
  uint32_t capacity;     // power of two, >= 8
  value* keys;           // parallel arrays: probes touch keys only
  value* values;         // values stream in after the hit
} map;                    // slot == NULL_AS_VALUE() means empty
```

- probe: `idx = hash(key) & (cap-1)`; linear forward, wrap with mask.
  linear probing + parallel key array = every probe is one cache line of
  keys; with lf <= 0.7 average probe count stays ~1.5.
- load factor 0.7: grow x2 (reinsert all), shrink at < 0.15 if capacity > 8
  (optional in v1; games leak maps otherwise).
- empty slot representation: keys[i] strictly-null. since strict tags can't
  alias doubles (the lesson of this round — see okvalue.h), null keys are
  unambiguous. storing a null KEY is simply disallowed ("map key can't be
  null"), null VALUES are fine.

## hashing (zero deps)

- numbers (double or int tag): splitmix64 over the raw bits.
- strings: FNV-1a over bytes, **cached in the string header** at creation.
  object_string gains a `uint64_t hash` field computed in
  create_object_string (immutable strings = compute once, forever).
- every other object type (post-classes): pointer -> splitmix64 (identity
  semantics match `==`).
- mix the TAG into the final hash: `h = splitmix64(h ^ tag_bits)` so `1`,
  `true`, `"1"` can never share a bucket path — equality must agree with
  hashing or the map silently lies.

## deletion — backward shift, no tombstones

```
remove slot i:
  j = i
  loop:
    keys[j] = EMPTY
    k = j
    forever:
      k = (k + 1) & mask
      if keys[k] is EMPTY: done
      home = hash(keys[k]) & mask
      // can the entry at k legally move back into the hole at j?
      if "j is cyclically between home and k": break
    keys[j] = keys[k]; values[j] = values[k]
    j = k
```

15 lines, keeps every entry within probe distance of its home forever, no
tombstone counters, no periodic rehash-to-clear. this is the one clever bit
in the whole design; the rest is deliberately boring.

## API

```
map_new/map_deinit/map_get(key) -> value|null+found
map_set(key, value)             // grows if lf > 0.7 AFTER insert
map_remove(key) -> bool
map_count(map)
for let k, let v in map ?       // iteration order: UNSPECIFIED in v1
```

iteration order unspecified v1 (it's slot order — an implementation detail).
python-style insertion order is a v2 upgrade (entries vector + key->index
hash); do not promise it in docs until then.

## what NOT to do (and why)
- robin hood: better worst-case probes, but the deletion story (backward
  shift with distances) and the mental model cost more than they buy at our
  table sizes.
- swiss/meta tables: the win is SIMD group-scanning; portable-C fallback
  makes it merely equal to linear probing. revisit when we have intrinsics
  glue post-classes.
- separate chaining with linked entries: what we have now, minus the array
  allocs. still pointer-chases. no.

## migration
oktable users: globals_store's table (hashed_string -> global_id) and the
compiler's scope tables. migrate globals_store first (it's the one that
matters at runtime), compiler scopes can stay on the old table until the
c++ rewrite eats them anyway.

## tests
insert/get/remove roundtrip at 10k entries (grow path), remove-then-get,
backward-shift correctness property test (random insert/remove/get sequences
vs a reference python dict), string/int/bool key discrimination, null value
storage, lf 0.69 boundary, iteration visits exactly count entries, gc: map
as a root traces keys+values (mark both arrays — same lesson as arrays),
OK_AGGRESSIVE_GC soak.
