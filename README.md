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

[`src/lmp/`](./src/lmp) holds two independent LAMMPS plugins, for two workflows
that cannot be used together. Each is its own CMake project and ships the
deepmd-kit patch its own workflow needs under `patch/`; that patch must be applied
to the deepmd-kit source before deepmd-kit is built.

| Style                              | Requires               | Plugin                 | Built from                  |
| ---------------------------------- | ---------------------- | ---------------------- | --------------------------- |
| `run_style verlet/split/kspace`    | `PLUGIN`               | `ecmlpplugin.so`       | `src/lmp/verlet-split-dplr` |
| `run_style verlet/split/dplr`      | `PLUGIN`               | `ecmlpplugin.so`       | `src/lmp/verlet-split-dplr` |
| `kspace_style pppm/electrode/dplr` | `PLUGIN` + `ELECTRODE` | deepmd-kit             | `legacy-ecmlp/patch/`       |
| `fix electrode/conp/dplr`          | `PLUGIN` + `ELECTRODE` | `ecmlplegacyplugin.so` | `src/lmp/legacy-ecmlp`      |
| `fix electrode/conq/dplr`          | `PLUGIN` + `ELECTRODE` | `ecmlplegacyplugin.so` | `src/lmp/legacy-ecmlp`      |

#### `verlet/split/dplr` — MD acceleration

```bash
# get deepmd-kit source code
git clone -b v3.1.1 https://github.com/deepmodeling/deepmd-kit.git
cd deepmd-kit
# mandatory: makes FixDPLR::dfele public, which verlet_split_dplr.cpp writes to
git am -3 /path/to/ec-MLP/src/lmp/verlet-split-dplr/patch/202506-fix_dplr.patch
# install deepmd-kit from src...
```

```bash
cd ec-MLP/src/lmp/verlet-split-dplr
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

#### Constant-potential styles

A different deepmd-kit patch, and a plugin that needs no deepmd arguments at all:

```bash
cd deepmd-kit
git am -3 --empty=keep \
  /path/to/ec-MLP/src/lmp/legacy-ecmlp/patch/202404-fix_dplr-lammps-22Jul2025.patch
```

```bash
cd ec-MLP/src/lmp/legacy-ecmlp
mkdir -p build && cd build
cmake -DLAMMPS_SOURCE_DIR=$LAMMPS_PREFIX/src ..
make
```

`202506-fix_dplr.patch` is **not** needed for this workflow. The two patch lines
are independent branches from the same deepmd-kit v3.0.0 base and do compose, if
you want one deepmd-kit build serving both.

Put whichever build directories you need on `LAMMPS_PLUGIN_PATH`. See
[Constant-potential DPLR](./doc/src/lmp/electrode_dplr.md) and
[deepmd-kit patches](./doc/src/lmp/dp_patch.md).

## Version compatibility

This plugin is compatible with [DeepMD-kit v3.1.1](https://github.com/deepmodeling/deepmd-kit/releases/tag/v3.1.1) and [Lammps Stable release 22 July 2025](https://github.com/lammps/lammps/releases/tag/stable_22Jul2025). Older versions of both softwares do not work.

`202506-fix_dplr.patch` applies cleanly to DeepMD-kit v3.0.0 through v3.2.0, and
is verified behaviour-preserving on all of them; see
[`tests/dp-patch-regression/`](./tests/dp-patch-regression/). The
`202404-fix_dplr-lammps-*` series is cut from v3.0.0 and applies cleanly through
v3.1.0; from v3.1.1 on it leaves two trivial pointer-style conflicts, described in
[`src/lmp/legacy-ecmlp/README.md`](./src/lmp/legacy-ecmlp/README.md). Those ranges
describe the patches only — they say nothing about the plugins on those versions.

## Documentation

The complete documentation for ec-MLP is available at:

- [**Live documentation**](https://wiki.cheng-group.net/ec-MLP/)
- [**Documentation source**](./doc/)

The documentation is automatically built and deployed to GitHub Pages using GitHub Actions. For more details on building documentation locally or contributing to the documentation, see [`doc/README.md`](./doc/README.md).
