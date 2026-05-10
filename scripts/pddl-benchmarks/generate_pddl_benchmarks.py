import os
import subprocess
import pathlib
from concurrent.futures import ProcessPoolExecutor

# Paths
DEPSYNT_ROOT = pathlib.Path("/home/cowclaw/DepSynt-1")
FOND_DOMAINS_ROOT = pathlib.Path("/home/cowclaw/fond-domains/benchmarks")
GENERATED_DIR = DEPSYNT_ROOT / "pddl-benchmarks" / "generated"
CONVERTER_SCRIPT = DEPSYNT_ROOT / "scripts" / "pddl-benchmarks" / "pddl_to_ltlf.py"

# Ensure output directory exists
GENERATED_DIR.mkdir(parents=True, exist_ok=True)

def process_benchmark(task_id, domain_dir, domain_file, problem_file):
    benchmark_name = f"{domain_dir.name}_{problem_file.stem}"
    output_file = GENERATED_DIR / f"{task_id}.txt"
    
    print(f"Generating {benchmark_name} -> {output_file.name}...")
    
    try:
        # Command to run
        cmd = [
            "python3", str(CONVERTER_SCRIPT),
            "-d", str(domain_file),
            "-p", str(problem_file),
            "--ltl",
            "--text-format",
            "--benchmark-name", benchmark_name,
            "--benchmark-family", "pddl"
        ]
        
        with open(output_file, "w") as f:
            result = subprocess.run(cmd, stdout=f, stderr=subprocess.PIPE, text=True, timeout=30)
        
        if result.returncode != 0:
            error_msg = result.stderr.strip()
            if "too many children" in error_msg:
                print(f"Error generating {benchmark_name}: Formula too complex (too many children).")
            else:
                print(f"Error generating {benchmark_name}: {error_msg}")
            # Remove empty/incomplete file
            if output_file.exists():
                output_file.unlink()
        else:
            print(f"Successfully generated {output_file.name}")
            
    except subprocess.TimeoutExpired:
        print(f"Error generating {benchmark_name}: Timeout after 30s")
        if output_file.exists():
            output_file.unlink()
    except Exception as e:
        print(f"Unexpected error for {benchmark_name}: {str(e)}")
        if output_file.exists():
            output_file.unlink()

def generate_benchmarks():
    tasks = []
    task_id = 1
    for domain_dir in sorted(FOND_DOMAINS_ROOT.iterdir()):
        if not domain_dir.is_dir():
            continue
        
        domain_file = domain_dir / "domain.pddl"
        if not domain_file.exists():
            print(f"Skipping {domain_dir.name}: domain.pddl not found.")
            continue
        
        for problem_file in sorted(domain_dir.glob("*.pddl")):
            if problem_file.name == "domain.pddl":
                continue
            tasks.append((task_id, domain_dir, domain_file, problem_file))
            task_id += 1

    print(f"Total benchmarks to process: {len(tasks)}")
    with ProcessPoolExecutor(max_workers=32) as executor:
        for t in tasks:
            executor.submit(process_benchmark, *t)

if __name__ == "__main__":
    generate_benchmarks()
