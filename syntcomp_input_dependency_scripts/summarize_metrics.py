import json
import subprocess
import os

with open('../src/inputDependencies/benchmarks/verification_suite.json', 'r') as f:
    benchmarks = json.load(f)

for b in benchmarks:
    cmd = [
        './inp_dep_synthesis',
        '-e', b['env'],
        '-s', b['sys'],
        '--input', b['inputs'],
        '--output', b['outputs'],
        '--algorithm', 'naive'
    ]
    res = subprocess.run(cmd, capture_output=True, text=True)
    
    lines = res.stdout.strip().split('\n')
    json_lines = [l for l in lines if l.startswith('{')]
    
    if json_lines:
        try:
            data = json.loads(json_lines[-1])
            inp_deps = data.get('input_dependencies', {})
            deps = data.get('dependency', {})
            strat = data.get('synthesis', {}).get('final_strategy', {})
            
            print(f"{b['name']:<40} | InpDeps: {inp_deps.get('count', 0):<2} | OutDeps: {deps.get('total_dependencies', 0):<2} | Time: {data.get('total_time', 0):<4}ms | Controller Gates: {strat.get('total_gates', 'N/A')}")
        except Exception as e:
            print(f"{b['name']:<40} | Failed to parse JSON: {e}")
    else:
        print(f"{b['name']:<40} | No JSON output")
