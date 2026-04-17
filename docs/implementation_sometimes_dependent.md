# Implementation Plan: Contextual (Sometimes Dependent) Input Dependencies

This document outlines the technical steps to implement **Contextual Input Dependency** discovery and synthesis, generalizing the current global dependency framework.

## 1. Objective
A variable is **Contextually Dependent** if there exists at least one state in the automaton where its value is uniquely determined by other input variables (via environment safety assumptions). The goal is to:
1.  Identify these variables and the specific states where they are deterministic.
2.  Synthesize a monitor that outputs `is_dependent` and `predicted_value` in real-time.

## 2. Discovery Algorithm (NBA-Aware)
Since the tool operates on the **raw NBA** of the negated formula $\neg \varphi$, the controller does not know exactly which state is active. It only knows the **Belief State** (a set of possibly active states).

### The Conflict Graph Model
Instead of a global "pass/fail," we build a **Conflict Graph** $G_{conflict}(V)$ for each variable:
1.  **Nodes**: States of the NBA.
2.  **Conflict Edges**: An edge exists between $(s_i, s_j)$ if they are **compatible** but allow different values for $v$ under the same $i_{others}$.
    *   BDD Condition: $(\exists I_{ignored} . (T_{s_i} \wedge v)) \wedge (\exists I_{ignored} . (T_{s_j} \wedge \neg v)) \neq \text{false}$
3.  **Contextual Dependency**: $v$ is dependent at time $t$ if the current belief state $C_t$ is an **Independent Set** in the conflict graph.

### New Data Structures
*   `std::vector<std::pair<unsigned, unsigned>> m_conflict_pairs`: List of state pairs that form the conflict edges.
*   `std::map<unsigned, bdd> m_state_dep_functions`: Local dependency function per state.

## 3. Synthesis and AIGER Generation
While finding the *maximum* independent set is NP-complete, implementing the **runtime check** in the circuit is polynomial.

### Output Interface
For each sometimes-dependent variable $v$:
1.  `out_v_is_dep`: Boolean gate (True if no two active states in the belief-state have a conflict).
2.  `out_v_val`: Predicted value (Logical OR of values from active states).

### Monitoring Logic
The `is_dependent` bit is synthesized as a quadratic check over the current latches $(L_1, \dots, L_n)$:
```cpp
// v is dependent if NO two active latches have a conflict
Gate is_dep = m_aiger->aig_not(m_aiger->aig_or({
    m_aiger->aig_and(L_i, L_j) for each (i, j) in m_conflict_pairs
}));
```
This bypasses the NP-complete search by performing a real-time validation of the current execution trace.

## 4. CLI Extensions
Modify `bins/find_input_dependencies.cpp` to support:
*   `--contextual`: Enable contextual dependency discovery.
*   `--report-density`: Output the percentage of states where each variable is dependent.

## 5. Implementation Steps
1.  **Step 1 (Core)**: Implement `FindInputDepsByAutomaton::check_contextual_deps()`.
2.  **Step 2 (Analysis)**: Create a new benchmark task to measure the "Dependency Density" across the 1141 test cases.
3.  **Step 3 (Synthesis)**: Update `InputDependentsSynthesiser` to add the `is_dep` and `val` output bits.
4.  **Step 4 (Verification)**: Update the Python verification scripts to handle the dual-output monitor format.
