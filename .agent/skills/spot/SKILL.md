---
name: spot-library
description: A skill for working with the Spot library for LTL, ω-automata, and games.
---

# Spot Library Skill

Spot is a C++ library for LTL, ω-automata, and games. This skill provides instructions on how to effectively use it for synthesis and automata operations.

## 🛠 Core Operations

### 1. LTL Parsing & Manipulation
- **Parse**: Use `spot::parse_formula(f, dict)` to get a `spot::formula`.
- **Atomic Propositions**: Use `f.ap()` to get set of variables.
- **Conversion to NBA**: Use `spot::translator` with `set_type(spot::post_t::BA)` and `set_pref(spot::post_t::Deterministic)`.

### 2. Automata Operations (`spot::twa_graph_ptr`)
- **Product**: `spot::product(aut1, aut2)` - Synchronous product of two automata.
- **Filtering**: `spot::scc_filter_states(aut)` - Removes non-accepting SCCs or unreachable states.
- **Projection**: To "forget" variables $V_{forget}$:
    1. Create a BDD for $V_{forget}$.
    2. Use `bdd_existstep` or re-map labels in the transition condition.
    3. Call `aut->merge_edges()` to simplify after projection.
- **Merge Edges**: `aut->merge_edges()` - Merges parallel transitions that have become equivalent.

### 3. Synthesis & Games
- **Synthesis Info**: `spot::synthesis_info` - Used to configure the synthesis algorithm.
    - `gi.s = spot::synthesis_info::algo::SPLIT_DET;` (Commonly used in this repo).
- **Solving Games**: `spot::synthesis(gi)` where `gi` is `synthesis_info`.
- **AIGER Extraction**: `spot::aig_ptr` represents an AIGER circuit. 
    - Use `spot::twa_to_aiger` to extract a strategy.
    - `aig->as_automaton(false)`: Converts an AIGER strategy back to a `twa_graph_ptr` for model checking.

### 4. Model Checking & Verification
- **Negation of Specification**: Use `spot::formula::Not(f)` to get the "error" property.
- **Intersection Check**: `neg_spec->intersects(strategy_aut)` returns `true` if the strategy violates the specification.
- **Product for Verification**: You can also use `spot::product(strategy_aut, neg_spec)` and check if the result is empty.

### 5. Advanced BDD Operations
For detailed operations on Binary Decision Diagrams (BDDs), transition conditions, and logical quantifiers, refer to the **[buddy-bdd](file:///home/cowclaw/DepSynt-1/.agents/skills/buddy/SKILL.md)** skill.

## ⚠️ Common Pitfalls

- **Acceptance Conditions**: Always check if an operation (like projection) preserves the acceptance condition (`aut->acc()`).
- **Initial States**: When creating a new graph via `make_twa_graph`, don't forget to call `set_init_state()`.
- **Memory Management**: Use `spot::twa_graph_ptr` (shared pointer) to avoid memory leaks.

## 📂 Related Resources
- [Spot Documentation](https://spot.lre.epita.fr/)
- [AIGER Format Specification](http://fmv.jku.at/aiger/)
