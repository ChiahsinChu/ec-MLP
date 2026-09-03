# Constant-potential DPLR

## Introduction

These three styles extend DPLR from "long-range polarization under a uniform
external E-field" to **constant-potential electrochemistry**, by wiring DPLR into
the LAMMPS `ELECTRODE` package.

The problem they solve is force routing. Part of the charge in a DPLR system sits
on massless Wannier centroids. Any electrostatic force written straight into
`atom->f` for those sites is lost, because a centroid has no dynamics of its own —
the force has to be back-propagated onto the host atom by the
`DipoleChargeModifier`. Stock `pppm/electrode` and `fix electrode/conp` both write
directly into `atom->f`, so they need DPLR-aware variants.

| Style                              | Role                                                                         | Ships in                        |
| ---------------------------------- | ---------------------------------------------------------------------------- | ------------------------------- |
| `kspace_style pppm/electrode/dplr` | constant-potential PPPM; hands its k-space force to `fix dplr`               | deepmd-kit, via the patch below |
| `fix electrode/conp/dplr`          | constant-**potential** charge solver, field and gausscorr force left to DPLR | ec-MLP, `src/lmp/legacy-ecmlp`  |
| `fix electrode/conq/dplr`          | constant-**charge** variant of the same                                      | ec-MLP, `src/lmp/legacy-ecmlp`  |

