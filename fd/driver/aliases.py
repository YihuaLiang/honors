# -*- coding: utf-8 -*-
from __future__ import print_function

import os

from .util import DRIVER_DIR


PORTFOLIO_DIR = os.path.join(DRIVER_DIR, "portfolios")

ALIASES = {}


#ALIASES["porit"] = \
#    ["--heuristic", "u=uc(x=-1, clauses=statemin, cost_type=one)",
#     "--heuristic", "hff=ff(cost_type=one)",
#     "--search", "tarjan(eval=hff, preferred=hff, cost_type=one, open_list=h, "
#     "pruning=stubborn_sets_ec, "
#     "u_new=0, u_open=2, u_bprop=2, u_refine=true, u_refine_initial_state=false, "
#     "refiner=[uc_iterative(uc=u, selection=scc_level_random, reuse_conflicts=true, forward_phase=complete, restart=false, minimize_result=false)]"
#     ")"]

#ALIASES["test"] = \
#[ "--heuristic", "u=uc(x=-1, clauses=statemin, cost_type=one)",
#  "--heuristic", "hff=ff(cost_type=one)",
#  "--heuristic", "hms=merge_and_shrink(merge_criteria=[scc_no_level, goal], merge_order=level, shrink_strategy=shrink_label_subset_bisimulation(max_states_before_merge=100000, max_states_before_catching=100000, max_states=infinity,  greedy=false, threshold=1, goal_leading=path_relevant_from_threshold, cost_type=1), reduce_labels=true, use_empty_label_shrinking=true, cost_type=3, use_uniform_distances=true, check_unsolvability=true)",
#  "--search", "tarjan(eval=[unsath(hms), hff], preferred=hff, cost_type=one, open_list=h, u_new=0, u_open=2, u_bprop=2, u_refine=true, u_refine_initial_state=false, refiner=[ucrn2_1(uc=u, rn=state, succ=true, size=true)], u_consistent=true)"
#]

#ALIASES["test"] = \
#[ "--heuristic", "u=uc(x=-1, clauses=statemin, cost_type=one)",
#  "--heuristic", "hff=ff(cost_type=one)",
#  "--heuristic", "h_seq=operatorcounting([state_equation_constraints(), feature_constraints(max_size=2)], cost_type=zero)", 
#  "--search", "tarjan(eval=[unsath(h_seq), hff], preferred=hff, cost_type=one, open_list=h, u_new=0, u_open=2, u_bprop=2, u_refine=true, u_refine_initial_state=false, refiner=[ucrn2_1(uc=u, rn=state, succ=true, size=true)], u_consistent=true)"
#]

#ALIASES["test"] = \
#[ "--heuristic", "u=uc(x=-1, clauses=statemin, cost_type=one)",
#  "--heuristic", "hff=ff(cost_type=one)",
#  "--heuristic", "hms=merge_and_shrink(merge_criteria=[scc_no_level, goal], merge_order=level, shrink_strategy=shrink_label_subset_bisimulation(max_states_before_merge=100000, max_states_before_catching=100000, max_states=infinity,  greedy=false, threshold=1, goal_leading=path_relevant_from_threshold, cost_type=1), reduce_labels=true, use_empty_label_shrinking=true, cost_type=3, use_uniform_distances=true, check_unsolvability=true)",
#  "--search", "tarjan(eval=[unsath(hms), hff], preferred=hff, cost_type=one, open_list=h, "
#  "u_new=0, u_open=2, u_bprop=2, u_refine=true, u_refine_initial_state=false, refiner=["
#  "ucrn2_1(uc=u, rn=state, succ=true, size=true), "
#  "uc_iterative(uc=u, selection=scc_level_random, reuse_conflicts=true, forward_phase=complete, restart=false, minimize_result=false)"
#  "])"
#]

#ALIASES["test"] = \
# [
#    "--heuristic", "u=uc(x=-1, clauses=statemin, cost_type=one)",
#    "--heuristic", "hff=ff(cost_type=one)",
#     "--search", "tarjan(eval=hff, preferred=hff, cost_type=one, open_list=h, u_new=0, u_open=2,     u_bprop=2, u_refine=false, u_refine_initial_state=false, refiner=[uc_iterative(uc=u, selection=scc_level_random, reuse_conflicts=true, forward_phase=complete, restart=false, minimize_result=false, offline_x=32)])"
# ]

