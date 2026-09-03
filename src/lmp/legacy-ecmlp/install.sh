mkdir -p build
cd build

# $LAMMPS_PREFIX: the path of lammps code (including src, cmake, lib, etc.),
# built with the PLUGIN and ELECTRODE packages
cmake -DLAMMPS_SOURCE_DIR=$LAMMPS_PREFIX/src \
	..
make
