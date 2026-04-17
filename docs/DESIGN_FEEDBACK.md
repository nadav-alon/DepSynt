# Design Feedback: Contextual (Sometimes Dependent) Input Dependencies

I have reviewed the proposed implementation plan for Contextual Input Dependencies. The design is technically sound and addresses the goal of generalizing dependency discovery to specific states of the automaton.

## 1. Technical Assessment

### Completeness
The plan covers all necessary phases: discovery, analysis, synthesis, and verification. The approach of using a **Conflict Graph** based on NBA states is a powerful way to handle the non-determinism of the environment's safety assumptions.

### Soundness
- **Conflict Condition**: The BDD-based conflict check correctly identifies state pairs that could allow non-deterministic choices for a variable $v$.
- **Real-time Monitoring**: The quadratic check over active latches in the AIGER circuit successfully avoids the NP-complete maximum independent set problem by validating only the current execution trace.
- **Belief State Tracking**: The proposal implicitly assumes a power-set construction (one latch per NBA state), which is the standard and correct way to monitor non-deterministic automata.

### Implementability
The suggested function signatures and data structures (`m_conflict_pairs`, `m_state_dep_functions`) are well-aligned with the existing `FindInputDepsByAutomaton` class.

## 2. Recommendations & Critical Items

### [IMPORTANT] Dependency Function Synthesis
The design mentions `m_state_dep_functions` but doesn't explicitly state how these will be combined into the final `out_v_val`.
- **Proposed Logic**: `out_v_val = OR_{s in BeliefState} (Latch_s AND f_s(I_{others}, O))`.
- **Refinement**: Since multiple states in the belief set might be active, and they are guaranteed (if `is_dep` is true) to agree on the value of $v$ for the current $i_{others}$, a simple OR is sufficient. However, if no states are active, the output will be 0. We should ensure this is consistent with the `is_dep` signal.

### [WARNING] Causality and Variable Sets
When checking if $v$ is dependent on $I_{dependency\_set}$, you must ensure that:
1.  $O$ is always quantified out (causality).
2.  Variables in $I \setminus (I_{dependency\_set} \cup \{v\})$ are also quantified out (independence).
- **Action**: Explicitly define $I_{ignored} = O \cup (I \setminus (I_{dependency\_set} \cup \{v\}))$ in the implementation of `check_contextual_deps`.

### [NOTE] Conflict Graph Self-loops
A state $s$ must be considered "conflicting with itself" if it allows both $v=0$ and $v=1$ for the same $i_{others}$.
- **Recommendation**: Ensure $(s, s)$ is added to `m_conflict_pairs` if $s$ is locally non-deterministic for $v$.

## 3. Review Verdict

**Verdict**: **RECOMMENDED FOR APPROVAL**

The design is ready for implementation once the variable quantification logic is explicitly verified.

## 4. Open Questions for the USER
1.  **Gate Conversion**: Will you use the existing `InputDependentsSynthesiser::get_partial_impl` to convert the BDDs in `m_state_dep_functions` to AIGER gates?
2.  **Benchmark Scope**: Is the "Dependency Density" metric intended to be a global average, or a per-variable state-count?
3.  **Verification**: Do we need to verify that the `predicted_value` is correct even when `is_dependent` is false? (Typically, it can be "don't care" in that case).
