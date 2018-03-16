
#include "dfs_open_set.h"

#include "../option_parser.h"
#include "../plugin.h"

#include <string>

namespace dfs_open_set {

  enum TieBreaking {
    H = 0,
    RANDOM = 1,
    H_RANDOM = 2,
    H_ID = 3,
  };

  void add_options_to_parser(OptionParser &parser)
  {
    std::vector<std::string> tiebreaking;
    tiebreaking.push_back("h");
    tiebreaking.push_back("random");
    tiebreaking.push_back("h_random");
    tiebreaking.push_back("h_id");
    //tiebreaking.push_back("default");
    parser.add_enum_option("tiebreaking", tiebreaking, "", "h");
    parser.add_option<int>("seed", "", "", OptionFlags(false));
  }

  OpenSet *parse(const Options &opts)
  {
    TieBreaking t = TieBreaking(opts.get_enum("tiebreaking"));
    bool preferred = opts.contains("preferred");
    int seed = 1;
    if (opts.contains("seed")) {
      seed = opts.get<int>("seed");
    }
    switch (t) {
    case H:
      return new dfs_h_ties(preferred);
    case RANDOM:
      return new dfs_rnd_ties(preferred, seed);
    case H_RANDOM:
      return new dfs_h_rnd_ties(preferred, seed);
    case H_ID:
      return new dfs_h_id_ties(preferred);
      //case H_DEFAULT:
      //  return new DefaultDFSOpenList;
    }
    return NULL;
  }

}

