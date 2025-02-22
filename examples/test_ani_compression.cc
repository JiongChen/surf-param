#include <iostream>
#include <boost/filesystem.hpp>
#include <jtflib/mesh/mesh.h>
#include <jtflib/mesh/io.h>

#include "src/json.h"
#include "src/diffuse_dihedral_rot.h"
#include "src/dual_graph.h"
#include "src/config.h"
//#include "igl/principal_curvature.h"
//#include "igl/readOBJ.h"
//#include "src/vtk.h"
//#include "src/write_vtk.h"

using namespace std;
using namespace Eigen;
using namespace riemann;
using namespace zjucad::matrix;

typedef signed char byte;

static void quantize(const vector<double> &dat, const pair<double, double> range,
                     const pair<byte, byte> bound, vector<byte> &qdat) {
  if ( qdat.size() != dat.size() )
    qdat.resize(dat.size());
  for (size_t i = 0; i < qdat.size(); ++i) {
    qdat[i] = floor((double)bound.first+((double)bound.second-(double)bound.first)/(range.second-range.first)*(dat[i]-range.first)+0.5);
  }
}

static void dequantize(const vector<byte> &qdat, const pair<double, double> range,
                       const pair<byte, byte> bound, vector<double> &dat) {
  if ( dat.size() != qdat.size() )
    dat.resize(qdat.size());
  for (size_t i = 0; i < dat.size(); ++i) {
    dat[i] = range.first+(range.second-range.first)/((double)bound.second-(double)bound.first)*((double)qdat[i]-(double)bound.first);
  }
}

static int write_quant_res_bin(const char *file, const vector<byte> &qdat) {
  ofstream ofs(file, ios::binary);
  if ( ofs.fail() ) {
    cerr << "[Error] can not open " << file << endl;
    return __LINE__;
  }
  for (size_t i = 0; i < qdat.size(); ++i)
    ofs.write((char *)&qdat[i], sizeof(byte));
  ofs.close();
  return 0;
}

static int read_quant_res_bin(const char *file, vector<byte> &qdat) {
  ifstream ifs(file, ios::binary);
  if ( ifs.fail() ) {
    cerr << "[Error] can not open " << file << endl;
    return __LINE__;
  }
  if ( !qdat.empty() )
    qdat.clear();
  byte buffer;
  while ( ifs.read((char *)&buffer, sizeof(byte)) )
    qdat.push_back(buffer);
  ifs.close();
  return 0;
}

static int write_data_text(const char *file, const vector<double> &data) {
  ofstream ofs(file);
  if ( ofs.fail() ) {
    cerr << "[Error] cant open " << file << endl;
    return __LINE__;
  }
  for (size_t i = 0; i < data.size(); ++i)
    ofs << i << " " << data[i] << endl;
  ofs.close();
  return 0;
}

static int write_data_binary(const char *file, const vector<double> &data) {
  ofstream ofs(file, ios::binary);
  if ( ofs.fail() ) {
    cerr << "[Error] cant open " << file << endl;
    return __LINE__;
  }
  for (size_t i = 0; i < data.size(); ++i)
    ofs.write((char *)&data[i], sizeof(double));
  ofs.close();
  return 0;
}

extern "C" {
void calc_dihedral_angle_(double *value, const double *x);
}