ALIASES["test"] = \
["--heuristic", "u=adr(learn_on_i=false, x=1.2, cost_type=one, use_conditional_effects=true)", "--heuristic", "hff=ff(cost_type=one)", "--search", "tarjan(eval=hff, preferred=hff, cost_type=one, u_new=0,     u_open=2, u_bprop=2, u_refine=true, u_refine_initial_state=false, open_list=h(tiebreaking=state_id), refiner=[uadr(pic=u, nogoods=true, eval=true, reeval=false, cost_type=one)])"]

#ALIASES["test"] = \
#[
# "--heuristic", "u=adr(learn_on_i=false, x=999999, cost_type=one, use_conditional_effects=true)", "--heuristic", "hff=ff(cost_type=one)", "--search", "tarjan(eval=hff, preferred=hff, cost_type=one, open_list=h, u_new=0, u_open=2, u_bprop=2, u_refine=false, u_refine_initial_state=false, refiner=[uadr(pic=u, nogoods=true, eval=true, reeval=false, cost_type=one, offline_x=32)])"   
#]


ALIASES["ucxit"] = \
    [
     "--heuristic", "u=uc(x=-1, clauses=statemin, cost_type=one)",
        "--heuristic", "hff=ff(cost_type=one)",
        "--search", "dfs(eval=hff, preferred=hff, cost_type=one, refiner=uc_iterative(uc=u, selection=scc_level_random, reuse_conflicts=true, forward_phase=complete, restart=false),  u_refine=true, u_refine_initial_state=true, u_delete_states=false, u_use_plan=true)"
    ]

ALIASES["hpcce"] = \
    [
    "--heuristic",
        "u=adr(learn_on_i=false, x=999999, cost_type=one, use_conditional_effects=true, no_search=true)",
        "--search", "tarjan(eval=inf, cost_type=one, u_new=0, u_open=2, u_bprop=2, u_refine=true, u_refine_initial_state=true, u_use_plan=true, refiner=[uadr(pic=u, nogoods=true, eval=true, reeval=false, cost_type=one)])"   
    ]
                     

ALIASES["dfsff"] = \
    ["--heuristic", "hff=ff(cost_type=one)",
     "--search", "dfs(eval=hff, preferred=hff, tiebreaking=h, cost_type=one)"
    ]

ALIASES["tarjanffucxinf"] = \
    ["--heuristic", "hff=ff(cost_type=one)",
     "--heuristic", "u=uc(x=-1, clauses=statemin, cost_type=one)",
     "--search", "tarjan(eval=hff, preferred=hff, cost_type=one, open_list=h, refiner=ucrn2_1(uc=u, rn=state, succ=true, size=true), u_refine=true, u_refine_initial_state=false, u_delete_states=false)"
    ]
ALIASES["dfsffucxinf"] = \
    ["--heuristic", "hff=ff(cost_type=one)",
     "--heuristic", "u=uc(x=-1, clauses=statemin, cost_type=one)",
     "--search", "dfs(eval=hff, preferred=hff, tiebreaking=h_id, cost_type=one, refiner=ucrn2_1(uc=u, rn=state, succ=true, size=true), u_new=0, u_open=2, u_closed=0, u_bprop=2, u_refine=true, u_refine_initial_state=false)"
    ]

ALIASES["greedyffucxinf"] = \
    ["--heuristic", "hff=ff(cost_type=one)",
     "--heuristic", "u=uc(x=-1, clauses=statemin, cost_type=one)",
     "--search", "dfs(eval=hff, preferred=hff, tiebreaking=h, cost_type=one, refiner=ucrn2_1(uc=u, rn=state, succ=true, size=true), u_new=0, u_open=2, u_closed=0, u_bprop=2, u_refine=true, u_refine_initial_state=false)"
    ]


PORTFOLIOS = {}
for portfolio in os.listdir(PORTFOLIO_DIR):
    name, ext = os.path.splitext(portfolio)
    assert ext == ".py", portfolio
    PORTFOLIOS[name.replace("_", "-")] = os.path.join(PORTFOLIO_DIR, portfolio)


def show_aliases():
    for alias in sorted(ALIASES.keys() + PORTFOLIOS.keys()):
        print(alias)


def set_options_for_alias(alias_name, args):
    """
    If alias_name is an alias for a configuration, set args.search_options
    to the corresponding command-line arguments. If it is an alias for a
    portfolio, set args.portfolio to the path to the portfolio file.
    Otherwise raise KeyError.
    """
    assert not args.search_options
    assert not args.portfolio

    if alias_name in ALIASES:
        args.search_options = [x.replace(" ", "").replace("\n", "")
                               for x in ALIASES[alias_name]]
    elif alias_name in PORTFOLIOS:
        args.portfolio = PORTFOLIOS[alias_name]
    else:
        raise KeyError(alias_name)
