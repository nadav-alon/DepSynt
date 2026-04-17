# Performance Analysis: Contextual Input Dependencies

This document analyzes the computational and circuit-level performance of the "Sometimes Dependent" (Contextual Dependency) framework, specifically focusing on the challenges posed by non-deterministic automata (NBA).

## 1. Discovery Complexity: NBA vs. DPA
The complexity of discovering dependencies depends heavily on the determinism of the underlying automaton.

| Feature | Deterministic (DPA) | Non-Deterministic (NBA) |
| :--- | :--- | :--- |
| **Discovery Time** | **Linear** $O(|S| \cdot |V|)$ | **Quadratic** $O(|S|^2 \cdot |V|)$ |
| **Explanation** | Single-pass state check. | Requires exploration of compatible state pairs. |
| **Consistency** | Trivial (only one active state). | Must check if all states in a belief state agree. |

In the current project, discovery is performed on the **raw NBA** of the negated formula. This means the $O(|S|^2)$ compatible-states algorithm is the bottleneck. However, since the tool already performs a quadratic compatibility search, adding the "sometimes" check adds negligible overhead—it simply requires storing the conflict pairs instead of a single global pass/fail.

## 2. Synthesis and Circuit Overhead
A major academic concern with state-based dependencies is the potential for combinatorial explosion.

### Avoiding NP-Completeness
Finding the *maximum* independent set of states for a dependency context is NP-complete. However, our implementation bypasses this by shifting the burden to a **Runtime Monitor**:
*   Instead of searching for optimal sets offline, we synthesize a **Quadratic Conflict Monitor**.
*   The monitor checks the current belief state for any of the $O(|S|^2)$ conflict pairs found during discovery.
*   **Performance**: Synthesis is polynomial $O(|Pairs|)$, and runtime execution is linear with respect to the number of gates.

### Circuit Gate Count
*   **Latch Count**: Remains the same (one per NBA state).
*   **Gate Count**: Increases by $O(|Conflicts|)$ for each sometimes-dependent variable. 
*   **Typical Impact**: For most benchmarks, $|Conflicts| \ll |S|^2$, resulting in a lightweight monitor that does not significantly impact overall circuit delay.

## 3. Storage Efficiency (BDD Sharing)
We store local dependency functions per state. Thanks to **canonical BDD sharing** in the BuDDy library, if multiple states share the same dependency logic (e.g., during the same protocol phase), the memory usage is minimal. The memory footprint scales with the number of *distinct* dependency functions, not the number of states.

## 4. Summary
The "Sometimes Dependent" approach is computationally feasible because it:
1.  Leverages the **existing quadratic compatibility search**.
2.  Bypasses NP-complete graph problems via a **polynomial runtime monitor**.
3.  Uses **BDD sharing** to compress the state-based dependency mappings.
