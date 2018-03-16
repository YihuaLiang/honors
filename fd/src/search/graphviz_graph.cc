#include "graphviz_graph.h"

void
GraphvizGraph::clear() {
  edges.clear();
  nodes.clear();
}

void
GraphvizGraph::add_edge(std::string a, std::string b) {
  add_edge(a, b, "");
}

void
GraphvizGraph::add_edge(std::string a, std::string b, std::string label) {
  nodes.insert(a);
  nodes.insert(b);
  edges.insert(std::make_pair(std::make_pair(a, b), label));
}

void
GraphvizGraph::add_node(std::string a) {
  nodes.insert(a);
}

void
GraphvizGraph::dump(std::string filename) {
  std::ofstream gvfile;
  gvfile.open(filename.c_str());
  dump(gvfile);
  gvfile.close();
}

std::ostream&
GraphvizGraph::dump(std::ostream &os) {
  os << "digraph supportgraph {" << std::endl;

  for(std::set<Node>::iterator it = nodes.begin();
      it != nodes.end(); it++) {
    os << "\t\"" << *it << "\" ;" << std::endl;
  }

  for(std::set<std::pair<Edge, std::string> >::iterator it = edges.begin();
      it != edges.end(); it++) {
    os << "\t\"";
    os << it->first.first;
    os << "\" -> \"";
    os << it->first.second;
    os << "\" [label=\"" << it->second << "\"];" << std::endl;
  }
  os << "}" << std::endl;
  return os;
}
