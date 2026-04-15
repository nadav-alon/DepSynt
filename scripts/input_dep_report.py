#!/usr/bin/env python3
import subprocess
import json
import argparse
import os
from glob import glob
from pathlib import Path
import sys

def parse_benchmark_file(filepath):
    try:
        with open(filepath, 'r') as f:
            lines = [l.strip() for l in f.readlines() if l.strip()]
            if len(lines) < 5:
                return None
            
            # Check for labeled format (Name: , Formula: , etc.)
            d = {}
            is_labeled = False
            for line in lines:
                if ':' in line:
                    parts = line.split(':', 1)
                    key = parts[0].strip().lower()
                    val = parts[1].strip()
                    if key in ['name', 'formula', 'input', 'output', 'family', 'id', 'env', 'sys']:
                        d[key] = val
                        is_labeled = True
            
            if is_labeled and 'formula' in d:
                # Clean up comma-separated lists
                inputs = ','.join([v.strip() for v in d.get('input', '').split(',')])
                outputs = ','.join([v.strip() for v in d.get('output', '').split(',')])
                return {
                    'name': d.get('name', Path(filepath).stem),
                    'family': d.get('family', 'Unknown'),
                    'formula': d.get('formula', ''),
                    'env_formula': d.get('env_formula', d.get('env', '')),
                    'sys_formula': d.get('sys_formula', d.get('sys', '')),
                    'inputs': inputs,
                    'outputs': outputs
                }
            
            if len(lines) >= 8:
                return {
                    'id': lines[0],
                    'name': lines[1],
                    'family': lines[2],
                    'formula': lines[3],
                    'env_formula': lines[4],
                    'sys_formula': lines[5],
                    'inputs': lines[6],
                    'outputs': lines[7]
                }
            elif len(lines) >= 6:
                return {
                    'id': lines[0],
                    'name': lines[1],
                    'family': lines[2],
                    'formula': lines[3],
                    'env_formula': '',
                    'sys_formula': '',
                    'inputs': lines[4],
                    'outputs': lines[5]
                }
            return None
    except Exception:
        return None

import re
def add_turn_delay(formula, variables):
    res = formula
    for var in variables.split(','):
        var = var.strip()
        if var:
            res = re.sub(r'\b' + re.escape(var) + r'\b', f'(X {var})', res)
    return res

def run_find_deps(benchmark, tool_path="./find_dependencies"):
    env_form = benchmark.get('env_formula', '')
    sys_form = benchmark.get('sys_formula', '')
    formula = benchmark.get('formula', '')
    
    if "find_input_dependencies" in tool_path:
        # The new tool handles input dependencies natively and is causality-aware.
        # It expects the original spec (env -> sys) or just the formula.
        # It will handle negation internally if it follows the updated bins/find_input_dependencies.cpp logic.
        cmd = [
            tool_path,
            "--formula", formula if formula else f"({env_form}) -> ({sys_form})",
            "--input", benchmark['inputs'],
            "--output", benchmark['outputs'],
            "--algo", "automaton"
        ]
    else:
        # Legacy mode using find_dependencies (which finds output dependencies)
        # This requires swapping inputs/outputs and adding a turn delay to fake causality.
        if not env_form:
            print(f"Warning: No env_formula found for {benchmark['name']}")
        
        # Add X to every output variable of Gamma to add a 1 turn delay.
        adjusted_env = add_turn_delay(env_form, benchmark['inputs'])
        
        cmd = [
            tool_path,
            "--formula", adjusted_env,
            "--input", benchmark['outputs'],  # Swapped
            "--output", benchmark['inputs'],  # Swapped
            "--algo", "automaton"
        ]
    
    try:
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0:
            print(f"Error: Tool exited with code {result.returncode} for {benchmark['name']}")
            if result.stderr:
                print(f"Stderr: {result.stderr.strip()}")
            return None
            
        output = result.stdout.strip()
        json_start = output.find('{')
        if json_start != -1:
            json_str = output[json_start:]
            # find_dependencies might print multiple things, we want the last JSON block if possible
            # But usually it's just one dump_measures call at the end.
            try:
                return json.loads(json_str)
            except json.JSONDecodeError:
                # Try to find the last occurrence of '{' and see if that works
                json_start = output.rfind('{')
                json_str = output[json_start:]
                return json.loads(json_str)
    except Exception as e:
        print(f"Error running tool for {benchmark['name']}: {e}")
    return None

