#ifndef GRAPHVIZ_GRAPH_H
#define GRAPHVIZ_GRAPH_H

#include <fstream>
#include <iosfwd>
#include <iostream>
#include <ostream>
#include <set>
#include <sstream>
#include <string>

struct GraphvizGraph {

  typedef std::string Node;
  typedef std::pair<Node, Node> Edge;

  std::set<std::pair<Edge,std::string> > edges;
  std::set<Node> nodes;

  void clear();
  void add_edge(std::string a, std::string b);
  void add_edge(std::string a, std::string b, std::string label);
  void add_node(std::string a);
  std::ostream& dump(std::ostream& os);
  void dump(std::string filename);
};

#endif
