# Logic Projection in Spot

When dealing with dependencies, we often need to "forget" certain atomic propositions (APs) from an automaton's transitions.

## 🛠 Simple Projection (Mapping to Unlabeled Transitions)

If you have a `twa_graph_ptr aut` and want to project out a variable `v`:

```cpp
// 1. Identification
int var_id = aut->get_dict()->register_ap(v);

// 2. Mapping
// Spot provides 'remove_ap_from_automaton' in several project-specific utils, 
// but the raw logic is to use bdd_exist for the variable's ID.
for (auto& edge : aut->edges()) {
  edge.cond = bdd_exist(edge.cond, bdd_ithvar(var_id));
}

// 3. Clean up
aut->merge_edges();
```

## ⚠️ Advanced: Using a BDD Map

If you are transforming the transitions to a smaller variable set:

```cpp
unordered_map<int, bdd> bdd_map; 
// ... populate map ...

for (auto& edge : aut->edges()) {
  // Remap the condition based on a precomputed BDD transformation
  edge.cond = bdd_replace(edge.cond, bdd_map);
}
```

## 📉 Impact on Acceptance
Projection creates nondeterminism. In **Deterministic Parity Automata (DPA)**, simply projecting variables can break determinism and the parity condition's meaning if not handled carefully. Always verify if the resulting automaton needs to be re-determinized for synthesis.
