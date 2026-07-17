# Slippery World Dependency — Investigation Handoff

**Branch:** `slippery-world-dependency-experiment` (off `master`, checkpoint commit `a5e36c12`)
**Goal:** make the grid position variables (`r2,r3,c2,c3`) be discovered as *dependent* by the causal input-dependency finder, and understand why they aren't.

**Bottom line so far:** the original "determinize with `slip`" idea and the encoding fix were both necessary but not sufficient. After fixing the encoding *and* the automaton quality, the finder **still reports only the trivial per-triple mutual-exclusion dependency** (`c3`,`r3`). We traced this to a structural fact about the automaton and are left with one sharp theoretical question (below).

---

## What is established (do not re-derive)

### Theory
1. **Two dependency finders.** `src/findDeps/` (classic; Output or Input variants) and `src/inputDependencies/find_input_deps_by_automaton.cpp` ("causal input dependency", inputs-only candidates).
2. **Causal input dependency notion (intended, confirmed correct):** input `x(t)` may depend on inputs (current+history) and on **past** outputs `output(<t)`, but **not** the current-step output `output(t)` (Mealy: env commits input before/without seeing current output). An automaton edge is labeled with *current-step* APs only; history lives in the state. So priming the current-step outputs in `is_dependent_by_pair_edges` is **correct** — outputs are not "wrongly excluded" (my earlier claim, retracted).
3. **Running on `¬φ` is sound for input deps.** The binary negates the formula (`bins/find_input_dependencies.cpp:45`). For `φ = A → G`, `¬φ = A ∧ ¬G`, so the env assumption `A` is a positive conjunct of `¬φ` and holds in every accepted word. Input dependencies are assumptions ⇒ preserved under negation. (Complement does NOT preserve dependency in general; it only damages the guarantee side — which is exactly why assumption placement matters.)
4. **`A → true` is degenerate:** `¬(A→true)=∅` ⇒ empty automaton ⇒ everything vacuously independent. Minimal tests need a *violable* guarantee.

### Verified by experiment (Diagnostic 1 — the machinery works)
Run on tiny hand-built specs (rebuild first; the on-disk binary had been stale — see "Gotchas"):

| Spec | Meaning | Result | Conclusion |
|---|---|---|---|
| `(!i & G(o<->Xi)) -> G(o)` | `i(t)=o(t-1)`, first pinned | **dependent** | history of *past output* IS captured |
| `G(o<->Xi) -> G(o)` | same, first value free | independent | first-step freedom (correct) |
| `(!i & G(i<->o)) -> G(o)` | `i(t)=o(t)` current output | independent | current-step output correctly **excluded** |

So the compatible-states machinery genuinely detects depth-1 history dependency on a (nondeterministic) NBA, and correctly refuses current-step outputs. **No determinization needed for that.**

### Step A — encoding fix (DONE, kept in working tree)
`scripts/pddl-benchmarks/slippery_world_ltlf_generator.py` `generate_ltlf` now emits
`( (init & G(env)) -> (G(agent) & prec & goal) )` instead of a flat conjunction, so the env
dynamics are an **assumption** (preserved in `¬φ`). This is conceptually required and is kept.
(TLSF path already separated `REQUIRE` vs `ASSERT`/`GUARANTEE`, so it was fine.)

### Run command (3×3 determinized)
```
GEN=$(python3 scripts/pddl-benchmarks/slippery_world_ltlf_generator.py --gridsize 3 --format ltlf --determinize)
FORMULA=$(echo "$GEN" | tail -1)
./find_input_dependencies --algo automaton --find-input-only \
  --formula "$FORMULA" --input 'r1,r2,r3,c1,c2,c3,slip' --output 'l,r,u,d'
```
**NOTE: `--input`/`--output` are COMMA-separated** (`utils.cpp:209`). Space-separated silently becomes one variable.

---

## What we tried and what it showed

Result for the 3×3 determinized spec (after Step A): **dependent = {c3, r3}**, everything else independent.
Inspecting the dependency sets shows `c3` is found dependent on a set still containing `c1,c2` ⇒ it's just `c3 = !(c1|c2)` (one-hot mutual exclusion). Same for `r3`. So this is **only** the trivial per-triple redundancy (exactly one var per row/col triple), NOT the deep "position = f(move+slip history)" dependency.

