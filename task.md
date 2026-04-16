# Task: input-deps-causal-find

## Description
Implement a causality-aware input dependency discovery algorithm (`FindInputDepsByAutomaton`) that ignores current-step output values to correctly identify environment dependencies without $XO$ substitution hacks.

The naive algorithm, both projection based and non projection based, should be updated to have a flag that decides if to use the XO substitution or the casual fix to FindInputDepsByAutomaton, to compare.

As an addition, the dependantSynthesis likely needs to be updated as well, as it is geared toward output dependencies. It should probably have a copy as well, and that copy should be used by both substitution based and casual based dependencies.

## Status
- [x] Design Drafted
- [x] Design Approved
- [x] Implementation Started
- [ ] Phase 1: XO Substitution Baseline
- [ ] Phase 2: FindInputDepsByAutomaton
- [ ] Phase 3: InputDependentsSynthesiser
- [ ] Phase 4: Integration and CLI Flags
- [ ] Verification and Benchmarking
