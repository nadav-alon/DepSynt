import os
import json
import glob
import re

def analyze_input_deps(search_dir):
    results = []
    files = glob.glob(os.path.join(search_dir, "*.out"))
    
    # Sort by numeric ID if possible
    def get_id(filepath):
        name = os.path.basename(filepath).split('.')[0]
        try:
            return int(name)
        except ValueError:
            return name
            
    files.sort(key=get_id)
    
    for file_path in files:
        try:
            with open(file_path, 'r') as f:
                content = f.read()
                lines = content.splitlines()
                
                # Try to find JSON summary
                json_data = None
                for line in reversed(lines):
                    line = line.strip()
                    if line.startswith('{') and line.endswith('}'):
                        try:
                            json_data = json.loads(line)
                            break
                        except:
                            continue
                
                # Extract info
                dependent_vars = []
                formula = "Unknown"
                source_file = "Unknown"
                
                # Extract from JSON if available
                if json_data:
                    deps = json_data.get('dependency', {}).get('tested_dependencies', [])
                    dependent_vars = [d['name'] for d in deps if d.get('is_dependent')]
                    formula = json_data.get('formula', "Unknown")
                
                # Extract from text if not in JSON or to supplement
                for line in lines:
                    if line.startswith("Running: "):
                        # Look for --formula and --input
                        match = re.search(r'--formula="([^"]+)"', line)
                        if match:
                            formula = match.group(1)
                    elif line.startswith("Running on:"):
                        source_file = line.replace("Running on:", "").strip()
                    elif not dependent_vars and line.startswith("Input Dependent Variables:"):
                        vars_str = line.replace("Input Dependent Variables:", "").strip()
                        if vars_str:
                            dependent_vars = [v.strip().rstrip(',') for v in vars_str.split() if v.strip() and v.strip() != ',']

                if dependent_vars:
                    results.append({
                        "id": os.path.basename(file_path).split('.')[0],
                        "file": os.path.basename(file_path),
                        "source": source_file,
                        "vars": dependent_vars,
                        "formula": formula
                    })
        except Exception:
            continue
            
    return results

if __name__ == "__main__":
    output_dir = "tasks_output_hpc/find_deps"
    if not os.path.exists(output_dir):
        print(f"Directory {output_dir} not found.")
    else:
        all_files = glob.glob(os.path.join(output_dir, "*.out"))
        found = analyze_input_deps(output_dir)
        
        print(f"Analyzed {len(all_files)} files in {output_dir}.")
        if not found:
            print("No input dependencies found.")
        else:
            print(f"Found {len(found)} tests with input dependencies.")
            print("=" * 100)
            for item in found:
                print(f"ID: {item['id']:<5} | Vars: {', '.join(item['vars'])}")
                print(f"Source: {item['source']}")
                fmt_formula = item['formula']
                if len(fmt_formula) > 100:
                    fmt_formula = fmt_formula[:97] + "..."
                print(f"Formula: {fmt_formula}")
                print("-" * 100)
            
            print(f"\nSummary: {len(found)}/{len(all_files)} tests have input dependencies.")
