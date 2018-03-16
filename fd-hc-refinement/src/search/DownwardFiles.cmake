set(PLANNER_SOURCES
  planner.cc
  )

# See http://www.fast-downward.org/ForDevelopers/AddingSourceFiles
# for general information on adding source files and CMake plugins.
#
# If you're adding a file to the codebase which *isn't* a plugin, add
# it to the following list. We assume that every *.cc file has a
# corresponding *.h file and add headers to the project automatically.
# For plugin files, see below.

set(CORE_SOURCES
  axioms.cc
  causal_graph.cc
  scc.cc
  domain_transition_graph.cc
  equivalence_relation.cc
  exact_timer.cc
  globals.cc
  heuristic.cc
  heuristic_refiner.cc
  int_packer.cc
  legacy_causal_graph.cc
  operator.cc
  operator_cost.cc
  option_parser.cc
  option_parser_util.cc
  segmented_vector.cc
  per_state_information.cc
  rng.cc
  search_engine.cc
  search_progress.cc
  state.cc
  state_id.cc
  state_registry.cc
  successor_generator.cc
  timer.cc
  utilities.cc
  graphviz_graph.cc
  pruning_method.cc
  unsatisfiability_heuristic.cc
  plugin.h
  evaluator.h
  scalar_evaluator.h
  )

fast_downward_add_headers_to_sources_list(CORE_SOURCES)
source_group(core FILES planner.cc ${CORE_SOURCES})
list(APPEND PLANNER_SOURCES ${CORE_SOURCES})

## Details of the plugins
#
# For now, everything defaults to being enabled - it's up to the user to specify
#    -DPLUGIN_FOO_ENABLED=FALSE
# to disable a given plugin.
#
# Defining a new plugin:
#    fast_downward_plugin(
#        NAME <NAME>
#        [ DISPLAY_NAME <DISPLAY_NAME> ]
#        [ HELP <HELP> ]
#        SOURCES
#            <FILE_1> [ <FILE_2> ... ]
#        [ DEPENDS <PLUGIN_NAME_1> [ <PLUGIN_NAME_2> ... ] ]
#        [ DEPENDENCY_ONLY ]
#        [ CORE_PLUGIN ]
#    )
#
# <DISPLAY_NAME> defaults to lower case <NAME> and is used to group
#                files in IDEs and for messages.
# <HELP> defaults to <DISPLAY_NAME> and is used to describe the cmake option.
# DEPENDS lists plugins that will be automatically enabled if this plugin
# is enabled. If the dependency was not enabled before, this will be logged.
# DEPENDENCY_ONLY disables the plugin unless it is needed as a dependency and
#     hides the option to enable the plugin in cmake GUIs like ccmake.
# CORE_PLUGIN enables the plugin and hides the option to disable it in
#     cmake GUIs like ccmake.

option(
  DISABLE_PLUGINS_BY_DEFAULT
  "If set to YES only plugins that are specifically enabled will be compiled"
  NO)
# This option should not show up in cmake GUIs like ccmake where all
# plugins are enabled or disabled manually.
mark_as_advanced(DISABLE_PLUGINS_BY_DEFAULT)

fast_downward_plugin(
  NAME UTILS
  HELP "System utilities"
  SOURCES
  utils/logging.cc
  utils/markup.cc
  utils/system.cc
  utils/system_unix.cc
  utils/system_windows.cc
  utils/timer.cc
  CORE_PLUGIN
  )

fast_downward_plugin(
  NAME LP_SOLVER
  HELP "LP solver interface"
  SOURCES
  lp/lp_solver.cc
  lp/lp_internals.cc
  DEPENDENCY_ONLY
  )


fast_downward_plugin(
  NAME NULL_PRUNING_METHOD
  HELP "Pruning method that does nothing"
  SOURCES
  null_pruning_method.cc
  )

fast_downward_plugin(
  NAME STUBBORN_SETS
  HELP "Base class for all stubborn set partial order reduction methods"
  SOURCES
  por/stubborn_sets.cc
  DEPENDENCY_ONLY
  )

fast_downward_plugin(
  NAME STUBBORNSETS_EC
  HELP "Stubborn set method that dominates expansion core"
  SOURCES
  por/stubborn_sets_ec.cc
  DEPENDS STUBBORN_SETS
  )

fast_downward_plugin(
  NAME OPEN_CLOSED_LIST_SEARCH
  HELP "Open/Closed -based search algorithms"
  SOURCES
  search_node_info.cc
  search_space.cc
  open_sets/open_set.cc
  open_sets/dfs_open_set.cc
  open_sets/v_open_set.cc
  search.cc
  DEPENDS NULL_PRUNING_METHOD
  )

