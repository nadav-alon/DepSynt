---
name: buddy-bdd
description: A skill for working with the BuDDy Binary Decision Diagram (BDD) library, used by Spot for automata edge conditions.
---

# BuDDy BDD Skill

This skill provides a reference for Binary Decision Diagram (BDD) operations using the BuDDy library, which is the underlying engine for Spot's transition conditions.

## 🛠 Basic BDD Operations

| Operation | Function | Description |
|-----------|----------|-------------|
| **Logical AND** | `bdd_and(a, b)` | $a \land b$ |
| **Logical OR** | `bdd_or(a, b)` | $a \lor b$ |
| **Logical NOT** | `bdd_not(a)` | $\neg a$ |
| **Logical XOR** | `bdd_xor(a, b)` | $a \oplus b$ |
| **Logical IMPLY** | `bdd_imp(a, b)` | $a \to b$ |

### Constants
- `bddtrue`: Represents the constant true Boolean value (1).
- `bddfalse`: Represents the constant false Boolean value (0).

## 📊 Variable Management

- **Get Variable by ID**: `bdd_ithvar(id)` - Returns a BDD representing the $i$-th variable.
- **Get Negated Variable**: `bdd_nithvar(id)` - Returns a BDD representing the negation of the $i$-th variable.
- **Node Breakdown**:
    - `bdd_var(b)`: Returns the variable ID at the root of BDD $b$.
    - `bdd_low(b)`: Returns the "low" branch (where the root variable is false).
    - `bdd_high(b)`: Returns the "high" branch (where the root variable is true).

## 🔍 Quantifiers & Projection

- **Existential Quantification**: `bdd_exist(b, var_set)` - $\exists x \in var\_set . b$
- **Universal Quantification**: `bdd_forall(b, var_set)` - $\forall x \in var\_set . b$
- **Variable Sets**: Create a "variable set" BDD by AND-ing multiple `bdd_ithvar(id)` results together.

## 🔄 Mapping & Substitution

- **Replacement**: `bdd_replace(b, pair_map)` - Replaces variables in $b$ according to a map.
- **Variable Alignment**: Useful when moving an automaton from one dictionary to another.

## 🔗 Spot Integration

- **BDD to Formula**: `spot::bdd_to_formula(b, dict)` 
- **Variable ID Discovery**: `dict->register_ap(ap_name)` returns the BDD ID for an atomic proposition.

## ⚠️ Common Pitfalls

- **BDD Initialization**: In standalone programs, `bdd_init()` must be called. In Spot programs, the `bdd_dict` usually handles initialization.
- **Variable Ordering**: BuDDy's performance is highly sensitive to variable ordering.
- **Reference Counting**: Be careful with `bdd_addref` and `bdd_delref` when manually managing BDD forest life cycles.
