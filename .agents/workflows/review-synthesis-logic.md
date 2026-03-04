---
description: An overview of LTL synthesis logic and workflow, for reviewing implementations 
---

The general process in solving LTL synthesis contains the following steps:
1. Generating the NBA (nondeterministic buchi automata) for the LTL formula
2. Determinizing to get the DPA (deterministic parity automata)
3. Transforming to a DPG (deterministic parity game) 
4. Solving the game to get the strategy

## Review Steps
1.  **Architecture Check**:
    - Verify that all changes are confined to `src/inputDependencies/`, except for build files, Taskfile.yml, and other such files.
    - Ensure no changes were made to the core logic of the original "Output Dependencies" project.

2.  **Logic Decomposition**:
    - Review the newly changed synthesis files, make sure they follow the general process, and any deviation from it makes sense.
    - If the algorithm splits dependant variables, make sure the way they are merged back in (if output merge into the strategy, if input prunning) correctly.

3.  **Verification & Model Checking**:
    - Check if the `apply_model_checking` flag is respected.
    - Verify that the final `aig_ptr` strategy is validated against the original environment and system formulas.

4.  **Execution & Benchmarking**:
    - Run the synthesis tasks using `Taskfile.yml` or the main binary with the `--verbose` flag to trace the decomposition steps. If no task in Taskfile.yml fits the new case, create one.