fast_downward_plugin(
  NAME TARJAN_SEARCH
  HELP "Tarjan's DFS search algorithm"
  SOURCES
  tarjan/tarjan_state_info.cc
  tarjan/tarjan_search_space.cc
  tarjan/tarjan_open_list.cc
  tarjan/tarjan_dfs.cc
  DEPENDS NULL_PRUNING_METHOD
  )

fast_downward_plugin(
  NAME BLIND_HEURISTIC
  HELP "Blind heuristic"
  SOURCES
  blind_search_heuristic.h
  inf_heuristic.h
  )

fast_downward_plugin(
  NAME DELETE_RELAXATION_HEURISTIC
  HELP "Delete relaxation heuristic"
  SOURCES
  relaxation_heuristic.cc
  DEPENDENCY_ONLY
  )

fast_downward_plugin(
  NAME HADD
  HELP "hadd heuristic"
  SOURCES
  additive_heuristic.cc
  DEPENDS DELETE_RELAXATION_HEURISTIC
  )

fast_downward_plugin(
  NAME HMAX
  HELP "hmax heuristic"
  SOURCES
  max_heuristic.cc
  DEPENDS DELETE_RELAXATION_HEURISTIC
  )

fast_downward_plugin(
  NAME HFF
  HELP "hFF heuristic"
  SOURCES
  ff_heuristic.cc
  DEPENDS HADD
  )

fast_downward_plugin(
  NAME HM
  HELP "hM heuristics"
  SOURCES
  hm_heuristic.cc
  )

fast_downward_plugin(
  NAME LMCUT
  HELP "LMcut heuristic"
  SOURCES
  lm_cut_heuristic.cc
  )

fast_downward_plugin(
  NAME MS
  HELP "M&S heuristics"
  SOURCES
  merge_and_shrink/abstraction.cc
  merge_and_shrink/label_reducer.cc
  merge_and_shrink/merge_and_shrink_heuristic.cc
  merge_and_shrink/shrink_bisimulation.cc
  merge_and_shrink/shrink_label_subset_bisimulation.cc
  merge_and_shrink/shrink_bucket_based.cc
  merge_and_shrink/shrink_fh.cc
  merge_and_shrink/shrink_random.cc
  merge_and_shrink/shrink_none.cc
  merge_and_shrink/shrink_strategy.cc
  merge_and_shrink/linear_merge_strategy.cc
  merge_and_shrink/variable_order_finder.cc
  merge_and_shrink/max_regression_operators.cc
  merge_and_shrink/shrink_empty_labels.cc
  merge_and_shrink/relaxed_plan_operators.cc
  )

fast_downward_plugin(
  NAME PDB
  HELP "PDB heuristics"
  SOURCES
  pdbs/canonical_pdbs_heuristic.cc
  pdbs/dominance_pruner.cc
  pdbs/match_tree.cc
  pdbs/max_cliques.cc
  pdbs/pattern_generation_edelkamp.cc
  pdbs/pattern_generation_haslum.cc
  pdbs/pdb_heuristic.cc
  pdbs/util.cc
  pdbs/zero_one_pdbs_heuristic.cc
  )

fast_downward_plugin(
  NAME POTENTIALS
  HELP "Potential heuristics"
  SOURCES
  operator_counting/operator_counting_heuristic.cc
  operator_counting/constraint_generator.cc
  operator_counting/feature_constraints.cc
  operator_counting/state_equation_constraints.cc
  operator_counting/lm_cut_landmarks.cc
  operator_counting/lm_cut_constraints.cc
  DEPENDS LP_SOLVER
  )

fast_downward_plugin(
  NAME PIC
  HELP "PiC and PiCce heuristics"
  SOURCES
  hcff/fluent_set_utilities.cc
  hcff/operator_utilities.cc
  hcff/augmented_delete_relaxation.cc
  hcff/nogoods.cc
  hcff/adr_heuristic_refiner.cc
  )

fast_downward_plugin(
  NAME HC
  HELP "hC heuristic"
  SOURCES
  hc/hc_heuristic.cc
  hc/conjunction_operations.cc
  )

fast_downward_plugin(
  NAME UC
  HELP "uC heuristic"
  SOURCES
  hc/uc_heuristic.cc
  hc/uc_clause_extraction.cc
  hc/uc_refinement.cc
  hc/uc_refinement_on_rn.cc
  hc/rn_map.cc
  hc/uc_refinement_critical_paths.cc
  DEPENDS HC
  )

fast_downward_add_plugin_sources(PLANNER_SOURCES)

# The order in PLANNER_SOURCES influences the order in which object
# files are given to the linker, which can have a significant influence
# on performance (see issue67). The general recommendation seems to be
# to list files that define functions after files that use them.
# We approximate this by reversing the list, which will put the plugins
# first, followed by the core files, followed by the main file.
# This is certainly not optimal, but works well enough in practice.
list(REVERSE PLANNER_SOURCES)
