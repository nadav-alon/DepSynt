import os
import subprocess
import re
import glob
import csv
import shlex
import signal
import resource

def set_max_stack_size():
    try:
        soft, hard = resource.getrlimit(resource.RLIMIT_STACK)
        resource.setrlimit(resource.RLIMIT_STACK, (hard, hard))
        print(f"Stack size increased to {hard}")
    except Exception as e:
        print(f"Warning: Could not increase stack size: {e}")

def get_signal_name(signum):
    try:
        return signal.Signals(abs(signum)).name
    except ValueError:
        return f"Signal {abs(signum)}"

SYFCO = "/home/cowclaw/.local/bin/syfco"
FIND_DEPS = "./find_dependencies"
LTLFILT = "ltlfilt"
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
OUTPUT_CSV = os.path.join(SCRIPT_DIR, "input_dependencies_results.csv")

def has_env_spec(tlsf_path):
    try:
        with open(tlsf_path, 'r') as f:
            content = f.read()
            return any(keyword in content for keyword in ["REQUIRE", "ASSUME", "INITIALLY"])
    except UnicodeDecodeError:
        return False

def get_env_formula_from_basic(tlsf_path):
    cmd = [SYFCO, "-f", "basic", tlsf_path]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        return None
    
    content = result.stdout
    
    def get_block(name, text):
        pattern = f"{name}\\s*\{{"
        match = re.search(pattern, text)
        if not match:
            return []
        
        start_idx = match.end()
        depth = 1
        end_idx = -1
        for i in range(start_idx, len(text)):
            if text[i] == '{':
                depth += 1
            elif text[i] == '}':
                depth -= 1
                if depth == 0:
                    end_idx = i
                    break
        
        if end_idx == -1:
            return []
            
        block_content = text[start_idx:end_idx]
        # Split by ; and handle simple parenthesized items
        return [i.strip() for i in block_content.split(";") if i.strip()]

    initially = get_block("INITIALLY", content)
    require = get_block("REQUIRE", content)
    assume = get_block("ASSUME", content)
    
    if not (initially or require or assume):
        return None

    final_parts = []
    
    if initially:
        # Join all initial conditions with &&
        init_str = " && ".join([f"({p})" for p in initially])
        final_parts.append(f"({init_str})")
        
    if require:
        # Join all safety requirements with &&, then wrap in single G
        req_str = " && ".join([f"({p})" for p in require])
        final_parts.append(f"G({req_str})")
        
    if assume:
        # Liveness/Assumptions usually carry their own operators or are G-wrapped if interpreted as LTL
        # Join directly
        assume_str = " && ".join([f"({p})" for p in assume])
        final_parts.append(f"({assume_str})")
    
    combined = " && ".join(final_parts)
    
    # Optional: Skip ltlfilt simplification if suspected to cause issues, 
    # but usually it's good. Let's keep it for now but handle failure gracefully.
    try:
        simplify_cmd = [LTLFILT, "-f", combined]
        simplify_result = subprocess.run(simplify_cmd, capture_output=True, text=True)
        if simplify_result.returncode == 0:
            return simplify_result.stdout.strip()
    except Exception:
        pass
        
    return combined

def get_signals(tlsf_path, signal_type="-ins"):
    cmd = [SYFCO, signal_type, tlsf_path]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        return ""
    signals = [s.strip().lower() for s in result.stdout.split(",") if s.strip()]
    return ",".join(signals)

def main():
    set_max_stack_size()
    benchmarks_dir = "scripts/benchmarks/tlsf"
    tlsf_files = glob.glob(f"{benchmarks_dir}/**/*.tlsf", recursive=True)
    
    print(f"Found {len(tlsf_files)} TLSF files.")
    
    # FOR TESTING: only run on a few known ones
    # tlsf_files = ["scripts/benchmarks/tlsf/amba/amba_gr1/specs/amba_gr+_2.tlsf", "scripts/test_simple.tlsf"]
    
    results = []
    tlsf_files.sort()
    
    for tlsf_file in tlsf_files:
        if not has_env_spec(tlsf_file):
            print(f"Skipping {tlsf_file} (no env spec)")
            continue
            
        print(f"Processing {tlsf_file}...")
        env_formula = get_env_formula_from_basic(tlsf_file)
        if not env_formula:
            print(f"  No environment formula extracted.")
            continue
        print(f"  Extracted env formula (first 50 chars): {env_formula[:50]}...")
            
        inputs = get_signals(tlsf_file, "-ins")
        outputs = get_signals(tlsf_file, "-outs")
        
        if not inputs:
            print(f"  No inputs found, skipping.")
            continue

        print(f"  Original Inputs: {inputs}")
        print(f"  Original Outputs: {outputs}")

        cmd = [
            FIND_DEPS,
            "--formula", env_formula,
            "--input", outputs if outputs else "dummy_out",
            "--output", inputs,
            "--algo", "automaton",
            "--verbose"
        ]
        
        print(f"  Running command: {shlex.join(cmd)}")
        
        status = "Success"
        stdout = ""
        try:
            res = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
            stdout = res.stdout
            if res.returncode != 0:
                status = "Error"
                if res.returncode < 0:
                    sig_name = get_signal_name(res.returncode)
                    print(f"  Error: Process terminated by {sig_name} (Exit Code {res.returncode})")
                    # Python's subprocess doesn't capture the shell's 'Segmentation fault' message
                    if res.returncode == -11:
                         print("  Hint: This often indicates a Segmentation Fault (memory access violation).")
                else:
                    print(f"  Error (Exit Code {res.returncode}): {res.stderr.strip()}")
        except subprocess.TimeoutExpired:
            status = "Timeout"
            print(f"  Timeout!")
        except Exception as e:
            status = f"Exception"
            print(f"  Exception: {e}")
            
        dep_vars = []
        if status == "Success":
            # Fixed regex to match "Automaton Dependent Variables: var1, var2, "
            match = re.search(r"Automaton Dependent Variables: (.*)", stdout)
            print(f"  Raw stdout from tool: {stdout.strip()}")
            if match:
                line = match.group(1).strip()
                if line.endswith(","):
                    line = line[:-1]
                dep_vars = [v.strip() for v in line.split(",") if v.strip()]
        
        results.append({
            "benchmark": os.path.relpath(tlsf_file, benchmarks_dir),
            "status": status,
            "dependent_inputs": ",".join(dep_vars),
            "num_dependent": len(dep_vars),
            "total_inputs": len(inputs.split(",")) if inputs else 0
        })
        
        if dep_vars:
            print(f"  Found {len(dep_vars)} dependent inputs: {dep_vars}")
        elif status == "Success":
            print(f"  No dependent inputs found.")

    with open(OUTPUT_CSV, 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=["benchmark", "status", "dependent_inputs", "num_dependent", "total_inputs"])
        writer.writeheader()
        writer.writerows(results)
    
    print(f"\nDone! Results saved to {OUTPUT_CSV}")

if __name__ == "__main__":
    main()
