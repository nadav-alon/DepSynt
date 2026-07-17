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


def get_environment_transitions(rows, cols, determinize=False):
    trans_list = []
    # action left
    for i, col in enumerate(cols):
        if i == 0: continue
        elif i == 1:
            trans_list.append((f"{col}l", f"({col} -> X(l -> {cols[i-1]}))"))
        else:
            if determinize:
                trans_list.append((f"{col}l", f"({col} -> X(l -> ((!slip & {cols[i-1]}) | (slip & {cols[i-2]}))))"))
            else:
                trans_list.append((f"{col}l", f"({col} -> X(l -> ({cols[i-1]} | {cols[i-2]})))"))
    # action right
    for i, col in enumerate(cols):
        if i == len(cols)-1: continue
        elif i == len(cols)-2:
            trans_list.append((f"{col}r", f"({col} -> X(r -> {cols[i+1]}))"))
        else:
            if determinize:
                trans_list.append((f"{col}r", f"({col} -> X(r -> ((!slip & {cols[i+1]}) | (slip & {cols[i+2]}))))"))
            else:
                trans_list.append((f"{col}r", f"({col} -> X(r -> ({cols[i+1]} | {cols[i+2]})))"))
    # action up
    for i, row in enumerate(rows):
        if i == 0: continue
        elif i == 1:
            trans_list.append((f"{row}u", f"({row} -> X(u -> {rows[i-1]}))"))
        else:
            if determinize:
                trans_list.append((f"{row}u", f"({row} -> X(u -> ((!slip & {rows[i-1]}) | (slip & {rows[i-2]}))))"))
            else:
                trans_list.append((f"{row}u", f"({row} -> X(u -> ({rows[i-1]} | {rows[i-2]})))"))
    # action down
    for i, row in enumerate(rows):
        if i == len(rows)-1: continue
        elif i == len(rows)-2:
            trans_list.append((f"{row}d", f"({row} -> X(d -> {rows[i+1]}))"))
        else:
            if determinize:
                trans_list.append((f"{row}d", f"({row} -> X(d -> ((!slip & {rows[i+1]}) | (slip & {rows[i+2]}))))"))
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

def _axis_total_dynamics(axis, mneg, mpos):
    """Total, deterministic, CAUSAL, NON-CLAIRVOYANT next-coordinate function.

    `axis` is the ordered list of position vars (cols or rows); `mneg` is the
    move that decreases the index (l for cols, u for rows) and `mpos` the move
    that increases it (r / d).

    Timing convention (non-clairvoyant): the starting cell and the move are read
    at time t; the slip outcome and the resulting cell appear together at t+1
    (`slip` is INSIDE the X):
        (cur(t) & move(t)) -> X( (!slip & near) | (slip & far) )   [slip,near,far @ t+1]
    so position(t+1) = f(position(t), move(t), slip(t+1)). The slip that resolves a
    move is revealed WITH the result, one step AFTER the move is committed, so when
    the controller chooses move(t) its outcome slip(t+1) is still in the future ->
    the controller never "sees the slip coming" (no clairvoyance). The move is a
    PAST output by the time the next position is set, and slip(t+1) is a current
    input, so position is a causal input dependency (current-step outputs excluded,
    past ones live in the automaton state).

    (An earlier variant put `slip` OUTSIDE the X, at the same step as the move:
    `(cur & move & slip) -> X(next)`. That also finds the dependency but is
    clairvoyant -- the controller reads slip(t) before committing move(t) -- so it
    is not the faithful slippery-world game. We use the slip-inside form here.)

    Totality: for EVERY current coordinate and EVERY move-combo the next coordinate
    is defined (a function of the next slip), so position stays determined on ALL
    runs (incl. rule-violating ones in !phi). Resolution: no move / conflicting
    same-axis move -> stay; single move -> slip-aware step, clamped at walls.
    """
    n = len(axis)
    rules = []
    for i, cur in enumerate(axis):
        # neg move (decreasing index), slip-aware + wall clamp
        neg_near = axis[i-1] if i >= 1 else cur          # wall: bump, stay
        neg_far  = axis[i-2] if i >= 2 else neg_near      # clamp slip overshoot
        # pos move (increasing index)
        pos_near = axis[i+1] if i <= n - 2 else cur       # wall: bump, stay
        pos_far  = axis[i+2] if i <= n - 3 else pos_near  # clamp slip overshoot
        # stay cases do not involve slip; move cases resolve via the NEXT slip
        rules.append((f"{cur}_stay0", f"(({cur} & !{mneg} & !{mpos}) -> X({cur}))"))
        rules.append((f"{cur}_stayC", f"(({cur} & {mneg} & {mpos}) -> X({cur}))"))
        rules.append((f"{cur}_neg",   f"(({cur} & {mneg} & !{mpos}) -> X((!slip & {neg_near}) | (slip & {neg_far})))"))
        rules.append((f"{cur}_pos",   f"(({cur} & !{mneg} & {mpos}) -> X((!slip & {pos_near}) | (slip & {pos_far})))"))
    return rules

