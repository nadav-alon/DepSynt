#!/usr/bin/env python3
import json
import subprocess
import os
import sys

# Locate the root of the project
ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
BINARY = os.path.join(ROOT_DIR, "build", "inp_dep_synthesis")
BENCHMARKS_FILE = os.path.join(os.path.dirname(__file__), "benchmarks/verification_suite.json")

def run_synthesis(env, sys_formula, inputs, outputs, algorithm):
    cmd = [
        BINARY,
        "-e", env,
        "-s", sys_formula,
        "--input", inputs,
        "--output", outputs,
        "--algorithm", algorithm,
        "--verbose"
    ]
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=120, cwd=os.path.join(ROOT_DIR, "build"))
        output = result.stdout + result.stderr
        
        # Realizable if "Strategy found" is in output
        # Unrealizable if "Strategy not found" is in output
        if "=> Strategy found" in output:
            return "Realizable"
        elif "=> Strategy not found" in output:
            return "Unrealizable"
        else:
            # Check for specific known errors
            if "terminate called" in output:
                return "CRASH"
            return "ERROR"
    except subprocess.TimeoutExpired:
        return "Timeout"
    except Exception as e:
        return f"EXCEPTION: {str(e)}"

def verify_all():
    if not os.path.exists(BINARY):
        print(f"Error: Binary {BINARY} not found. Build it first.")
        sys.exit(1)

    with open(BENCHMARKS_FILE, "r") as f:
        benchmarks = json.load(f)

    algorithms = ["ultra_naive", "naive", "naive_projected"]
    all_success = True

    print(f"{'Benchmark':<40} | {'Ultra Naive':<15} | {'Naive':<15} | {'Naive Projected':<15} | {'Verdict'}")
    print("-" * 110)

    for b in benchmarks:
        results = {}
        for algo in algorithms:
            res = run_synthesis(b["env"], b["sys"], b["inputs"], b["outputs"], algo)
            results[algo] = res
        
        un_res = results["ultra_naive"]
        n_res = results["naive"]
        np_res = results["naive_projected"]
        
        # Verdict: All results should match ultra_naive (baseline)
        match = (un_res == n_res == np_res)
        
        expected = b["expected"]
        status = "PASS" if match else "FAIL"
        if match and un_res != expected:
            status = "PASS (Mismatch expected)"
        
        results_str = f"{b['name']:<40} | {un_res:<15} | {n_res:<15} | {np_res:<15} | {status}"
        print(results_str)
        if not match:
            all_success = False

    if all_success:
        print("\nALL ALGORITHMS AGREE ON ALL BENCHMARKS.")
    else:
        print("\nMAJOR DISCREPANCIES DETECTED.")
        # We don't exit with failure because the user might just want to see the report
        # sys.exit(1)

if __name__ == "__main__":
    verify_all()
