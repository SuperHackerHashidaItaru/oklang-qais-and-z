# README — template (what to include, and WHEN)

The README is v0.0.1-era; the language outgrew it. This is the structure I'd
move to, section by section, with the timing rule for each — so you can write
it in your own sound without wondering what belongs. My own attempt at the
content is `README-DRAFT.md` next to this file: raw material, not a takeover.

## The order that works for a language landing page

| # | Section | When | Rule |
|---|---|---|---|
| 1 | Title + one-line tagline | now, keep yours | one line, no adjectives doing push-ups |
| 2 | Example block | now | MUST run verbatim (`./ok file.ok`). update it every feature round or it lies |
| 3 | Status | now | version, rewrite-era honesty, "intended for my other projects" scope line — that sentence is good, keep it |
| 4 | Features | now | ONLY what exists. planned things live in the roadmap, never here |
| 5 | Building + Running | now | cmake block you already have; add the `--dump` teaser |
| 6 | Tests | now | one block: OK_BUILD_TESTS, ctest, e2e, embedding |
| 7 | Embedding | now, brief | flag it as the section that WILL churn when library-ization lands |
| 8 | CMake options | now | you have it, keep |
| 9 | Roadmap | now | short, ordered, no dates. maps → classes → stdlib → the parked perf menu |
| 10 | Editor support | now | tree-sitter link stays |
| 11 | CI badge | when 08 lands | `[![CI](https://github.com/<mirror>/oklang/actions/workflows/ci.yml/badge.svg)](…)` — image is fetched from GH, works fine in a CB-hosted README |
| 12 | Benchmarks | post-jam, NOT before | numbers move nightly right now; a table would be stale by the time you push it. when it goes in: rig spec, versions, hyperfine flags, and the comparison set (lua/python) |
| 13 | Contributing | first stranger who asks | until then it's noise |
| 14 | FAQ / comparisons vs lua/python | post-1.0 | comparison tables age terribly; earn them |
| 15 | License | already there ✓ | MIT — no action |

## Anti-patterns to avoid (the internet is full of these)

- a features list where half the bullets are plans — the roadmap is the plan,
  the features list is the receipt
- benchmark tables that rot (see #12's timing rule)
- "blazingly fast", "modern", "zero-cost" — let the numbers and the code say it
- an example that doesn't run (compile it before every push of the README)
- apologizing for the project's size — "small" is the design, not a deficit

## The one structural idea worth stealing

Lead with the example, not the philosophy. Someone scanning repos decides in
~15 seconds whether the language looks like something they'd enjoy typing —
the example IS the pitch, the prose is the fine print. Your bird example is
already the right instinct: it shows `let`, `fu`, `if?`, strings, and a
return in five lines while being memorable. Extend it (a loop, an array, a
default arg, one ternary) rather than replacing it.

## Updating cadence

- every feature round: example + features list (one commit, part of the
  feature branch — "docs" don't get a backlog)
- every tag: status section version bump
- post-jam: benchmarks section
- the rest: when it changes