def main():
    parser = argparse.ArgumentParser(description="Report input dependencies for benchmarks.")
    parser.add_argument("benchmarks", nargs="+", help="Benchmark files or directories")
    parser.add_argument("--tool", default="./find_dependencies", help="Path to find_dependencies binary")
    args = parser.parse_args()

    if not os.path.exists(args.tool):
        print(f"Error: Binary {args.tool} not found. please build it first (e.g., 'make find_dependencies')")
        sys.exit(1)

    files = []
    for path in args.benchmarks:
        if os.path.isdir(path):
            files.extend(sorted(glob(os.path.join(path, "*.txt"))))
        else:
            files.append(path)

    if not files:
        print("No benchmark files found.")
        return

    for filepath in files:
        benchmarks = []
        if filepath.endswith('.json'):
            try:
                with open(filepath, 'r') as f:
                    data = json.load(f)
                    for entry in data:
                        benchmarks.append({
                            'name': entry.get('name', 'unknown'),
                            'family': entry.get('family', 'verification_suite'),
                            'formula': entry.get('formula', ''),
                            'env_formula': entry.get('env_formula', entry.get('env', '')),
                            'sys_formula': entry.get('sys_formula', entry.get('sys', '')),
                            'inputs': entry.get('inputs', ''),
                            'outputs': entry.get('outputs', ''),
                            'expected_deps': entry.get('expected_deps', None),
                            'expected_deps_options': entry.get('expected_deps_options', None)
                        })
            except Exception as e:
                print(f"Error reading JSON {filepath}: {e}")
                continue
        else:
            benchmark = parse_benchmark_file(filepath)
            if benchmark:
                benchmarks.append(benchmark)

        total_tests = 0
        passed_tests = 0
        failed_tests = []

        for benchmark in benchmarks:
            total_tests += 1
            print(f"=== Benchmark: {benchmark['name']} ({benchmark['family']}) ===")
            data = run_find_deps(benchmark, args.tool)
            if not data:
                print("Failed to get dependency data.\n")
                failed_tests.append(benchmark['name'])
                continue
            
            deps = data.get("dependency", {}).get("tested_dependencies", [])
            input_deps = [d for d in deps if d.get("is_dependent")]
            found_dep_names = [d['name'] for d in input_deps]
            
            print(f"Amount of input dependencies: {len(input_deps)}")
            if input_deps:
                print("Dependencies:")
                for d in input_deps:
                    dep_set = d.get("tested_dependency_set", [])
                    label = "nothing (constant)" if d.get('is_constant') else "nothing (history/outputs)"
                    print(f"  - Input '{d['name']}' depends on: {', '.join(dep_set) if dep_set else label}")
            else:
                print("No input dependencies found.")
            
            expected_options = benchmark.get('expected_deps_options')
            expected_single = benchmark.get('expected_deps')
            
            if expected_options is not None:
                passed = any(set(found_dep_names) == set(opt) for opt in expected_options)
                if passed:
                    print("\033[92m[PASS]\033[0m")
                    passed_tests += 1
                else:
                    print(f"\033[91m[FAIL]\033[0m (Found: {found_dep_names}, Expected one of: {expected_options})")
                    failed_tests.append(benchmark['name'])
            elif expected_single is not None:
                passed = set(found_dep_names) == set(expected_single)
                if passed:
                    print("\033[92m[PASS]\033[0m")
                    passed_tests += 1
                else:
                    print(f"\033[91m[FAIL]\033[0m (Found: {found_dep_names}, Expected: {expected_single})")
                    failed_tests.append(benchmark['name'])
            else:
                print("[INFO] No expectations defined.")
                passed_tests += 1
            print()

        print(f"Summary: {passed_tests}/{total_tests} passed.")
        if failed_tests:
            print(f"Failed benchmarks: {', '.join(failed_tests)}")
            sys.exit(1)

if __name__ == "__main__":
    main()
