# Slippery World: Determinized Variant Experiment

## Motivation

The slippery grid world benchmark is a navigation problem on an N×N grid where the environment is **nondeterministic**: moving in a direction can land the agent 1 or 2 steps ahead (slipping). The goal is for the agent to reach a target cell.

The standard TLSF encoding has:
- **Inputs (env):** `r1..rN`, `c1..cN` (current row/col position)
- **Outputs (agent):** `l`, `r`, `u`, `d` (move direction)

The nondeterminism is expressed as a disjunction, e.g.:
```
c3 -> X(l -> (c2 | c1))
```

The `--extra-equivalences` flag previously added auxiliary variables (e.g. `c3l`, `c2r`) as biconditionals with transition sub-formulas. These were identified as **not meaningfully useful** for dependency finding.

---

## The `--determinize` Flag

**Replaced** `--extra-equivalences` with `--determinize`, which adds a single boolean input variable `slip` that resolves the nondeterminism:

- `slip=0` → agent moves 1 step (normal)
- `slip=1` → agent moves 2 steps (slipped)

The transition formula changes from:
```
c3 -> X(l -> (c2 | c1))
```
to:
```
c3 -> X(l -> ((!slip & c2) | (slip & c1)))
```

Boundary-adjacent cells have no nondeterminism and are unchanged, e.g.:
```
c2 -> X(l -> c1)   -- slip has no effect here
```

**Semantic claim:** With `slip` as an environment input, the current position (`r1..rN`, `c1..cN`) is fully determined by the history of `lrud` + `slip`. The position variables become *functionally dependent* on that history. The row/col variables are kept as inputs (not removed) because the benchmark is meant to test whether the solver can *discover* this dependency.

---

## Experiment: Running the Dependency Finder (3×3 Grid)

Generated the 3×3 TLSF and ran `find_dependencies --algo automaton`:

### Determinized (with `slip`):
| Category | Variables |
|---|---|
| Dependent | `r1`, `c1` |
| Independent | `r2`, `r3`, `c2`, `c3`, `slip` |
| Automaton states | 10 |

### Base (no `slip`):
| Category | Variables |
|---|---|
| Dependent | `r1`, `c1` |
| Independent | `r2`, `r3`, `c2`, `c3` |
| Automaton states | 10 |

---

## Findings

1. **Both variants find the same result:** only `r1` and `c1` are detected as dependent. These are trivially dependent via mutual exclusion: `r1 = !(r2|r3)` and `c1 = !(c2|c3)`.

2. **The deeper dependency is not detected.** The expected result — that *all* position variables (`r2`, `r3`, `c2`, `c3`) are determined by the history of `lrud+slip` — is not found. `slip` itself is also not found dependent.

3. **The automaton is the same size in both cases (10 states)**, suggesting the solver builds the same automaton regardless of the `--determinize` flag. This is the correct number for a 3×3 grid (9 positions + 1 accepting/goal state).

4. **The dependency algorithm appears to test point-in-time functional dependency**, not history-encoded dependency. Even though the automaton states encode position history, the deeper `r2 = f(history of lrud+slip)` relationship is not being surfaced.

---

## Open Questions

- Does the dependency algorithm actually use automaton state history when testing dependencies, or only current-step variable values?
- Should `r2` be detectable as dependent on `{l, r, u, d, slip}` (the minimal set) given the automaton encodes position? If so, why isn't it?
- Is the issue in the algorithm, or in the TLSF encoding (e.g., does the INITIALLY block need to be stronger)?
- Would a different automaton construction (e.g., tracking the full `lrud+slip` history explicitly rather than position) help?
