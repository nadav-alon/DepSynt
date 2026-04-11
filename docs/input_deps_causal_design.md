# Design: Causality-Aware Input Dependency Discovery

## Goal
Implement a causality-aware input dependency discovery algorithm that correctly identifies environment dependencies while respecting the temporal order of choices (environment chooses inputs $I_t$ before the system chooses outputs $O_t$).

## The Problem: Non-Causality in Swapped-Role Discovery
The standard approach to find input dependencies is:
1. Negate the formula: $\phi_{env} = \neg \phi_{spec}$
2. Swap roles: Inputs become outputs, Outputs become inputs.
3. Apply output dependency discovery to $\phi_{env}$.

However, output dependency discovery assumes that outputs at step $t$ can depend on **all** inputs at step $t$. Since roles are swapped, this allows $I_t$ to depend on $O_t$, which is non-causal.

## Proposed Solutions

### 1. XO Substitution Hack (Baseline)
This hack "shifts" the system outputs to the future from the environment's perspective.
- Transform $\phi_{env}(I, O)$ into $\phi_{env}(I, XO)$.
- This ensures that any dependency discovered for $I_t$ can only involve $O_{t+1}, O_{t+2}, \dots$ (which practically means it won't depend on $O_t$).
- *Pros*: Simple to implement as a formula transformation.
- *Cons*: Might introduce unnecessary $X$ operators that complicate the automaton.

### 2. Causality-Aware Discovery (`FindInputDepsByAutomaton`)
Instead of transforming the formula, we modify the dependency discovery algorithm to ignore system outputs for the current step.
- When testing if an input $i \in I$ is dependent on a set $Y$:
    - The set $Y$ must **not** include any system outputs $o \in O$.
    - **Reset Variables**: System outputs $O$ are explicitly moved from the dependency set to the "reset variables" list.
    - During the BDD-based double-path check, these reset variables are primed in the second path (e.g., $o$ in path 1 vs $o'$ in path 2).
    - This ensures $i_t$ is uniquely determined by $\text{past}$ and other current inputs $I \setminus \{i\}$, independent of the current system's choice $O_t$.

### 3. Causality-Aware Synthesis (`InputDependentsSynthesiser`)
The synthesis of the dependency transducer must also be updated to consume only valid causal inputs.
- **Pruned Inputs**: The AIGER circuit for input dependencies must take **only** other environment inputs (and past state) as its primary inputs. System outputs $O$ are excluded from the AIGER input list.
- **Existential Quantification**: Before generating gates for the partial implementation, system outputs $O$ must be exists-quantified out of the edge conditions: $cond_{causal} = \exists O : cond_{original}$.
- This ensures the resulting strategy for $I$ is mathematically $O$-independent.

## Implementation Plan

### [Component] `src/inputDependencies`
We will implement the new logic in the `src/inputDependencies` directory to avoid modifying the core `src/findDeps` and `src/synthesis` which are geared towards output dependencies.

#### [NEW] `find_input_deps_by_automaton.h / .cpp`
A specialized version of `FindDepsByAutomaton` for input dependencies.
- Exclude system outputs from the dependency set.
- Treat system outputs as variables that are existentialy quantified or allowed to vary.

#### [NEW] `input_dependents_synthesiser.h / .cpp`
A specialized version of `DependentsSynthesiser`.
- Do not include system outputs in AIGER inputs.
- Exists-quantify system outputs from edge BDDs.

#### [MODIFY] `naive_algorithm.cpp`
- Add support for both "Substitution" and "Causal" modes.
- Implement formula transformation for "Substitution" mode using `spot::relabel`.

#### [MODIFY] `inp_dep_utils.cpp / .h`
- Update CLI options to include `--causal` and `--substitution` flags.

## Verification Plan
1. **Unit Tests**:
    - **Non-Causal**: $G(i \leftrightarrow o)$. Verify that the old algorithm finds $i$ as dependent, but the new causal algorithm finds it independent.
    - **Causal (Step 0)**: $i \land G(i \leftrightarrow Xi)$. $i$ is uniquely determined (it's true).
    - **Causal (Step > 0)**: $i \land G(Xi \leftrightarrow o)$. $i$ at step $t+1$ is uniquely determined by $o$ at step $t$.
2. **Benchmarking**: Compare performance and number of dependencies found between the two modes.
