# README

[![Test Python](https://github.com/chenggroup/ec-MLP/actions/workflows/test_python.yml/badge.svg)](https://github.com/chenggroup/ec-MLP/actions/workflows/test_python.yml) [![codecov](https://codecov.io/gh/chenggroup/ec-MLP/graph/badge.svg?token=742UjFq34v)](https://codecov.io/gh/chenggroup/ec-MLP)

DeepMD-kit plugin for ElectroChemical interfaces simulations.

## Installation

### Python interface (training and inference)

After installing deepmd-kit (python interface), the plugin for python interface can be installed by:

```bash
git clone https://github.com/chenggroup/ec-MLP.git
pip install ec-MLP[torch_admp]
```

For testing, you might need to install additional dependencies via

```bash
pip install ec-MLP[torch_admp,test]
cd ec-MLP
python -m pytest tests
```

### Lammps interface

For a LAMMPS interface, a `fix dplr` patch from [`dp-patch/`](./dp-patch/) must be
applied to the source code of deepmd-kit before it is built:

```bash
# get deepmd-kit source code
git clone -b v3.1.1 https://github.com/deepmodeling/deepmd-kit.git
cd deepmd-kit
# add patch for fix dplr
git am -3 /path/to/ec-MLP/dp-patch/202506-fix_dplr.patch
# install deepmd-kit from src...
```

`202506-fix_dplr.patch` is **mandatory** if you want to use `verlet/split` with the
official DPLR (ELECTRODE is not supported yet): it makes `FixDPLR::dfele` public,
which `src/lmp/verlet_split_dplr.cpp` writes to, and the plugin does not compile
without it.

`dp-patch/` also ships `202404-fix_dplr_a0.patch`, which is not needed for that
path — it is for testing, and the patches derived from it that add ELECTRODE
support will follow later. See [`dp-patch/README.md`](./dp-patch/README.md).

After installing deepmd-kit (dp-lmp interface), the plugin can be installed by:

```bash
cd ec-MLP/src/lmp
mkdir -p build && cd build
# $LAMMPS_PREFIX: the path of lammps code (including src, cmake, lib, etc.)
# $deepmd_source_dir: the path of deepmd-kit source code (including deepmd, source, examples, etc.)
# $deepmd_root: the path of deepmd-kit’s C++ interface installed (including bin, include, lib, share, etc.)
cmake -DLAMMPS_SOURCE_DIR=$LAMMPS_PREFIX/src \
      -DDEEPMD_SOURCE_DIR=$deepmd_source_dir/source/lmp \
      -DCMAKE_PREFIX_PATH=$deepmd_root \
      ..
make
```

For testing:

```bash
cd ec-MLP/tests/lmp
bash run_all_tests.sh
```

## Version compatibility

This plugin is compatible with [DeepMD-kit v3.1.1](https://github.com/deepmodeling/deepmd-kit/releases/tag/v3.1.1) and [Lammps Stable release 22 July 2025](https://github.com/lammps/lammps/releases/tag/stable_22Jul2025). Older versions of both softwares do not work.

The `dp-patch/` patches themselves apply cleanly to DeepMD-kit v3.0.0 through
v3.2.0 and are verified behaviour-preserving on all of them, each on its own and
both together; see [`dp-patch/regression/`](./dp-patch/regression/). That record
covers the patches only — it says nothing about this plugin on those versions.

## Documentation

The complete documentation for ec-MLP is available at:

- [**Live documentation**](https://wiki.cheng-group.net/ec-MLP/)
- [**Documentation source**](./doc/)

The documentation is automatically built and deployed to GitHub Pages using GitHub Actions. For more details on building documentation locally or contributing to the documentation, see [`doc/README.md`](./doc/README.md).
