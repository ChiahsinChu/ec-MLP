# `legacy-ecmlp` — constant-potential DPLR fixes

| Style                     | Class                                         |
| ------------------------- | --------------------------------------------- |
| `fix electrode/conp/dplr` | `FixElectrodeConpDPLR : FixElectrodeConp`     |
| `fix electrode/conq/dplr` | `FixElectrodeConqDPLR : FixElectrodeConpDPLR` |

Both derive from the LAMMPS `ELECTRODE` package and touch only `atom` and `modify`
— no deepmd-kit dependency at all, which is why they live here rather than in a
deepmd-kit patch. The kspace style they run with, `pppm/electrode/dplr`, *does*
have one (`FixDPLR::post_force()` names its type to call `get_fele()`), so it is
added to deepmd-kit by the patch below.

## Required deepmd-kit patch

[`patch/202404-fix_dplr-lammps-<release>.patch`](./patch) — three commits that add
`kspace_style pppm/electrode/dplr` to deepmd-kit, place the Wannier centroids in
`setup_post_neighbor()` so `fix electrode/conp` sees them while it builds its
elastance matrix, and pin the LAMMPS release the copied `PPPMElectrode::compute()`
was checked against. Pick the file matching your LAMMPS:

```bash
git clone -b v3.1.0 https://github.com/deepmodeling/deepmd-kit.git
cd deepmd-kit
git am -3 --empty=keep \
  /path/to/ec-MLP/src/lmp/legacy-ecmlp/patch/202404-fix_dplr-lammps-22Jul2025.patch
```

`--empty=keep` is required: the third commit is a release pin with no file changes,
and plain `git am` refuses it.

The series is cut from deepmd-kit **v3.0.0** and applies cleanly through **v3.1.0**.
From v3.1.1 on it leaves two conflicts, both a pointer-style context mismatch
(`Type* x` vs `Type *x`) — one hunk in `source/lmp/fix_dplr.cpp` and one in
`source/lmp/plugin/deepmdplugin.cpp`; take the patch side of each.

`patch/202404-fix_dplr_a0.patch` is that series' first commit on its own, kept as
the ELECTRODE-free reference behind
[`tests/dp-patch-regression/`](../../../tests/dp-patch-regression). You do not need
it in addition to the series.

`patch/202506-fix_dplr.patch` from [`../verlet-split-dplr`](../verlet-split-dplr) is
**not** needed here. The two patch lines are independent branches from the same
v3.0.0 base and do compose, if you want one deepmd-kit build serving both workflows.

## Build

No deepmd arguments — this plugin does not link deepmd-kit:

```bash
mkdir -p build && cd build
# $LAMMPS_PREFIX: the path of lammps code (including src, cmake, lib, etc.)
cmake -DLAMMPS_SOURCE_DIR=$LAMMPS_PREFIX/src ..
make
```

`ELECTRODE` is a hard requirement here: configuring against a LAMMPS source tree
without it is a fatal error, not a silent opt-out — and so is LAMMPS stable
22 Jul 2025 or newer.

See [the documentation](../../../doc/src/lmp/electrode_dplr.md) for the input
syntax and for the two ways these fixes deviate from stock `fix electrode/conp` /
`fix electrode/conq`.
