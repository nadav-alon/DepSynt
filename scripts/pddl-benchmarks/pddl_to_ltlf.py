import os
import sys
import subprocess
import argparse

def run_translate(domain_pddl, problem_pddl, translate_py_path):
    """Runs the Fast Downward translate.py script."""
    print(f"[*] Running {translate_py_path}...", file=sys.stderr)
    try:
        subprocess.run(
            ["python3", translate_py_path, "1000", domain_pddl, problem_pddl],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )
        print("[+] Translation successful. output.sas generated.", file=sys.stderr)
    except subprocess.CalledProcessError as e:
        print(f"[-] Translation failed: {e.stderr.decode()}", file=sys.stderr)
        sys.exit(1)

def parse_sas(sas_file):
    """Parses output.sas in the format used by syft4fond."""
    variables = []
    initial_state = []
    goal = []
    operators = []

    with open(sas_file, "r") as f:
        content = f.read()

    # Split into sections
    sections = content.split("begin_")
    
    for section in sections:
        if section.startswith("variable"):
            lines = section.strip().split("\n")
            name = lines[1]
            axiom_layer = int(lines[2])
            range_val = int(lines[3])
            values = lines[4:4+range_val]
            variables.append({"name": name, "range": range_val, "values": values})
        elif section.startswith("state"):
            lines = section.strip().split("\n")
            # In .sas 0 is "true" and 1 is "false" (for boolean variables)
            # Domain.cpp maps 0->1 and 1->0
            for val in lines[1:-1]:
                initial_state.append(int(val))
        elif section.startswith("goal"):
            lines = section.strip().split("\n")
            nb_goals = int(lines[1])
            for i in range(nb_goals):
                var_idx, val = map(int, lines[2+i].split())
                goal.append((var_idx, val))
        elif section.startswith("operator"):
            lines = section.strip().split("\n")
            op_name = lines[1]
            preconditions = []
            effects = []
            for line in lines[2:-1]:
                line = line.strip()
                if not line: continue
                parts = line.split()
                # Precondition: v0: 0
                if len(parts) == 2:
                    var_idx = int(parts[0][1:-1])
                    val = int(parts[1])
                    preconditions.append((var_idx, val))
                # Effect: v2: 6 -> 10
                elif len(parts) == 4:
                    var_idx = int(parts[0][1:-1])
                    pre_val = int(parts[1])
                    post_val = int(parts[3])
                    effects.append({"var": var_idx, "pre": pre_val, "post": post_val})
            operators.append({"name": op_name, "pre": preconditions, "effects": effects})
            
    return variables, initial_state, goal, operators

def sanitize_name(name):
    """Sanitizes PDDL atom names for LTLf tools."""
    # Remove "Atom " or "NegatedAtom " prefixes
    s = name.replace("Atom ", "").replace("NegatedAtom ", "not_")
    # Replace invalid characters with underscores
    import re
    s = re.sub(r'[^a-zA-Z0-9_]', '_', s)
    # Remove trailing underscores
    s = s.rstrip('_')
    # Collapse multiple underscores
    s = re.sub(r'_+', '_', s)
    return s

