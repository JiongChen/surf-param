project(riemann)
cmake_minimum_required(VERSION 2.8)

list(APPEND CMAKE_CXX_FLAGS "-std=c++0x -fpermissive -fopenmp")

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/;${CMAKE_MODULE_PATH}")
include(geo_sim_sdk)
include_geo_sim_sdk()
link_geo_sim_sdk()
include_directories(${CMAKE_SOURCE_DIR})

# BOOST
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost COMPONENTS filesystem system program_options REQUIRED)
if(Boost_FOUND)
  message("-- Boost @ ${Boost_INCLUDE_DIRS}")
  include_directories(${Boost_INCLUDE_DIRS})
endif(Boost_FOUND)

# EIGEN3
find_package(Eigen3 REQUIRED)
if(EIGEN3_FOUND)
  message("-- Eigen @ ${EIGEN3_INCLUDE_DIR}")
  message("-- Eigen version: ${EIGEN3_VERSION}")
  include_directories(${EIGEN3_INCLUDE_DIR})
endif(EIGEN3_FOUND)

# PETSC
set(PETSC_DIR "/usr/lib/petscdir/3.6.2")
set(PETSC_ARCH "x86_64-linux-gnu-real")
find_package(PETSc REQUIRED)
if(PETSC_FOUND)
  include_directories(/usr/lib/openmpi/include)
  include_directories(/usr/lib/openmpi/lib)
  message("-- PETSc @ ${PETSC_INCLUDE_DIR}")
  message("-- PETSc version: ${PETSC_VERSION}")
  include_directories(${PETSC_INCLUDE_DIR})
endif()

# LBFGS
find_package(LBFGS REQUIRED)
if(LBFGS_FOUND)
  message("-- LBFGS @ ${LBFGS_INCLUDE_DIRS}")
  include_directories(${LBFGS_INCLUDE_DIRS})
endif(LBFGS_FOUND)

# UMFPACK
find_package(UMFPACK REQUIRED)
if(UMFPACK_FOUND)
  include_directories(${UMFPACK_INCLUDES})
endif(UMFPACK_FOUND)

# SUITESPARSE
find_package(SuiteSparse REQUIRED)
if(SuiteSparse_FOUND)
  message("-- SuiteSparse @ ${SuiteSparse_INCLUDE_DIRS}")
  include_directories(${SuiteSparse_INCLUDE_DIRS})
endif(SuiteSparse_FOUND)

# IPOPT
find_package(IPOPT REQUIRED)
if(IPOPT_FOUND)
  message("-- IPOPT @ ${IPOPT_INCLUDE_DIRS}")
  message("-- IPOPT lib @ ${IPOPT_LIBRARIES}")
  include_directories(${IPOPT_INCLUDE_DIRS})
endif(IPOPT_FOUND)

# ARPACK
find_package(Arpack REQUIRED)

# ViennaCL
find_package(CUDA)
if(CUDA_FOUND)
  find_package(ViennaCL)
  if(VIENNACL_FOUND)
    message("-- ViennaCL found @ ${VIENNACL_INCLUDE_DIRS}")
    include_directories(${VIENNACL_INCLUDE_DIRS})
  endif(VIENNACL_FOUND)
endif(CUDA_FOUND)

add_subdirectory(src)
add_subdirectory(examples)
add_subdirectory(test)
