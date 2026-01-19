#!/usr/bin/env python3
"""
Slippery World LTLf and TLSF Generator.
Converted from slippery_world_ltlf_generator.ipynb
"""

import argparse
import sys

def generate_locations(size):
    rows = ["r" + str(i+1) for i in range(size)]
    cols = ["c" + str(i+1) for i in range(size)]
    return rows, cols

def get_mutual_exclusion(vars):
    if not vars:
        return "true", "true"
    all_vars_or = " | ".join(vars)
    combinations = []
    for i in range(len(vars)):
        for j in range(i+1, len(vars)):
            combinations.append(f"!({vars[i]} & {vars[j]})")
    
    if not combinations:
        return f"({all_vars_or})", "true"
    
    return f"({all_vars_or})", " & ".join(combinations)

def get_agent_preconditions(rows, cols):
    prec_list = []
    prec_list.append(f"(l -> !({cols[0]}))")
    prec_list.append(f"(r -> !({cols[-1]}))")
    prec_list.append(f"(u -> !({rows[0]}))")
    prec_list.append(f"(d -> !({rows[-1]}))")
    return prec_list


def get_environment_transitions(rows, cols):
    trans_list = []
    # action left
    for i, col in enumerate(cols):
        if i == 0: continue
        elif i == 1:
            trans_list.append((f"{col}l", f"({col} -> X(l -> {cols[i-1]}))"))
        else:
            trans_list.append((f"{col}l", f"({col} -> X(l -> ({cols[i-1]} | {cols[i-2]})))"))
    # action right
    for i, col in enumerate(cols):
        if i == len(cols)-1: continue
        elif i == len(cols)-2:
            trans_list.append((f"{col}r", f"({col} -> X(r -> {cols[i+1]}))"))
        else:
            trans_list.append((f"{col}r", f"({col} -> X(r -> ({cols[i+1]} | {cols[i+2]})))"))
    # action up
    for i, row in enumerate(rows):
        if i == 0: continue
        elif i == 1:
            trans_list.append((f"{row}u", f"({row} -> X(u -> {rows[i-1]}))"))
        else:
            trans_list.append((f"{row}u", f"({row} -> X(u -> ({rows[i-1]} | {rows[i-2]})))"))
    # action down
    for i, row in enumerate(rows):
        if i == len(rows)-1: continue
        elif i == len(rows)-2:
            trans_list.append((f"{row}d", f"({row} -> X(d -> {rows[i+1]}))"))
        else:
            trans_list.append((f"{row}d", f"({row} -> X(d -> ({rows[i+1]} | {rows[i+2]})))"))
    return trans_list

def get_same_next_state(rows, cols):
    next_list = []
    for col in cols:
        next_list.append((f"{col}u", f"({col} -> X(u -> {col}))"))
        next_list.append((f"{col}d", f"({col} -> X(d -> {col}))"))
    for row in rows:
        next_list.append((f"{row}l", f"({row} -> X(l -> {row}))"))
        next_list.append((f"{row}r", f"({row} -> X(r -> {row}))"))
    return next_list

def generate_ltlf(gridsize, init_pos=(1,1), goal_pos=(4,4), extra_equivalences=False):
    rows, cols = generate_locations(gridsize)
    agent_actions = ["l", "r", "u", "d"]
    
    mut_exc_agent_all, mut_exc_agent_comb = get_mutual_exclusion(agent_actions)
    mut_exc_env_rows_all, mut_exc_env_rows_comb = get_mutual_exclusion(rows)
    mut_exc_env_cols_all, mut_exc_env_cols_comb = get_mutual_exclusion(cols)
    
    env_transitions = get_environment_transitions(rows, cols) + get_same_next_state(rows, cols)
    
    if extra_equivalences:
        env_transition_formulas = [f"({t[0]} <-> {t[1]})" for t in env_transitions]
        extra_inputs = sorted(list(set([t[0] for t in env_transitions])))
    else:
        env_transition_formulas = [t[1] for t in env_transitions]
        extra_inputs = []

    agent_preconditions = get_agent_preconditions(rows, cols)
    
    init_str = f"(r{init_pos[0]} & c{init_pos[1]})"
    agent_str = f"(({mut_exc_agent_all}) & {mut_exc_agent_comb})"
    env_str = f"((({mut_exc_env_rows_all}) & {mut_exc_env_rows_comb}) & (({mut_exc_env_cols_all}) & {mut_exc_env_cols_comb}) & ({' & '.join(env_transition_formulas)}))"
    prec_str = f"(G((X({cols[0]} | !{cols[0]})) -> ({' & '.join(agent_preconditions)})))"
    goal_str = f"F(r{goal_pos[0]} & c{goal_pos[1]})"
    
    # Combined formula according to notebook logic (just a big conjunction for now)
    combined = f"( {init_str} & G({agent_str}) & G({env_str}) & {prec_str} & {goal_str} )"
    return combined, rows + cols + extra_inputs, agent_actions

