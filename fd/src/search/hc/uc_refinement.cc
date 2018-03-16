
#include "uc_refinement.h"

#include "../option_parser.h"

#include <iostream>
#include <cstdio>

void UCRefinementStatistics::dump() const
{
    std::cout << "Number of uC Refinements: " << num_all_refinements
        << " (" << num_real_refinements << ")" << std::endl;
    std::cout << "Summed up size of RN components: " << size_all_component
        << " (" << size_real_component << ")" << std::endl;
    printf("Total time spent on uC Refinement: %.5fs (%.5fs)\n", t_all_refinements, t_real_refinements);
}

void UCRefinement::add_options_to_parser(OptionParser &parser)
{
    parser.add_option<Heuristic*>("uc", "", "uc(x=-1)");
}
