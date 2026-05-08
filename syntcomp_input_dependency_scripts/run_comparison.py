import os
import subprocess
import json
import csv
import time

BENCHMARKS_FILE = "input_dep_benchmarks.txt"
BENCHMARKS_DIR = "scripts/benchmarks-ltl"
RESULTS_DIR = "benchmark_results"
ROOT_DIR = "/home/cowclaw/DepSynt-1"
LD_LIBRARY_PATH = f"{ROOT_DIR}/libs/abc:{ROOT_DIR}/libs/spot/spot/.libs"

def run_cmd(cmd, cwd="build"):
    env = os.environ.copy()
    env["LD_LIBRARY_PATH"] = LD_LIBRARY_PATH
    try:
        result = subprocess.run(cmd, shell=True, env=env, cwd=cwd, capture_output=True, text=True, timeout=600)
        return result.stdout, result.stderr, result.returncode
    except subprocess.TimeoutExpired:
        return "", "TIMEOUT", 124
    except Exception as e:
        return "", str(e), 1

def parse_benchmark(name):
    path = os.path.join(ROOT_DIR, BENCHMARKS_DIR, name + ".txt")
    with open(path, "r") as f:
        lines = f.readlines()
        data = {}
        for line in lines:
            if line.startswith("Formula:"):
                data["formula"] = line.replace("Formula:", "").strip()
            elif line.startswith("Input:"):
                data["input"] = line.replace("Input:", "").strip()
            elif line.startswith("Output:"):
                data["output"] = line.replace("Output:", "").strip()
        return data

def main():
    results_abs_dir = os.path.join(ROOT_DIR, RESULTS_DIR)
    if not os.path.exists(results_abs_dir):
        os.makedirs(results_abs_dir)

    benchmarks_path = os.path.join(ROOT_DIR, BENCHMARKS_FILE)
    with open(benchmarks_path, "r") as f:
        benchmark_names = [line.strip() for line in f if line.strip()]

    summary = []

    for name in benchmark_names:
        data = parse_benchmark(name)
        
        standard_json = os.path.join(results_abs_dir, f"standard_{name}.json")
        naive_json = os.path.join(results_abs_dir, f"naive_{name}.json")

        # Run Standard
        if not os.path.exists(standard_json):
            cmd_standard = f"./depsynt --model-name=\"{name}\" --input=\"{data['input']}\" --output=\"{data['output']}\" --dependency-timeout=10000 --formula=\"{data['formula']}\" --measures-path=\"{standard_json}\""
            run_cmd(cmd_standard, cwd=os.path.join(ROOT_DIR, "build"))

        # Run Naive (No model checking by default as it is slow)
        if not os.path.exists(naive_json):
            cmd_naive = f"./inp_dep_synthesis --algorithm naive --model-name=\"{name}\" --input=\"{data['input']}\" --output=\"{data['output']}\" --dependency-timeout=10000 --env-formula=\"true\" --system-formula=\"{data['formula']}\" --measures-path=\"{naive_json}\""
            run_cmd(cmd_naive, cwd=os.path.join(ROOT_DIR, "build"))

        # Collect results
        row = {"benchmark": name}
        
        std_time = 0
        naive_time = 0

        if os.path.exists(standard_json):
            try:
                with open(standard_json, "r") as f:
                    res = json.load(f)
                    std_time = res.get("total_time", 0)
                    row["std_runtime"] = std_time
                    row["std_out_deps"] = res.get("dependency", {}).get("total_dependencies", 0)
            except:
                row["std_runtime"] = "Error"
                row["std_out_deps"] = "Error"
        else:
            row["std_runtime"] = "N/A"
            row["std_out_deps"] = "N/A"

        if os.path.exists(naive_json):
            try:
                with open(naive_json, "r") as f:
                    res = json.load(f)
                    naive_time = res.get("total_time", 0)
                    row["naive_runtime"] = naive_time
                    row["naive_inp_deps"] = res.get("input_dependencies", {}).get("count", 0)
                    row["naive_out_deps"] = res.get("dependency", {}).get("total_dependencies", 0)
            except:
                row["naive_runtime"] = "Error"
                row["naive_inp_deps"] = "Error"
                row["naive_out_deps"] = "Error"
        else:
            row["naive_runtime"] = "N/A"
            row["naive_inp_deps"] = "N/A"
            row["naive_out_deps"] = "N/A"

        # Calculate ratio (Speedup: Std / Naive)
        if isinstance(std_time, (int, float)) and isinstance(naive_time, (int, float)) and naive_time > 0:
            row["ratio"] = round(std_time / naive_time, 3)
        else:
            row["ratio"] = "N/A"

        summary.append(row)

    # Save summary
    summary_path = os.path.join(ROOT_DIR, "comparison_results.csv")
    with open(summary_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=["benchmark", "std_runtime", "std_out_deps", "naive_runtime", "naive_inp_deps", "naive_out_deps", "ratio"])
        writer.writeheader()
        writer.writerows(summary)

    print("\n>>> Comparison Summary (Time in ms):")
    print("{:<30} {:<10} {:<10} {:<10} {:<10} {:<10} {:<10}".format("Benchmark", "StdTime", "StdDep", "NaivTime", "InpDep", "OutDep", "Ratio"))
    for row in summary:
        print("{:<30} {:<10} {:<10} {:<10} {:<10} {:<10} {:<10}".format(
            row["benchmark"][:29], 
            str(row["std_runtime"]), 
            str(row["std_out_deps"]), 
            str(row["naive_runtime"]), 
            str(row["naive_inp_deps"]), 
            str(row["naive_out_deps"]),
            str(row["ratio"])
        ))

if __name__ == "__main__":
    main()