The split follows the coupling. `pppm/electrode/dplr` has to live in deepmd-kit
because `FixDPLR::post_force()` names its type to call `get_fele()`. The two fixes
have no deepmd dependency at all — they touch only `atom` and `modify` — so they
ship as a second ec-MLP plugin, built from
[`src/lmp/legacy-ecmlp/`](https://github.com/chenggroup/ec-MLP/tree/master/src/lmp/legacy-ecmlp),
separate from the `verlet/split` plugin in `src/lmp/verlet-split-dplr`.

## Installation

Build deepmd-kit with the patch series that adds `pppm/electrode/dplr`, picking
the file that matches your LAMMPS release:

```bash
git clone -b v3.1.0 https://github.com/deepmodeling/deepmd-kit.git
cd deepmd-kit
git am -3 --empty=keep \
  /path/to/ec-MLP/src/lmp/legacy-ecmlp/patch/202404-fix_dplr-lammps-22Jul2025.patch
# then build deepmd-kit as usual
```

`--empty=keep` is required — the series' third commit is a release pin with no file
changes. The series supersedes `202404-fix_dplr_a0.patch`, whose single commit it
already contains, and it does **not** need `202506-fix_dplr.patch`: that one exists
only for `verlet/split/dplr`, which cannot be used with `pppm/electrode/dplr`
anyway. See [deepmd-kit patches for `fix dplr`](./dp_patch.md), which also lists
which deepmd-kit releases the series applies to cleanly.

LAMMPS must be built with the `PLUGIN`, `KSPACE` and `ELECTRODE` packages. Then
build the fixes:

```bash
cd ec-MLP/src/lmp/legacy-ecmlp
mkdir -p build && cd build
# $LAMMPS_PREFIX: the path of lammps code (including src, cmake, lib, etc.)
cmake -DLAMMPS_SOURCE_DIR=$LAMMPS_PREFIX/src ..
make
```

This produces `ecmlplegacyplugin.so`; put its directory on `LAMMPS_PLUGIN_PATH`
alongside the deepmd-kit plugin. Unlike `src/lmp/verlet-split-dplr`, `ELECTRODE`
is a hard requirement here — configuring without it is a fatal error, not a silent opt-out —
and so is LAMMPS stable 22 Jul 2025 or newer.

## Usage

```bash
# ELECTRODE-aware PPPM that routes its force through DPLR
kspace_style    pppm/electrode/dplr 1e-5
kspace_modify   gewald ${BETA} diff ik mesh ${KMESH_X} ${KMESH_Y} ${KMESH_Z}

fix             fdplr all dplr model graph.pb \
                type_associate 1 6 3 7 4 8 bond_type 1
fix             fconp slab electrode/conp/dplr ${PSI} pair lj/cut/coul/long/gauss
```

The constant-potential workflow runs on a **single partition**;
`run_style verlet/split/dplr` is incompatible with `pppm/electrode/dplr` and says
so.

```bash
mpirun -np 4 lmp_mpi -i input.lmp
```

`fix dplr` must be defined **before** the electrode fix. LAMMPS runs fixes in
definition order, and `fix electrode/conp` builds its elastance matrix in
`setup_post_neighbor()` — it has to see the Wannier centroids already placed,
which is what the `202404` patch arranges.

### `fix electrode/conp/dplr`

Same syntax as `fix electrode/conp`. Two behavioural differences:

- With `ffield yes` it does **not** create the internal `fix efield` that
  `fix electrode/conp` adds. The variables `<fixname>_ffield_vtop`,
  `_ffield_vbot` and `_ffield_zfield` are still defined, so the field can be
  applied through DPLR instead — which is what routes it onto the centroids:

  ```bash
  fix  fdplr all dplr model graph.pb ... efield 0 0 v_fconp_ffield_zfield
  ```

- The Gaussian charge correction contributes energy and virial but **no force**,
  for the force-routing reason above.

### `fix electrode/conq/dplr`

Same syntax as `fix electrode/conq`: the per-group numbers are target **charges**
rather than potentials. It inherits both deviations above from
`fix electrode/conp/dplr`, by construction — it is derived from it rather than
from `fix electrode/conq`, so that any future change to the DPLR force routing
reaches both fixes at once.

```bash
fix  fconq slab electrode/conq/dplr ${QTARGET} pair lj/cut/coul/long/gauss
```

The `qtotal` keyword is rejected, as it is for `fix electrode/conq`.

### `kspace_style pppm/electrode/dplr`

Same syntax as `pppm/electrode`. Requires `newton on`. As with `pppm/dplr`, the
Ewald self-energy is not subtracted from the reported `elong`, so that the energy
keeps the convention the DPLR model was trained with.

## Implementation notes

Both fixes use only public and protected LAMMPS API.
`FixElectrodeConp::gausscorr()` is private, so rather than calling it with
`fflag=false` the DPLR variant lets the base class run and then restores
`atom->f`. That is exactly equivalent: within `pre_reverse()` /
`setup_pre_reverse()` the base class calls only `ev_init()`, `gausscorr()` and
`self_energy()`, and of those only `gausscorr()`, and only under its `if (fflag)`
branch, ever touches `atom->f`.

`fix electrode/conq/dplr` carries a verbatim copy of the five members of
`fix_electrode_conq.cpp`, re-parented onto `FixElectrodeConpDPLR`. Every member
they reach is protected in `FixElectrodeConp`, so the copy compiles unchanged in a
grandchild. It must be re-synced when upstream LAMMPS changes that file. One line
is added rather than copied: `FixElectrodeConp` rejects the `qtotal` keyword by
comparing the style string against `"electrode/conq"` exactly, which
`"electrode/conq/dplr"` does not match, so the guard is re-armed in the
constructor.

## Status

`fix electrode/conq/dplr` has been checked against stock `fix electrode/conq` on
`lammps/examples/PACKAGES/electrode/graph-il/in.conq` at step 0, with `etypes` on
and off: the electrode charges are bit-identical over all 832 electrode atoms,
and the only difference in the forces is the discarded Gaussian correction
(~1.4e-3 kcal/mol/Å here, and exactly zero with `etypes on`, where that
correction vanishes for this system). The `qtotal` guard was checked to fire.

Multi-rank correctness has not been verified. `fele` in `pppm/electrode/dplr` is
indexed by local atom index over owned atoms only, which should be rank-safe, but
this has not been tested — compare a serial run against a parallel one before
trusting production results.