#define WRITE_DATA_PROG 0
#if WRITE_DATA_PROG == 0
int main(int argc, char *argv[])
{
  if ( argc != 2 ) {
    cerr << "#usage: ./test_ani_compression config.json\n";
    return __LINE__;
  }
  Json::Reader reader;
  Json::Value json;
  ifstream ifs(argv[1]);
  if ( ifs.fail() ) {
    cerr << "[Error] can not open " << argv[1] << endl;
    return __LINE__;
  }
  if ( !reader.parse(ifs, json) ) {
    cerr << "[Error] " << reader.getFormattedErrorMessages() << endl;
    return __LINE__;
  }
  ifs.close();

  // INPUT
  mati_t tris; matd_t nods, nods_prev, nods_curr;
  jtf::mesh::load_obj(json["mesh_rest"].asString().c_str(), tris, nods);
  jtf::mesh::load_obj(json["mesh_prev"].asString().c_str(), tris, nods_prev);
  jtf::mesh::load_obj(json["mesh_curr"].asString().c_str(), tris, nods_curr);

  string outdir = json["outdir"].asString();
  boost::filesystem::create_directories(outdir);
  {
    string outfile = json["outdir"].asString()+"/tri_groud_truth.obj";
    jtf::mesh::save_obj(outfile.c_str(), tris, nods_curr);
  }

  // BUILD SPANNING TREE OF DUAL GRAPH
  shared_ptr<edge2cell_adjacent> ec;
  shared_ptr<Graph> g; tree_t mst;
  build_tri_mesh_dual_graph(tris, ec, g);
  get_minimum_spanning_tree(g, mst);
  const size_t root_face = json["root_face"].asUInt();
  const size_t leaf_face = get_farest_node(g, root_face);
  printf("[Info] root face: %zu\n[Info] leaf face: %zu\n", root_face, leaf_face);
  {
    string outfile = json["outdir"].asString()+"/tree.vtk";
    draw_minimum_spanning_tree(outfile.c_str(), tris, nods, mst);
  }
  // Once we konw the root, the undirected tree could
  // be transformed to a directed version for reducing
  // the memory cost
  // get_directed_tree(mst, root, dmst);

  // ENCODE: angles and anchors
  diffuse_arap_encoder encoder;
  vector<double> da;
  matd_t root_curr, leaf_curr;
  encoder.calc_delta_angle(tris, nods_prev, nods_curr, mst,
                           root_face, leaf_face, root_curr, leaf_curr, da);
  const double min_da = *std::min_element(da.begin(), da.end()),
      max_da = *std::max_element(da.begin(), da.end());
  printf("[Info] min delta: %lf\n[Info] max delta: %lf\n", min_da, max_da);
  {
    string outfile = json["outdir"].asString()+"/data.txt";
    write_data_text(outfile.c_str(), da);
  }

  // DECODE UNQUANTIZED
  diffuse_arap_decoder decoder(tris, nods);
  for (size_t i = 0; i < 3; ++i) {
    size_t id = tris(i, root_face);
    decoder.pin_down_vert(id, &nods_curr(0, id));
//    id = tris(i, leaf_face);
//    decoder.pin_down_vert(id, &nods_curr(0, id));
  }
  decoder.estimate_rotation(nods_prev, mst, root_face, root_curr, da);
  matd_t rec_curr_uq(3, nods.size(2));
  decoder.solve(rec_curr_uq, json["linear_solver"].asString());
  {
    string outfile = json["outdir"].asString()+"/tri_recover_unquantized.obj";
    jtf::mesh::save_obj(outfile.c_str(), tris, rec_curr_uq);
  }

  // QUANTIZE: UNIFORM SCALE AND TRUNCATE
  const byte BITS = json["bits"].asInt();
  ASSERT(BITS <= 8*sizeof(byte));
  const byte bound = (2<<(BITS-2))-1;
  printf("[Info] quantization bound: [%d, %d]\n", -bound, bound);
  vector<byte> cda;
  quantize(da, make_pair(min_da, max_da), make_pair(-bound, bound), cda);
  {
    string quan_bin = outdir+string("/quant.dat");
    write_quant_res_bin(quan_bin.c_str(), cda);
  }

  // DEQUANTIZE
  vector<double> dq_data;
  dequantize(cda, make_pair(min_da, max_da), make_pair(-bound, bound), dq_data);
  {
    string outfile = json["outdir"].asString()+"/error.txt";
    vector<double> error;
    std::transform(dq_data.begin(), dq_data.end(), da.begin(), std::back_inserter(error), std::minus<double>());
    write_data_text(outfile.c_str(), error);
  }

  // DECODE QUANTIZED
  decoder.estimate_rotation(nods_prev, mst, root_face, root_curr, dq_data);
  matd_t rec_curr_q(3, nods.size(2));
  decoder.solve(rec_curr_q, json["linear_solver"].asString());
  {
    string outfile = json["outdir"].asString()+"/tri_recover_quantized.obj";
    jtf::mesh::save_obj(outfile.c_str(), tris, rec_curr_q);
  }

  cout << "[Info] done\n";
  return 0;
}
#else
int main(int argc, char *argv[])
{
  if ( argc != 2 ) {
    cerr << "#usage: ./prog input_dir\n";
    return __LINE__;
  }
  // INPUT
  mati_t tris; matd_t nods, nods_curr;
  char input[256];
  sprintf(input, "%s/rest.obj", argv[1]);
  if ( jtf::mesh::load_obj(input, tris, nods) ) {
    cerr << "[Error] cant load rest configuration\n";
    return __LINE__;
  }

  boost::filesystem::create_directories("./dress");

  // BUILD SPANNING TREE OF DUAL GRAPH
  shared_ptr<edge2cell_adjacent> ec;
  shared_ptr<Graph> g;
  build_tri_mesh_dual_graph(tris, ec, g);
  tree_t mst;
  get_minimum_spanning_tree(g, mst);

  // ENCODE: angles and anchors
  diffuse_arap_encoder encoder;
  for (int i = 0; i < 1000; ++i) {
    cout << "[Info] processing frame " << i << endl;
    sprintf(input, "%s/%04d_00.obj", argv[1], i);
    if ( jtf::mesh::load_obj(input, tris, nods_curr) ) {
      cout << "[Info] no frame " << i << endl;
      break;
    }
    vector<double> da;
    matd_t root_curr;
    encoder.calc_delta_angle(tris, nods, nods_curr, mst, 0, root_curr, da);
    {
      char output[256];
      sprintf(output, "./dress/delta_angle_%04d.dat", i);
      write_data_binary(output, da);
      sprintf(output, "./dress/delta_angle_%04d.txt", i);
      write_data_text(output, da);
    }
  }

  cout << "[Info] done\n";
  return 0;
}
#endif
