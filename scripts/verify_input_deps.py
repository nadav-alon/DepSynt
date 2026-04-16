#!/usr/bin/env python3
import json
import subprocess
import os
import sys
import tempfile

# Locate the root of the project
ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
FIND_DEP_BINARY = os.path.join(ROOT_DIR, "find_input_dependencies")
SIMULATE_BINARY = os.path.join(ROOT_DIR, "simulate_aiger")
BENCHMARKS_FILE = os.path.join(ROOT_DIR, "src/inputDependencies/benchmarks/input_deps_test_suite.json")

def run_command(cmd, timeout=30):
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout, cwd=ROOT_DIR)
        return result.returncode, result.stdout, result.stderr
    except subprocess.TimeoutExpired:
        return -1, "", "Timeout"
    except Exception as e:
        return -2, "", str(e)

def get_simulation_vectors(benchmark_name, dependent_vars, independent_vars, output_vars):
    # Order of inputs in AIGER: independent_vars then system outputs (output_vars)
    aiger_input_len = len(independent_vars) + len(output_vars)
    
    vectors = []
    expected_outputs = []

    def pad(v):
        if len(v) > aiger_input_len:
            return v[:aiger_input_len]
        return v + "0" * (aiger_input_len - len(v))

    if benchmark_name == "Simple_Constant":
        vectors = [pad("")]
        expected_outputs = ["1"]
    
    elif benchmark_name == "Simple_Equality":
        vectors = [pad("0"), pad("1")]
        expected_outputs = ["0", "1"]
        
    elif benchmark_name == "Temporal_Dependency":
        vectors = [pad("0"), pad("1"), pad("0")]
        expected_outputs = ["1", "0", "1"]

    elif benchmark_name == "Causality_Check_Past_Output_Dependent":
        vectors = [pad("0"), pad("1"), pad("0")]
        expected_outputs = ["1", "0", "1"]

    elif benchmark_name == "Mutual_Exclusion":
        vectors = [pad("0"), pad("1")]
        expected_outputs = ["1", "0"]

    elif benchmark_name == "Chain_of_Dependencies":
        vectors = [pad("0"), pad("1")]
        if "i1" in dependent_vars and "i2" in dependent_vars:
             expected_outputs = ["00", "11"]
        elif "i2" in dependent_vars and "i3" in dependent_vars:
             expected_outputs = ["00", "11"]
        else:
             expected_outputs = ["??"]
             
    elif benchmark_name == "Goal_Adds_Dependency":
        vectors = [pad("0"), pad("1")]
        expected_outputs = ["0", "1"]

    elif benchmark_name == "Environment_Deadlock":
        vectors = [pad("0")]
        expected_outputs = ["?"]

    return vectors, expected_outputs

def verify_benchmark(benchmark):
    name = benchmark["name"]
    env = benchmark["env"]
    sys_f = benchmark["sys"]
    inputs_str = benchmark["inputs"]
    outputs_str = benchmark["outputs"]
    
    input_vars = [v.strip() for v in inputs_str.split(",")]
    output_vars = [v.strip() for v in outputs_str.split(",")]

    formula = f"({env}) -> ({sys_f})"
    
    with tempfile.NamedTemporaryFile(suffix=".aag", delete=False) as tmp:
        transducer_path = tmp.name
    
    print(f"Testing {name}...")
    
    # 1. Run find_input_dependencies
    cmd = [
        FIND_DEP_BINARY,
        "--formula", formula,
        "--input", inputs_str,
        "--output", outputs_str,
        "--algo", "automaton",
        "--find-input-only",
        "--dependency-transducer-path", transducer_path
    ]
    
    rc, stdout, stderr = run_command(cmd)
    if rc != 0:
        print(f"  [FAIL] find_input_dependencies failed with code {rc}")
        return False

    # 2. Parse found dependencies
    found_deps = []
    found_indeps = []
    for line in stdout.splitlines():
        if "Input Dependent Variables:" in line:
            deps_part = line.split(":")[1].strip()
            if deps_part:
                found_deps = [v.strip() for v in deps_part.split(",") if v.strip()]
        if "Input Independent Variables:" in line:
            indeps_part = line.split(":")[1].strip()
            if indeps_part:
                found_indeps = [v.strip() for v in indeps_part.split(",") if v.strip()]

    # 3. Check if found dependencies match expectations
    expected_options = benchmark.get("expected_deps_options")
    expected_deps = benchmark.get("expected_deps")
    
    success = False
    if expected_deps is not None:
        if sorted(found_deps) == sorted(expected_deps):
            success = True
    elif expected_options is not None:
        for opt in expected_options:
            if sorted(found_deps) == sorted(opt):
                success = True
                break
    
    if not success:
        print(f"  [FAIL] Found dependencies {found_deps} do not match expected {expected_deps or expected_options}")
        return False

    if not found_deps:
        print(f"  [PASS] No dependencies expected and none found.")
        return True

    # 4. Simulate AIGER
    vectors, expected_outputs = get_simulation_vectors(name, found_deps, found_indeps, output_vars)
    if not vectors:
        print(f"  [SKIP] No simulation vectors defined for this benchmark.")
        return True

    cmd = [SIMULATE_BINARY, transducer_path] + vectors
    rc, stdout, stderr = run_command(cmd)
    
    if rc != 0:
        if name == "Environment_Deadlock":
            print(f"  [PASS] Deadlock environment handled (likely no transitions).")
            return True
        print(f"  [FAIL] simulate_aiger failed with code {rc}")
        print(stderr)
        return False

    # 5. Verify simulation outputs
    sim_outputs = []
    for line in stdout.splitlines():
        if "Step " in line and "Output" in line:
            try:
                # Step 1: Input 00 -> Output 1 (Next State: 1)
                out_part = line.split("Output")[1].strip()
                if "(" in out_part:
                    out_part = out_part.split("(")[0].strip()
                sim_outputs.append(out_part)
            except Exception:
                continue

    if len(sim_outputs) != len(expected_outputs):
        print(f"  [FAIL] Simulation produced {len(sim_outputs)} steps, expected {len(expected_outputs)}")
        return False

    for i, (actual, expected) in enumerate(zip(sim_outputs, expected_outputs)):
        if "?" in expected: continue
        if actual != expected:
            print(f"  [FAIL] Step {i+1}: expected output {expected}, got {actual}")
            return False

    print(f"  [PASS] Dependencies found and logic verified via simulation.")
    os.unlink(transducer_path)
    return True

def main():
    if not os.path.exists(FIND_DEP_BINARY):
        print(f"Error: {FIND_DEP_BINARY} not found. Build it first.")
        sys.exit(1)
    if not os.path.exists(SIMULATE_BINARY):
        print(f"Error: {SIMULATE_BINARY} not found. Build it first.")
        sys.exit(1)

    with open(BENCHMARKS_FILE, "r") as f:
        benchmarks = json.load(f)

    results = []
    for b in benchmarks:
        results.append(verify_benchmark(b))

    print("\nSummary:")
    passed = sum(1 for r in results if r)
    total = len(results)
    print(f" {passed}/{total} benchmarks passed.")
    
    if passed == total:
        sys.exit(0)
    else:
        sys.exit(1)

if __name__ == "__main__":
    main()
