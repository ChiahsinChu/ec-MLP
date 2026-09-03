# `verlet-split-dplr` — MD acceleration for DPLR

Provides two integrate styles:

| Style                           | Role                                                                                             |
| ------------------------------- | ------------------------------------------------------------------------------------------------ |
| `run_style verlet/split/dplr`   | runs the k-space part of the force on a second partition and hands the result back to `fix dplr` |
| `run_style verlet/split/kspace` | the k-space partition's counterpart                                                              |

## Required deepmd-kit patch

[`patch/202506-fix_dplr.patch`](./patch/202506-fix_dplr.patch) — **mandatory**. It
promotes `FixDPLR::dfele` from a local of `post_force()` to a public member,
re-zeroed at the end of `pre_force()`. `verlet_split_dplr.cpp` writes
`fix_dplr->dfele[...]` directly, so against an unpatched deepmd-kit that member is
private and this plugin does not compile.

```bash
git clone -b v3.1.1 https://github.com/deepmodeling/deepmd-kit.git
cd deepmd-kit
git am -3 /path/to/ec-MLP/src/lmp/verlet-split-dplr/patch/202506-fix_dplr.patch
# then build deepmd-kit as usual
```

It applies cleanly to deepmd-kit v3.0.0 through v3.2.0; see
[`tests/dp-patch-regression/`](../../../tests/dp-patch-regression) for the
behaviour-preserving record.

## Build

```bash
mkdir -p build && cd build
# $LAMMPS_PREFIX: the path of lammps code (including src, cmake, lib, etc.)
# $deepmd_source_dir: the path of deepmd-kit source code
# $deepmd_root: the path of deepmd-kit's installed C++ interface
cmake -DLAMMPS_SOURCE_DIR=$LAMMPS_PREFIX/src \
      -DDEEPMD_SOURCE_DIR=$deepmd_source_dir/source/lmp \
      -DCMAKE_PREFIX_PATH=$deepmd_root \
      ..
make
```

Put the build directory on `LAMMPS_PLUGIN_PATH`.

This plugin is **not** compatible with `kspace_style pppm/electrode/dplr` — the
constant-potential workflow runs on a single partition. See
[`../legacy-ecmlp`](../legacy-ecmlp) for that.

See [the documentation](../../../doc/src/lmp/verlet_split_mlp.md) for usage.
