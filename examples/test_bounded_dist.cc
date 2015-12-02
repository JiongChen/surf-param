#include <iostream>
#include <jtflib/mesh/io.h>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <zjucad/matrix/io.h>
#include <zjucad/matrix/itr_matrix.h>
#include <unordered_map>

#include "src/bounded_distortion.h"
#include "src/vtk.h"

using namespace std;
using namespace riemann;
using namespace Eigen;
using namespace zjucad::matrix;
namespace po=boost::program_options;

struct argument {
  string src_tet_file;
  string ini_tet_file;
  string pos_cons_file;
  string output_folder;
  double K;
  bd_args bd;
};

int read_fixed_verts(const char *path, unordered_map<size_t, Vector3d> &fv) {
  ifstream ifs(path);
  if ( ifs.fail() ) {
    cerr << "[info] can not open " << path << endl;
    return __LINE__;
  }
  size_t id; double x, y, z;
  while ( ifs >> id >> x >> y >> z ) {
    fv[id] = Vector3d(x, y, z);
  }
  return 0;
}

int main(int argc, char *argv[])
{
  po::options_description desc("Available options");
  desc.add_options()
      ("help,h", "produce help message")
      ("src_tet,s", po::value<string>(), "set the source tet mesh")
      ("ini_tet,i", po::value<string>(), "set the initial tet mesh")
      ("pos_cons,c", po::value<string>(), "set the position constraints")
      ("out_folder,o", po::value<string>(), "set the output folder")
      ("bound,k", po::value<double>(), "set the bound")
      ("maxiter,m", po::value<size_t>()->default_value(20000), "max iterations")
      ("tolerance,t", po::value<double>()->default_value(1e-8), "tolerance")
      ;
  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);
  if ( vm.count("help") ) {
    cout << desc << endl;
    return __LINE__;
  }
  argument args; {
    args.src_tet_file  = vm["src_tet"].as<string>();
    args.ini_tet_file  = vm["ini_tet"].as<string>();
    args.pos_cons_file = vm["pos_cons"].as<string>();
    args.output_folder = vm["out_folder"].as<string>();
    args.K             = vm["bound"].as<double>();
    args.bd.maxiter    = vm["maxiter"].as<size_t>();
    args.bd.tolerance  = vm["tolerance"].as<double>();
  }
  if ( !boost::filesystem::exists(args.output_folder) )
    boost::filesystem::create_directory(args.output_folder);

  mati_t tets; matd_t nods, nods0;
  jtf::mesh::tet_mesh_read_from_vtk(args.src_tet_file.c_str(), &nods, &tets);
  jtf::mesh::tet_mesh_read_from_vtk(args.ini_tet_file.c_str(), &nods0, &tets);
  unordered_map<size_t, Vector3d> fixv;
  read_fixed_verts(args.pos_cons_file.c_str(), fixv);

  bd_solver solver(tets, nods, args.bd);
  solver.set_bound(args.K);

  for (auto &elem : fixv)
    solver.pin_down_vert(elem.first, elem.second.data());

  solver.prefactorize();
  solver.solve(&nods0[0]);

  char outfile[256];
  sprintf(outfile, "%s/bdmap.bound%.0lf.vtk", args.output_folder.c_str(), args.K);
  ofstream os(outfile);
  tet2vtk(os, &nods0[0], nods0.size(2), &tets[0], tets.size(2));

  cout << "[info] done\n";
  return 0;
}