def get_total_dynamics(rows, cols):
    """Totalized transition relation (2A): every coordinate is a total
    deterministic function of (current coord, its two moves, slip)."""
    return _axis_total_dynamics(cols, "l", "r") + _axis_total_dynamics(rows, "u", "d")

def generate_ltlf(gridsize, init_pos=(1,1), goal_pos=(4,4), determinize=False, totalize=False):
    rows, cols = generate_locations(gridsize)
    agent_actions = ["l", "r", "u", "d"]

    mut_exc_agent_all, mut_exc_agent_comb = get_mutual_exclusion(agent_actions)
    mut_exc_env_rows_all, mut_exc_env_rows_comb = get_mutual_exclusion(rows)
    mut_exc_env_cols_all, mut_exc_env_cols_comb = get_mutual_exclusion(cols)

    if totalize:
        env_transitions = get_total_dynamics(rows, cols)
    else:
        env_transitions = get_environment_transitions(rows, cols, determinize) + get_same_next_state(rows, cols)
    env_transition_formulas = [t[1] for t in env_transitions]

    agent_preconditions = get_agent_preconditions(rows, cols)

    init_str = f"(r{init_pos[0]} & c{init_pos[1]})"
    agent_str = f"(({mut_exc_agent_all}) & {mut_exc_agent_comb})"
    env_str = f"((({mut_exc_env_rows_all}) & {mut_exc_env_rows_comb}) & (({mut_exc_env_cols_all}) & {mut_exc_env_cols_comb}) & ({' & '.join(env_transition_formulas)}))"
    prec_str = f"(G((X({cols[0]} | !{cols[0]})) -> ({' & '.join(agent_preconditions)})))"
    goal_str = f"F(r{goal_pos[0]} & c{goal_pos[1]})"

    # Totalized dynamics need the slip variable too.
    extra_inputs = ["slip"] if (determinize or totalize) else []
    # Structure as (environment assumptions) -> (system guarantees).
    # The env assumptions (initial position + position mutual-exclusion + transition
    # dynamics) must sit on the LEFT of the implication so they are preserved as
    # positive conjuncts under negation (the dependency finder analyses the automaton
    # of !phi). As a flat conjunction the dynamics are NOT preserved under negation,
    # so position would be free to violate them and would never be found dependent.
    assumptions = f"({init_str} & G({env_str}))"
    guarantees = f"(G({agent_str}) & {prec_str} & {goal_str})"
    combined = f"( {assumptions} -> {guarantees} )"
    return combined, rows + cols + extra_inputs, agent_actions

def generate_tlsf(gridsize, init_pos=(1,1), goal_pos=(4,4), determinize=False):
    rows, cols = generate_locations(gridsize)
    agent_actions = ["l", "r", "u", "d"]

    mut_exc_agent_all, mut_exc_agent_comb = get_mutual_exclusion(agent_actions)
    mut_exc_env_rows_all, mut_exc_env_rows_comb = get_mutual_exclusion(rows)
    mut_exc_env_cols_all, mut_exc_env_cols_comb = get_mutual_exclusion(cols)

    env_transitions = get_environment_transitions(rows, cols, determinize) + get_same_next_state(rows, cols)
    agent_preconditions = get_agent_preconditions(rows, cols)

    init_f = f"r{init_pos[0]} && c{init_pos[1]}"

    inputs_list = rows + cols + (["slip"] if determinize else [])
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
    parser.add_argument("--determinize", action="store_true", help="Add slip variable to determinize slipping nondeterminism")
    parser.add_argument("--totalize", action="store_true", help="Totalize the transition relation (2A): position becomes a total deterministic function of (current pos, moves, slip) on all runs. Implies slip.")
    args = parser.parse_args()

    goal = tuple(args.goal) if args.goal else (args.gridsize, args.gridsize)
    init = tuple(args.init)

    if args.format == "ltlf":
        content, inputs, outputs = generate_ltlf(args.gridsize, init, goal, determinize=args.determinize, totalize=args.totalize)
        prefix = f".inputs: {' '.join(inputs)}\n.outputs: {' '.join(outputs)}\n"
        content = prefix + content
    elif args.format == "tlsf":
        content = generate_tlsf(args.gridsize, init, goal, determinize=args.determinize)
    else: # both
        ltlf, inputs, outputs = generate_ltlf(args.gridsize, init, goal, determinize=args.determinize, totalize=args.totalize)
        tlsf = generate_tlsf(args.gridsize, init, goal, determinize=args.determinize)
        content = f"--- LTLf ---\n.inputs: {' '.join(inputs)}\n.outputs: {' '.join(outputs)}\n{ltlf}\n\n--- TLSF ---\n{tlsf}"
        
    if args.output:
        with open(args.output, "w") as f:
            f.write(content)
        print(f"Generated {args.format} written to {args.output}", file=sys.stderr)
    else:
        print(content)

if __name__ == "__main__":
    main()