def generate_ltlf(variables, initial_state, goal, operators):
    """Generates the LTLf formula string."""
    
    # 1. Initial State
    init_parts = []
    for i, val_idx in enumerate(initial_state):
        var = variables[i]
        prop = sanitize_name(var["values"][val_idx])
        init_parts.append(prop)
        # We don't strictly need to assert others are false if we trust the state representation,
        # but for LTLf synthesis it's often safer.
        for j in range(var["range"]):
            if j != val_idx:
                other_prop = sanitize_name(var["values"][j])
                init_parts.append(f"!{other_prop}")
    ltlf_init = " & ".join(f"({p})" for p in init_parts)

    # 2. Goal
    goal_parts = []
    for var_idx, val_idx in goal:
        prop = sanitize_name(variables[var_idx]["values"][val_idx])
        goal_parts.append(prop)
    ltlf_goal = " & ".join(f"({p})" for p in goal_parts)
    ltlf_goal = f"F({ltlf_goal})"

    # 3. Transitions (Successor State Axioms)
    ssa_parts = []
    for i, var in enumerate(variables):
        for j, val_str in enumerate(var["values"]):
            prop = sanitize_name(val_str)
            
            add_actions = []
            del_actions = []
            
            for op in operators:
                op_prop = sanitize_name(op["name"])
                is_add = False
                is_del = False
                for eff in op["effects"]:
                    if eff["var"] == i:
                        if eff["post"] == j:
                            is_add = True
                        else:
                            is_del = True
                
                if is_add:
                    add_actions.append(op_prop)
                elif is_del:
                    del_actions.append(op_prop)
            
            add_formula = "(" + (" | ".join(add_actions)) + ")" if add_actions else "false"
            del_formula = "(" + (" | ".join(del_actions)) + ")" if del_actions else "false"
            
            ssa = f"(X({prop}) <-> (({prop} & !{del_formula}) | {add_formula}))"
            ssa_parts.append(ssa)

    # 4. Preconditions
    pre_parts = []
    for op in operators:
        op_prop = sanitize_name(op["name"])
        op_pre = []
        for var_idx, val_idx in op["pre"]:
            pre_prop = sanitize_name(variables[var_idx]["values"][val_idx])
            op_pre.append(pre_prop)
        
        if op_pre:
            pre_formula = " & ".join(f"({p})" for p in op_pre)
            pre_parts.append(f"({op_prop} -> ({pre_formula}))")

    # 5. Action Mutual Exclusion (Standard Planning assumption: one action per step)
    all_ops = [sanitize_name(op["name"]) for op in operators]
    mutex_parts = []
    for i, op1 in enumerate(all_ops):
        for op2 in all_ops[i+1:]:
            mutex_parts.append(f"(!{op1} | !{op2})")

    # Combine everything with Globally
    ltlf_trans = " & ".join(f"G({p})" for p in ssa_parts + pre_parts + mutex_parts)
    
    full_formula = f"(({ltlf_init}) & ({ltlf_trans})) -> ({ltlf_goal})"
    return full_formula

def main():
    parser = argparse.ArgumentParser(description="Convert PDDL to LTLf/LTL")
    parser.add_argument("-d", "--domain", required=True, help="PDDL domain file")
    parser.add_argument("-p", "--problem", required=True, help="PDDL problem file")
    parser.add_argument("-t", "--translate", default=os.path.expanduser("~/syft4fond/submodules/translate.py"), help="Path to translate.py")
    parser.add_argument("--ltl", action="store_true", help="Output infinite-trace LTL (using ltlfilt)")
    parser.add_argument("--text-format", action="store_true", help="Output in the 8-line text format used by depsynt")
    parser.add_argument("--benchmark-name", default="pddl_benchmark", help="Benchmark name for text format")
    parser.add_argument("--benchmark-family", default="pddl", help="Benchmark family for text format")
    
    args = parser.parse_args()

    run_translate(args.domain, args.problem, args.translate)
    
    if not os.path.exists("output.sas"):
        print("[-] Error: output.sas was not generated.", file=sys.stderr)
        sys.exit(1)
        
    variables, initial_state, goal, operators = parse_sas("output.sas")
    
    ltlf_formula = generate_ltlf(variables, initial_state, goal, operators)
    
    if args.ltl:
        process = subprocess.Popen(["ltlfilt", "--from-ltlf=alive"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        ltl_formula_bytes, error = process.communicate(input=ltlf_formula.encode())
        if process.returncode != 0:
            print(f"[-] ltlfilt failed: {error.decode()}", file=sys.stderr)
            sys.exit(1)
        ltl_formula = ltl_formula_bytes.decode().strip()
    else:
        ltl_formula = ltlf_formula

    # Partition: Actions are outputs, Fluents are inputs
    all_ops = sorted(list(set([sanitize_name(op["name"]) for op in operators])))
    all_fluents = []
    for var in variables:
        for val in var["values"]:
            all_fluents.append(sanitize_name(val))
    all_fluents = sorted(list(set(all_fluents)))
    
    # In FOND planning for synthesis:
    # Outputs = Actions
    # Inputs = Fluents (the environment's "state" that we observe)
    if args.ltl:
        all_fluents.append("alive")
    input_vars = ",".join(sorted(list(set(all_fluents))))
    output_vars = ",".join(all_ops)

    if args.text_format:
        # ID (placeholder 1), Name, Family, LTL, Env, Sys, Inputs, Outputs
        print("1")
        print(args.benchmark_name)
        print(args.benchmark_family)
        print(ltl_formula)
        print("true") # Env
        print(ltl_formula) # Sys
        print(input_vars)
        print(output_vars)
    else:
        print(ltl_formula)

if __name__ == "__main__":
    main()