Things ruled out as the cause:
- **Agent move-uniqueness being a guarantee.** Moving `G(agent)` into the assumption side too → no change.
- **Nondeterminism / Low optimization.** The finder builds `¬φ` via `get_nba_for_synthesis`, which forces `simul=0` (`src/synthesis/synthesis_utils.cpp:67`) ⇒ a **nondeterministic 26-state** automaton (matches `ltl2tgba --low`: 27 states, det=0). `ltl2tgba --high` gives a **deterministic 21-state** automaton (smaller!). Rebuilding the dep-finder to translate `¬φ` fresh at `High` gave the deterministic 21-state automaton (compatible pairs dropped 41→21) — **but the dependency result was identical (`c3,r3` only).** So nondeterminism was a red herring for this problem. (Note `construct_automaton_negation` in `synt_instance.cpp:104` also uses `Low`, but that function is NOT on this binary's path — the path is `get_nba_for_synthesis`.)

### The structural finding (likely the real reason)
Dumping the deterministic 21-state `¬φ` automaton and inspecting each state's out-edges: **only the initial state pins a position** (`r1,c1`, from `init`). Every other state has out-edges carrying *many different current positions* (e.g. state 1 emits `(c1,r1),(c2,r1),(c3,r1),(c2,r2),(c3,r2),(c2,r3),(c3,r3),(c1,r3)...`). Even the states immediately after the pinned init do not pin position (a move like `r` from `c1` leads to `c2` or `c3` depending on the *next* `slip`, so the successor legitimately has two columns).

So the automaton's **states do not encode absolute position** — only the local one-step transition obligations. The dependency-via-automaton method detects `x` as dependent only when the automaton's state structure already determines `x` given the dependency set. Position is determined by the *full* move+slip history, but the residual language of `¬φ` does not refine by position, so the method finds only the memoryless (mutual-exclusion) dependency.

---

## RESOLVED: the method is correct; the issue is what `¬φ` permits

Position is **genuinely not** a full dependency of `¬φ` *as long as agent well-formedness is a guarantee*, because `¬φ` admits rule-violating runs after which position is unconstrained:
- **Zero-move step** (violates agent mutual-exclusion): every transition implication is guarded by a move (`pos -> X(move -> ...)`), so with no move *none* fire ⇒ next position constrained only by one-hot ⇒ free.
- **Wall move** (violates `prec`, e.g. `l` from `c1`): the generator emits no dynamics constraint for that case (the left-action loop skips the boundary column, and `get_same_next_state` doesn't cover it) ⇒ next position free.

Both are *guarantees*, so they are negated/optional in `¬φ` ⇒ position determinism breaks on those runs ⇒ correctly reported as non-dependent. The compatible-states method is **sound**, not incomplete.

### Confirming experiment
Put **init + env + agent-mutex + prec all in the assumption**, leaving only `goal` as guarantee:
`( (init & G(env) & G(agent) & G(prec)) -> F(goal) )`. Result on 3×3 determinized:

```
dependent = {c3, c1, r3}     (was {c3, r3})
c1 dep=True set=['r1','r2','r3','slip','c2']     <-- note: NO c3 in the set
```
`c1` is found dependent on `{c2, slip, rows}` **without `c3`** — pure mutual exclusion would require `c3`, so this is a genuine **history-based** dependency surfacing. So constraining the agent to behave (as assumptions) exposes deeper position dependencies, exactly as predicted.

## THE real (design) question to resolve next

There is a tension between *dependency existence* and *synthesis validity*:
- For position to be a dependency of `¬φ`, agent well-formedness (one move/step, no wall moves) must hold on all of `¬φ` ⇒ must be **assumptions**.
- For the synthesis problem to be meaningful, those must be **system guarantees** (the controller must be forced to play one legal move; if they were assumptions the controller could "cheat" by playing zero moves and vacuously satisfy the spec).

The dependency the user wants ("position = f(move+slip history)") holds *in the play* (both sides obey the rules) but is diluted as a *language property of `¬φ`* by the rule violations `¬φ` permits. Options to explore next session:
1. **Restrict the dependency analysis to "rule-abiding" runs** — i.e. evaluate dependency on the part of `¬φ` where the agent guarantees still hold (intersect `¬φ` with `G(agent) ∧ G(prec)`, or restrict to co-accessible/not-yet-violated states). This is the principled fix and connects to the removed `scc_filter_states` thread. It keeps synthesis semantics intact while exposing the dependency.
2. **Total transition relation** — make the dynamics define the next position for *every* (position, move-set) combination (including zero/multi-move and wall moves), so position stays determined even on rule-violating runs. Changes the benchmark semantics; evaluate whether that's acceptable.
3. **Decide whether the intended dependency is even the right target for `¬φ`-based input-dependency**, vs. something evaluated on `φ`/the game.

Recommendation: try (1) first — it's the smallest change that respects both the dependency notion and synthesis validity.

### Reproduced (2026-06-30) + candidate theoretical resolution
Both results reproduce on the current binary (3×3 determinized):
- Baseline (Step A, agent/prec as **guarantees**): dependent = `{c3, r3}` (mutual exclusion only).
- Option 1 (agent-mutex + prec as **assumptions**): dependent = `{c3, c1, r3}`.
  - Per-variable sets: `c3:{r1,r2,r3,c1,c2,slip}`, `c1:{r1,r2,r3,slip,c2}`, `r3:{r1,r2,slip,c2}`.
  - **`c1`'s set excludes `c3`** — mutual exclusion would need it, so this is genuine history-dependence.

**Candidate resolution (needs the user's theoretical sign-off, NOT yet implemented):**
*Discover on the rule-restricted language, apply to synthesis on the original spec.*
The dependency `position = f(move+slip history)` holds on every **rule-abiding** play. The
synthesized controller is forced (by its guarantees) to play only rule-abiding moves, so the
dependency holds on every play the controller can actually produce. Hence it is legitimate to
*find* the dependency on `¬φ ∩ G(agent) ∩ G(prec)` (Option 1) while the *main* DepSynt synthesis
still runs on the original spec (agent as guarantees). The open risk is whether the
input-dependency *construction* (`InputDependentsSynthesiser`, which builds `nba_with/without_deps`
from the same automaton the dependency was found on) stays sound when discovery uses the
restricted automaton but synthesis uses the original — this is the theoretical point to settle
before coding. Implementation would add an *analysis-only* restriction (an extra constraint formula
intersected into `¬φ` for discovery), leaving default synthesis semantics untouched.

### Scale-test (2026-06-30) — formula-level Option 1, sizes 3×3 / 4×4 / 5×5
Compared baseline (Step A) vs rule-restricted, no code changes:

| size | baseline #dep | restricted #dep | restricted deps |
|---|---|---|---|
| 3×3 | 2 (`c3,r3`) | 3 | `c3`, **`c1`** (set `{r1,r2,r3,slip,c2}`, no `c3`), `r3` |
| 4×4 | 2 (`c4,r4`) | 3 | `c4`, **`c1`** (set `{r1..r4,slip,c2,c3}`, no `c4`), `r4` |
| 5×5 | 2 (`c5,r5`) | 3 | `c5`, **`c1`** (set `{r1..r5,slip,c2,c3,c4}`, no `c5`), `r5` |

**Findings:**
1. **The genuine-history dependency count is FLAT at +1** over baseline for every grid size. It does
   *not* grow with the grid. Rule-restriction reliably surfaces exactly one extra history dependency
   (`c1`), never more.
2. **The analysis automaton is deterministic** (`ltl2tgba --low` and `--high` both give `det=1,
   nondetstates=0`; states = N² : 8/16/25). So nondeterminism is **not** the cap, and determinizing
   the analysis automaton would **not** unlock more dependencies. This rules out that class of fix.
3. **The extra dep is order-fragile.** It appears as `c1` (columns get `{cN, c1}`) but the symmetric
   row `r1` is *not* found — an artifact of the greedy candidate order (dependency set shrinks as vars
   are classified; `cN` is consumed before the rows are tested). So even the "+1" is not a robust
   structural count.

**Conclusion of the scale-test:** formula-level rule-restriction is **insufficient** to capture the
intended "position = f(move+slip history)" dependency at scale. Because the automaton is already
deterministic, the limit is structural — the residual-language states do not individually pin absolute
position (positions with the same "obey-dynamics-and-never-reach-goal" future get merged). Fully
surfacing position-as-history-function would require the automaton to **explicitly track position**
(Option 2: total/explicit position encoding), not merely restrict which runs are considered. This
re-weights the recommendation: Option 1 alone won't scale; revisit Option 2, or accept that the
dependency is a play-level (game) property rather than a language property of any single automaton
(Option 3).

### Cross-check (2026-06-30): classic OUTPUT-dependency tool, all vars as outputs
Hypothesis tested (cheaper than 2A): does the *classic* output-dependency notion (allows current-step
dependence; moves are in the dependency set) catch position where causal-input can't? Built
`find_dependencies`, fed the **flat-conjunction φ** (every constraint asserted — most favorable case),
all position+move(+slip) variables as **outputs**. This tool runs on **φ** (not ¬φ).

3×3 results (identical for `slip`-as-input and literal all-outputs partitions):
```
states=12  #dep=5  dependent = {d, l, c3, c1, r3}
   c3 set={r1,r2,r3,c1,c2,slip,u,r}
   c1 set={r1,r2,r3,slip,u,r,c2}     <-- genuine (no c3), same as input tool
   r3 set={r1,r2,slip,u,r,c2}
   (d,l = move one-hot redundancy)
```
**The position dependencies are the SAME partial set `{c1,c3,r3}` as the causal-input tool.** `c2,r1,r2`
stay independent, and the dependent ones still rest on *other position* vars, not purely on move+slip
history. Switching the dependency **notion** (input→output) and the **partition** (all outputs) does
**not** help.

**Decisive conclusion:** the limitation is the **automaton abstraction, not the dependency notion.**
The φ automaton (12 states for 3×3) still merges distinct cells, so no dependency method reading it can
recover full position-as-history-function. This isolates the fix to making the *automaton* track
position — i.e. **2A (totalize the transition relation)** is the necessary and generic next step.
(2B per-domain trackers ruled out by the user as non-generic; 2C unactionable.)

---

## 2A — IMPLEMENTED & WORKING (2026-06-30): all position vars found dependent

Generator now has a `--totalize` flag (`get_total_dynamics` / `_axis_total_dynamics`). It required
**two** changes, and the experiments show **both are necessary**:

1. **Causal timing.** Rewrote transitions from `(cur -> X(move -> next))` to put the move on the
   assumption side as a *past* output. The old form made position depend on the *contemporaneous*
   `move(t+1)`, which the finder correctly excludes (current-step output).
2. **Totalization.** Defined the next coordinate for **every** `(move-combo, slip)` per axis (columns
   ← `l,r,slip`; rows ← `u,d,slip`): no move / conflicting same-axis move → stay; single move →
   slip-aware step, wall-clamped (bump = stay). So position stays determined even on the rule-violating
   runs `¬φ` admits.

**Final encoding = NON-CLAIRVOYANT (slip INSIDE the X).** `_axis_total_dynamics` now emits
`(cur & move) -> X((!slip & near) | (slip & far))`, i.e. `position(t+1) = f(position(t), move(t),
slip(t+1))`. The slip that resolves a move is revealed **with the result, one step after the move is
committed**, so when the controller picks `move(t)` its outcome `slip(t+1)` is still future — no
"sees-the-slip-coming" clairvoyance. This is the faithful slippery-world game.
An earlier variant put slip OUTSIDE the X (`(cur & move & slip) -> X(next)`): it also finds the full
dependency but is **clairvoyant** (controller reads `slip(t)` before `move(t)`), so it was replaced.
Both give the same dependent set; slip-inside has more states (69 vs 27 for 3×3) because move+slip can't
collapse into one step — that extra structure *is* the non-clairvoyant game.

**Attribution (3×3, input-dep, agent rules kept as GUARANTEES — standard encoding):**

| variant | #dep | dependent |
|---|---|---|
| totalize + OLD (contemporaneous) timing | 2 | `c3,r3` |
| causal timing + NON-total dynamics | 2 | `c3,r3` |
| **causal timing + totalization (2A)** | **6** | **all: `c1,c2,c3,r1,r2,r3`** |

**Scaling (2A, slip-inside / non-clairvoyant, input-dep, agent rules as guarantees):**

| size | states | #dep | result |
|---|---|---|---|
| 3×3 | 69 | 6 | all position vars dependent; only `slip` independent |
| 4×4 | 171 | 8 | all position vars dependent; only `slip` independent |
| 5×5 | 317 | 10 | all position vars dependent; only `slip` independent |

(The clairvoyant slip-outside variant was 27 / 48 / 75 states for 3/4/5 — smaller, same dependents.
slip-inside is ~3–4× larger because move+slip cannot collapse into one step.)

Dependency sets are the clean greedy chain (3×3): `r1←{slip}`, `r2←{r1,slip}`, `r3←{r1,r2,slip}`,
`c1←{r1,r2,r3,slip}`, … i.e. **position = f(move+slip history)**, with `slip` (the genuine
environmental nondeterminism) the sole independent input. This is the intended dependency, and it is
**generic** (no per-domain tracker): the change is purely in how the benchmark encodes dynamics.

Run command:
```
GEN=$(python3 scripts/pddl-benchmarks/slippery_world_ltlf_generator.py --gridsize 3 --format ltlf --totalize)
FORMULA=$(echo "$GEN" | tail -1)
./find_input_dependencies --algo automaton --find-input-only \
  --formula "$FORMULA" --input 'r1,r2,r3,c1,c2,c3,slip' --output 'l,r,u,d'
```

**Caveats / open follow-ups:**
- Totalization changes benchmark semantics (illegal/zero/multi moves now have defined outcomes:
  bump-stay / no-net-move). Need to confirm the totalized spec is still **realizable** (separate from
  dependency discovery — not yet checked).
- The causal-timing convention differs from the original non-total benchmark; decide whether to apply
  it to the non-total path too, or keep `--totalize` as the dependency-friendly variant.

---

## GENERAL THEORY — conditions for an input dependency to exist & be found

This is the main transferable result; the slippery world was the worked example.

**Principle.** An input dependency is *a deterministic law, in the environment-ASSUMPTION part of the
spec, expressing one input as a causal function of (other current inputs, past inputs, past outputs),
holding no matter how the controller behaves.* Equivalently: dependencies **partition the inputs into
"free environment choices" (independent) and "deterministic consequences" (dependent)**. The analysis
runs on `¬φ = A ∧ ¬G`; since `¬G` frees the outputs, a determination that only holds when the controller
plays legally is NOT a robust input dependency.

**Conditions** (1–2 = existence; 3–4 = the method sees it on `¬φ`; 5 = mechanism):
1. **Determinacy.** A law must pin `x` to one value given its determiners. Genuine nondeterminism
   (`X(l → (c_{i-1} ∨ c_{i-2}))`) makes `x` a *relation* → no dependency. Fix: expose the hidden choice
   as an explicit variable (`slip`).
2. **Causality.** `x(t)=g(other current inputs, strictly-past inputs, strictly-past outputs)`. May NOT
   reference a current-step output (Mealy: env commits `x(t)` before seeing `output(t)`).
3. **Assumption placement.** The *determining law for x* (NOT the agent rules) sits in the assumptions,
   so it survives as a positive conjunct of `¬φ`.
4. **Totality over what `¬φ` frees.** The law must define `x(t)` for **every** output assignment,
   including the illegal ones the guarantees forbid. Constructive form (see below).
5. **Finite sufficient statistic.** The needed history must be ω-regular; given 1–4 the total
   deterministic law *forces* the automaton to track that statistic (else position is "merged away",
   the pre-2A failure).

**Constructive totalization (condition 4 made mechanical).**
`¬one-hot ∨ ¬prec  =  ¬(one-hot ∧ prec)  =  ¬W`, where `W` = the controller's **output well-formedness
guarantees** (`one-hot` = move-mutex, `prec` = wall preconditions). The legal-move rules cover exactly
the `W` region; `¬φ` unlocks exactly the `¬W` region. So the required closure is:

> **`¬W → X(state unchanged)`** — "if the controller's outputs are not well-formed, the state stays."

Mechanically: for each output well-formedness guarantee, add the assumption `¬(guarantee) → no-op`.
*Verified:* `¬one-hot→stay` alone (misses wall moves) → only `{c3,r3}`; adding wall-bump (= `¬prec→stay`)
→ all 6. `(¬one-hot ∨ ¬prec)→stay` → all 6 (27 states), identical to per-axis 2A.

**Agent-rules-vs-dynamics clarification (resolves the earlier "tension").** Keep the agent's
*obligations* (`one-hot`, `prec`) in the **guarantees** (so the controller can't misbehave during
synthesis). *Separately*, make the environment's *response* total in the **assumptions**, covering the
forbidden actions with a no-op. These are two complementary clauses about different variables, not the
same fact in two places. The illegal-action branches are dead code for any valid controller but live in
`¬φ` — that asymmetry is the whole trick. 2A keeps agent rules as guarantees and still finds everything.

**Corollary — deterministic+total ⟹ dependencies.** Determinism *is* the redundancy: every non-free
input is a function of (free inputs, past outputs, history) = a dependency; the free inputs are the
independents; `#deps = #inputs − #true degrees of freedom`. The only dependency-free deterministic-total
env is the degenerate all-free one (= nondeterministic). Caveats are about *detection* (causality;
finite memory; totality on `¬φ`), not existence.

**Generalization — programmatic "determinizing variables".** For a nondeterministic-but-total spec, add
fresh **input** selector bits (`⌈log₂ branching⌉`) that index each transition's successor → spec becomes
deterministic-modulo-selectors → every original input becomes dependent, selectors are the minimal
independent set (= env entropy). KEY: this is **alphabet refinement** (relabel edges, same #states),
NOT powerset determinization — so it avoids the double-exponential. Can be done at the formula level.
**Placement rule (non-clairvoyance):** introduce a selector at the step where the *outcome* materializes
(INSIDE the `X`, with the result), not where the *action* is chosen — a selector resolving an
already-committed action leaks nothing; one read before its action is clairvoyant. (This is exactly the
slip-inside vs slip-outside distinction.) Discovery is sound either way; *synthesis* validity needs the
slip-inside placement. **Open:** build this selector-introduction pass as an automatic formula transform
and test on a non-`slip` benchmark.

---

## Gotchas / environment
- **Rebuild before testing.** On-disk `find_input_dependencies` was older than the source and `*** stack smashing detected ***` on every input; `make find_input_dependencies` fixed it. Build = `cmake .` then `make <target>` (see `build.sh`, `Taskfile.yml`).
- Spot CLI tools: `/home/cowclaw/ltlf-po-benchmarks/spot/local/bin/{ltl2tgba,autfilt}` (no python `spot` binding).
- JSON output: only the line starting with `{` is JSON; filter with `grep '^{'`.

## Working-tree state (uncommitted)
- `scripts/pddl-benchmarks/slippery_world_ltlf_generator.py` — modified (keep):
  - `--determinize` (`slip`) + **Step A assumption→guarantee restructure**.
  - **`--totalize` (2A)**: `_axis_total_dynamics` / `get_total_dynamics`, now the **non-clairvoyant
    slip-INSIDE** form. This is THE working knob: `--gridsize N --format ltlf --totalize` → all
    position vars found dependent, `slip` the only independent input.
- `src/inputDependencies/naive_algorithm.cpp` — removed `scc_filter_states` on composed synthesis automaton (in checkpoint; relevance to the open question's "secondary thread").
- `docs/slippery_world_determinize_experiment.md` — earlier write-up; its "Finding #4" (point-in-time vs history) is **superseded** by this doc.
- `docs/slippery_world_dependency_handoff.md` — this file (do NOT commit — per user).
- `find_input_dependencies` + `find_dependencies` binaries are built and current.
- No `src`/`bins` C++ changes for 2A — it is **purely a benchmark-encoding (generator) change**.

## Status & next steps
- **DONE:** position fully discoverable as input-dependent (all 2N vars), generic encoding change only,
  non-clairvoyant, agent rules stay guarantees. General theory section above captures the conditions.
- **Next (in priority order):**
  1. Confirm the totalized spec is still **realizable** as a synthesis problem (dependency discovery is
     proven; synthesis validity is not yet checked).
  2. Build the **automatic determinizing-variable pass** (formula-level selector introduction, slip-inside
     placement) and test on a non-`slip` benchmark — the generalization of `--totalize`.
  3. Decide whether DepSynt's input-dependency *synthesis* path correctly consumes these dependencies
     (remove dependent inputs, compute them from selectors + strategy) preserving observation order.
