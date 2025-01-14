#include "dual_graph.h"

#include <boost/graph/graph_traits.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/graph/kruskal_min_spanning_tree.hpp>
#include <boost/graph/dijkstra_shortest_paths.hpp>

#include "vtk.h"

using namespace std;
using namespace zjucad::matrix;

namespace riemann {

int build_tri_mesh_dual_graph(const mati_t &tris, shared_ptr<edge2cell_adjacent> &ec,
                              shared_ptr<Graph> &g, const char *dotfile) {
  ec.reset(edge2cell_adjacent::create(tris));
  if ( ec.get() == nullptr ) {
    cerr << "[Error] can not create edge2cell\n";
    return EXIT_FAILURE;
  }
  g.reset(new Graph(tris.size(2)));
  if ( g.get() == nullptr ) {
    cerr << "[Error] new graph fail\n";
    return EXIT_FAILURE;
  }
  property_map<Graph, edge_weight_t>::type weight = get(boost::edge_weight, *g);
  for (size_t i = 0; i < ec->edges_.size(); ++i) {
    pair<size_t, size_t> faces = ec->query(ec->edges_[i].first, ec->edges_[i].second);
    if ( ec->is_boundary_edge(faces) )
      continue;
    boost::graph_traits<Graph>::edge_descriptor e; bool inserted;
    tie(e, inserted) = boost::add_edge(faces.first, faces.second, *g);
    weight[e] = 1.0;
  }
  if ( dotfile != nullptr ) {
    ofstream ofs(dotfile);
    write_graphviz(ofs, *g);
    ofs.close();
  }
  return EXIT_SUCCESS;
}

size_t get_farest_node(const shared_ptr<const Graph> &g, const size_t source) {
  typedef boost::graph_traits<Graph>::vertex_descriptor vertex_descriptor;
  vector<double> d(num_vertices(*g));
  vertex_descriptor s = vertex(source, *g);
  dijkstra_shortest_paths(*g, s, boost::distance_map(boost::make_iterator_property_map(d.begin(), get(boost::vertex_index, *g))));

  double max_dist = -1.0;
  size_t rtn_face = -1;
  for (size_t i = 0; i < d.size(); ++i) {
    if ( d[i] > max_dist ) {
      max_dist = d[i];
      rtn_face = i;
    }
  }
  cout << "[Info] max dist from root: " << max_dist << endl;
  return rtn_face;
}

//int build_tri_mesh_dual_graph_version2(const mati_t &tris, const matd_t &nods, std::shared_ptr<edge2cell_adjacent> &ec,
//                                       std::shared_ptr<Graph> &g, const char *dotfile) {
//  ec.reset(edge2cell_adjacent::create(tris));
//  if ( ec.get() == nullptr ) {
//    cerr << "[Error] can not create edge2cell\n";
//    return EXIT_FAILURE;
//  }
//  g.reset(new Graph(tris.size(2)));
//  if ( g.get() == nullptr ) {
//    cerr << "[Error] new graph fail\n";
//    return EXIT_FAILURE;
//  }
//  property_map<Graph, edge_weight_t>::type weight = get(boost::edge_weight, *g);
//  for (size_t i = 0; i < ec->edges_.size(); ++i) {
//    pair<size_t, size_t> faces = ec->query(ec->edges_[i].first, ec->edges_[i].second);
//    if ( ec->is_boundary_edge(faces) )
//      continue;
//    boost::graph_traits<Graph>::edge_descriptor e; bool inserted;
//    tie(e, inserted) = boost::add_edge(faces.first, faces.second, *g);
//    // adaptive edge weight according to the orientation
//    matd_t edge_vec = nods(colon(), ec->edges_[i].first)-nods(colon(), ec->edges_[i].second);
//    matd_t axis_y = zeros<double>(3, 1); axis_y[1] = 1.0;
//    matd_t axis_x = zeros<double>(3, 1); axis_x[0] = 1.0;
//    if ( dot(edge_vec, axis_y) < 1e-6 )
//      weight[e] = 0.1;
//    else if ( dot(edge_vec, axis_x) < 1e-6 )
//      weight[e] = 1000;
//    else
//      weight[e] = 0.1;
//  }
//  if ( dotfile != nullptr ) {
//    ofstream ofs(dotfile);
//    write_graphviz(ofs, *g);
//    ofs.close();
//  }
//  return EXIT_SUCCESS;
//}

int get_minimum_spanning_tree(const shared_ptr<const Graph> &g, graph_t &mst, const char *dotfile) {
  if ( g.get() == nullptr ) {
    cerr << "[Error] graph object dosen't exist\n";
    return EXIT_FAILURE;
  }
  typedef boost::graph_traits<Graph>::edge_descriptor Edge;
  vector<Edge> spanning_tree;
  kruskal_minimum_spanning_tree(*g, std::back_inserter(spanning_tree));

  mst.u.resize(2*spanning_tree.size());
  mst.v.resize(2*spanning_tree.size());
  mst.first.resize(boost::num_vertices(*g));
  mst.next.resize(2*spanning_tree.size());
  std::fill(mst.first.begin(), mst.first.end(), -1);
  std::fill(mst.next.begin(), mst.next.end(), -1);

  size_t k = 0;
  for (vector<Edge>::iterator ei = spanning_tree.begin(); ei != spanning_tree.end(); ++ei) {
    mst.u[k] = source(*ei, *g);
    mst.v[k] = target(*ei, *g);
    mst.next[k] = mst.first[mst.u[k]];
    mst.first[mst.u[k]] = k;
    ++k;
    mst.u[k] = target(*ei, *g);
    mst.v[k] = source(*ei, *g);
    mst.next[k] = mst.first[mst.u[k]];
    mst.first[mst.u[k]] = k;
    ++k;
  }
  if ( dotfile != nullptr ) {
    Graph tree(boost::num_vertices(*g));
    for (vector<Edge>::iterator ei = spanning_tree.begin(); ei != spanning_tree.end(); ++ei) {
      boost::add_edge(source(*ei, *g), target(*ei, *g), tree);
    }
    ofstream ofs(dotfile);
    write_graphviz(ofs, tree);
    ofs.close();
  }
  return EXIT_SUCCESS;
}

int draw_minimum_spanning_tree(const char *file, const mati_t &tris, const matd_t &nods, const graph_t &mst) {
  mati_t tree(2, tris.size(2)-1);
  matd_t vert(3, tris.size(2));
  for (size_t i = 0; i < vert.size(2); ++i) {
    vert(colon(), i) = nods(colon(), tris(colon(), i))*ones<double>(3, 1)/3.0;
  }
  set<pair<size_t, size_t>> vis;
  size_t cnt = 0;
  for (size_t i = 0; i < mst.u.size(); ++i) {
    const size_t p = mst.u[i], q = mst.v[i];
    if ( vis.find(make_pair(p, q)) != vis.end() || vis.find(make_pair(q, p)) != vis.end() )
      continue;
    vis.insert(make_pair(p, q));
    tree(0, cnt) = p;
    tree(1, cnt) = q;
    ++cnt;
  }
  if ( cnt != tris.size(2)-1 ) {
    cerr << "[Error] in output the tree\n";
    return __LINE__;
  }
  ofstream ofs(file);
  if ( ofs.fail() ) {
    cerr << "[Error] cant open " << file << endl;
    return __LINE__;
  }
  line2vtk(ofs, &vert[0], vert.size(2), &tree[0], tree.size(2));
  ofs.close();
  return 0;
}
}