def generate_tlsf(gridsize, init_pos=(1,1), goal_pos=(4,4), extra_equivalences=False):
    rows, cols = generate_locations(gridsize)
    agent_actions = ["l", "r", "u", "d"]
    
    mut_exc_agent_all, mut_exc_agent_comb = get_mutual_exclusion(agent_actions)
    mut_exc_env_rows_all, mut_exc_env_rows_comb = get_mutual_exclusion(rows)
    mut_exc_env_cols_all, mut_exc_env_cols_comb = get_mutual_exclusion(cols)
    
    env_transitions = get_environment_transitions(rows, cols) + get_same_next_state(rows, cols)
    agent_preconditions = get_agent_preconditions(rows, cols)
    
    init_f = f"r{init_pos[0]} && c{init_pos[1]}"
    
    inputs_list = rows + cols
    if extra_equivalences:
        extra_inputs = sorted(list(set([t[0] for t in env_transitions])))
        inputs_list += extra_inputs
        
        equivalences = [f"({t[1]} <-> {t[0]})" for t in env_transitions]
        env_trans_formula = ";\n    ".join(extra_inputs + equivalences)
    else:
        env_trans_formula = " && ".join([t[1] for t in env_transitions])

    template = """INFO {{
  TITLE:       "Slippery Grid World {gridsize}x{gridsize}"
  DESCRIPTION: "Slippery grid world LTLf benchmark converted to TLSF"
  SEMANTICS:   Mealy
  TARGET:      Mealy
}}

MAIN {{
  INPUTS {{
    {inputs}
  }}

  OUTPUTS {{
    {outputs}
  }}

  INITIALLY {{
    {init_f};
  }}

  REQUIRE {{
    (({mut_exc_env_rows_all}) && ({mut_exc_env_rows_comb}));
    (({mut_exc_env_cols_all}) && ({mut_exc_env_cols_comb}));
    {env_trans_f};
  }}

  ASSERT {{
    (({mut_exc_agent_all}) && ({mut_exc_agent_comb}));
    ((X true) -> ({agent_prec_f}));
  }}

  GUARANTEE {{
    F (r{goal_row} && c{goal_col});
  }}
}}
"""
    
    def to_tlsf_expr(expr):
        return expr.replace(" & ", " && ").replace(" | ", " || ")

    tlsf = template.format(
        gridsize=gridsize,
        inputs=";\n    ".join(inputs_list) + ";",
        outputs=";\n    ".join(agent_actions) + ";",
        init_f=to_tlsf_expr(init_f),
        mut_exc_env_rows_all=to_tlsf_expr(mut_exc_env_rows_all),
        mut_exc_env_rows_comb=to_tlsf_expr(mut_exc_env_rows_comb),
        mut_exc_env_cols_all=to_tlsf_expr(mut_exc_env_cols_all),
        mut_exc_env_cols_comb=to_tlsf_expr(mut_exc_env_cols_comb),
        env_trans_f=to_tlsf_expr(env_trans_formula),
        mut_exc_agent_all=to_tlsf_expr(mut_exc_agent_all),
        mut_exc_agent_comb=to_tlsf_expr(mut_exc_agent_comb),
        agent_prec_f=to_tlsf_expr(" && ".join(agent_preconditions)),
        goal_row=goal_pos[0],
        goal_col=goal_pos[1]
    )
    return tlsf

def main():
    parser = argparse.ArgumentParser(description="Generate LTLf and TLSF for slippery grid world")
    parser.add_argument("--gridsize", type=int, default=6, help="Size of the grid (default 6)")
    parser.add_argument("--format", choices=["ltlf", "tlsf", "both"], default="tlsf", help="Output format (default: tlsf)")
    parser.add_argument("--init", type=int, nargs=2, default=[1, 1], help="Initial position (row col)")
    parser.add_argument("--goal", type=int, nargs=2, help="Goal position (row col), defaults to (gridsize, gridsize)")
    parser.add_argument("--output", type=str, help="Output file path")
    parser.add_argument("--extra-equivalences", action="store_true", help="Add extra variables for environment transitions")
    args = parser.parse_args()
    
    goal = tuple(args.goal) if args.goal else (args.gridsize, args.gridsize)
    init = tuple(args.init)
    
    if args.format == "ltlf":
        content, inputs, outputs = generate_ltlf(args.gridsize, init, goal, extra_equivalences=args.extra_equivalences)
        prefix = f".inputs: {' '.join(inputs)}\n.outputs: {' '.join(outputs)}\n"
        content = prefix + content
    elif args.format == "tlsf":
        content = generate_tlsf(args.gridsize, init, goal, extra_equivalences=args.extra_equivalences)
    else: # both
        ltlf, inputs, outputs = generate_ltlf(args.gridsize, init, goal, extra_equivalences=args.extra_equivalences)
        tlsf = generate_tlsf(args.gridsize, init, goal, extra_equivalences=args.extra_equivalences)
        content = f"--- LTLf ---\n.inputs: {' '.join(inputs)}\n.outputs: {' '.join(outputs)}\n{ltlf}\n\n--- TLSF ---\n{tlsf}"
        
    if args.output:
        with open(args.output, "w") as f:
            f.write(content)
        print(f"Generated {args.format} written to {args.output}", file=sys.stderr)
    else:
        print(content)

if __name__ == "__main__":
    main()
