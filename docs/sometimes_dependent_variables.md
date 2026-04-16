# Sometimes Dependent Variables in LTL Synthesis

This document summarizes the analysis of input dependencies in the current benchmark suite and the theoretical concept of "Sometimes Dependent" variables.

## 1. Current State: Global Input Dependencies
An **Input Dependency** exists if an environment input variable $v$ is uniquely determined by other input variables $I_{others}$ and past system outputs. In the current implementation, we look for **Global Dependencies**, meaning the variable must be determined for all time.

### HPC Benchmarking Results
*   **Total Tests Analyzed**: 1141
*   **Global Dependencies Found**: 31 (approx. 2.7%)
*   **Summary**: Most LTL specifications are "permissive" and allow the environment many valid actions. Global dependencies are rare and usually occur in structural constraints (e.g., "Exactly one grant must be high").

## 2. Concept: "Sometimes Dependent" Variables
A variable is **Sometimes Dependent** if its value is uniquely determined in certain states of the system's execution, but it remains "free" (independent) in others.

### Key Characteristics
*   **Localized Determinism**: The environment may have choices in most phases of the protocol but be "forced" into a single valid action during specific handshakes or critical sections.
*   **Runtime Predictability**: The system can "predict" the environment's next move only when in a "dependent" state.

## 3. Controller Architecture
To utilize these variables, we can construct a controller (e.g., an AIGER circuit) with an enhanced interface for the variable $v$:

*   **Inputs**: Current latches (tracking the automaton state) and independent variables.
*   **Outputs**:
    *   `v_is_dependent`: A boolean flag indicating if $v$ is currently uniquely determined.
    *   `v_value`: The value $v$ must take if it is dependent.

## 4. Search Algorithm: The Role of Compatible States
Even for "Sometimes Dependent" variables, **Compatible States** are essential.

### Why Compatible States Matter
A variable $v$ is dependent in a specific **Belief State** (a set of possible automaton states the controller might be in). For $v$ to be dependent in a belief state $C = \{s_1, \dots, s_k\}$:
1.  **Local Uniqueness**: For every state $s \in C$, given the other inputs $i_{others}$, there must be only one $v$ that doesn't lead to a losing state for the environment.
2.  **Global Consistency (Agreement)**: All states $s \in C$ must agree on the same value for $v$.

If two states $s_1$ and $s_2$ are compatible (reachable by the same sequence of independent inputs) but disagree on $v$, then $v$ is **not dependent** in that context.

### Finding the Regions
Finding these variables involves:
1.  Constructing the **Belief-State Automaton** (power-set construction).
2.  Iterating through belief states and checking for the **Uniqueness + Agreement** condition.
3.  Labeling the deterministic regions of the state space.

## 5. Potential Benefits
*   **Optimization**: The system strategy can be simplified for the dependent regions.
*   **Monitoring**: The `is_dependent` bit acts as a real-time monitor for environment assumption violations.
*   **Breadth**: This concept likely applies to a much larger percentage of the 1141 benchmarks than the global definition.
