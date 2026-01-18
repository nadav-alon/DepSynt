instructions for input dependency finder:

1. Filter benchmarks for ones that have an env specification (environment constraints)
2. Use find_dependencies on just the ltl formula that represents the environment constraints
    - Note that if find_dependencies only works on output, reverse input and output for this section. In general though the input variables are the ones we expect to have dependencies for this formula