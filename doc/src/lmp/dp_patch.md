# deepmd-kit patches for `fix dplr`

## Introduction

ec-MLP's two LAMMPS plugins need changes to deepmd-kit's `fix dplr` that are not
upstream. Each plugin ships the patch its own workflow needs, in its own `patch/`
folder, to be applied to the deepmd-kit source tree before deepmd-kit is compiled.

| patch                                    | shipped by                                                                                                | when you need it                                       |
| ---------------------------------------- | --------------------------------------------------------------------------------------------------------- | ------------------------------------------------------ |
| `202506-fix_dplr.patch`                  | [`src/lmp/verlet-split-dplr`](https://github.com/chenggroup/ec-MLP/tree/master/src/lmp/verlet-split-dplr) | `run_style verlet/split/dplr`                          |
| `202404-fix_dplr-lammps-22Jul2025.patch` | [`src/lmp/legacy-ecmlp`](https://github.com/chenggroup/ec-MLP/tree/master/src/lmp/legacy-ecmlp)           | constant-potential workflow, LAMMPS stable 22 Jul 2025 |
| `202404-fix_dplr-lammps-29Aug2024.patch` | same                                                                                                      | same, LAMMPS stable 29 Aug 2024                        |
| `202404-fix_dplr_a0.patch`               | same                                                                                                      | reference / testing only                               |

These are **two parallel branches from one base, not a stack.** Every patch here is
generated against `ac161730`, which is deepmd-kit **v3.0.0**'s
`source/lmp/fix_dplr.cpp`. From that base `202506-fix_dplr.patch` produces
`00758471`, while `202404-fix_dplr_a0.patch` produces `a2d54736` and the
`202404-fix_dplr-lammps-*` series continues from there. Neither line contains the
other, and neither workflow needs both.

## `202506-fix_dplr.patch` — for `verlet/split/dplr`

It promotes `dfele` from a local of `FixDPLR::post_force()` to a public member,
re-zeroed at the end of `pre_force()`.

This is a build requirement, not a behavioural tweak. `verlet/split/dplr` runs the
k-space part of the force on a separate partition and has to hand the result back
to `fix dplr`; `verlet_split_dplr.cpp` does that by writing `fix_dplr->dfele[...]`
directly. Against an unpatched deepmd-kit the member is private and the plugin does
not compile.

```bash
# get deepmd-kit source code
git clone -b v3.1.1 https://github.com/deepmodeling/deepmd-kit.git
cd deepmd-kit
git am -3 /path/to/ec-MLP/src/lmp/verlet-split-dplr/patch/202506-fix_dplr.patch
# then build deepmd-kit as usual
```

Applies cleanly to v3.0.0 through v3.2.0.

## `202404-fix_dplr-lammps-<release>.patch` — for the constant-potential workflow

**Apply this if you want `kspace_style pppm/electrode/dplr`.** Three commits:

1. the `202404-fix_dplr_a0.patch` commit described below, verbatim;
2. `feat(lmp): add kspace style pppm/electrode/dplr` — the new kspace style, its
   registration in `deepmdplugin.cpp`, the `ELECTRODE` build wiring in
   `builtin.cmake` / `plugin/CMakeLists.txt` / `Install.sh`, and the
   `FixDPLR::post_force()` hunk that collects `get_fele()` from it the same way the
   existing hunk collects from `pppm/dplr`;
3. a release pin recording which LAMMPS stable release the copied
   `PPPMElectrode::compute()` was checked against.

Only the third commit differs between the two files, so pick the one matching your
LAMMPS.

```bash
cd deepmd-kit
git am -3 --empty=keep \
  /path/to/ec-MLP/src/lmp/legacy-ecmlp/patch/202404-fix_dplr-lammps-22Jul2025.patch
```

`--empty=keep` is required. The third commit is a release pin with no file changes,
and plain `git am` stops on an empty patch.

`ELECTRODE` becomes a build dependency of deepmd-kit's LAMMPS interface on this
branch, and since `ELECTRODE` supports only the CMake build upstream, the legacy
`make` path is unavailable with it.

The two fixes that go with this kspace style ship separately, as an ec-MLP plugin —
see [Constant-potential DPLR](./electrode_dplr.md).

### Version range

The series is cut from v3.0.0 and applies cleanly through **v3.1.0**. From
**v3.1.1** on it leaves two conflicts, one hunk each in `source/lmp/fix_dplr.cpp`
and `source/lmp/plugin/deepmdplugin.cpp`. Both are the same pointer-style context
mismatch — the patch was authored on a tree formatted `Type *x`, those releases use
`Type* x` — so the resolution is to take the patch side of each hunk.

| deepmd-kit | `202506-fix_dplr.patch` | `202404-fix_dplr-lammps-*` | both together |
| ---------- | ----------------------- | -------------------------- | ------------- |
| v3.0.0     | clean                   | clean                      | clean         |
| v3.0.3     | clean                   | clean                      | clean         |
| v3.1.0     | clean                   | clean                      | clean         |
| v3.1.1     | clean                   | 2 hunks                    | 2 hunks       |
| v3.1.2     | clean                   | 2 hunks                    | 2 hunks       |
| v3.1.3     | clean                   | 2 hunks                    | 2 hunks       |
| v3.2.0     | clean                   | 2 hunks                    | 2 hunks       |

Where both apply, they compose: the two lines touch adjacent but distinct lines of
`post_force()`, and `git am -3` merges them in either order.

## `202404-fix_dplr_a0.patch` — the ELECTRODE groundwork, on its own

This patch is **not needed for the `verlet/split` path**, and not needed for the
constant-potential workflow either: the series above already contains it as its
first commit. It is kept separately as the ELECTRODE-free reference, because it
changes `fix dplr` and nothing else — which is what the regression record below
covers.

It moves the setup-time Wannier-centroid placement out of `setup_pre_force()` and
to the end of `setup_post_neighbor()`, and adds `MIN_POST_NEIGHBOR` to the fix
mask.

`fix dplr` writes the Wannier-centroid coordinates into `atom->x` from
`pre_force()`. During setup that used to happen in `setup_pre_force()`, which
LAMMPS calls only after *every* fix has already run its `setup_post_neighbor()`.
Any fix that reads `atom->x` while setting itself up therefore saw the centroids
still collapsed onto their host atoms — `fix electrode/conp` builds its elastance
matrix in exactly that hook, so a constant-potential run started from the wrong
geometry.

`setup_post_neighbor()` already existed in `fix dplr` for restart support: it
collapses each centroid onto its host, re-migrates and rebuilds the neighbor lists.
The placement is appended to the end of that same hook, which is also the earliest
point it can happen — `pre_force()` reads `pair_deepmd->list` and calls
`get_valid_pairs(..., false)`, which is fatal if a bonded (atom, centroid) pair
straddles two ranks, and neither the list nor the locality is guaranteed until
after the collapse and its `neighbor->build(1)`.

Two things follow from the move:

- **`fix dplr` must be defined before any fix that depends on the centroid
  positions**, because LAMMPS runs fixes in definition order.
- `Modify::setup_post_neighbor()` dispatches on `MIN_POST_NEIGHBOR` when
  `whichflag == 2` and on `POST_NEIGHBOR` when `whichflag == 1`, so `POST_NEIGHBOR`
  alone left `minimize` with no centroid placement at all — and, already before this
  patch, with no restart collapse either. Setting both bits repairs that path.

To exercise it on its own:

```bash
git am -3 /path/to/ec-MLP/src/lmp/legacy-ecmlp/patch/202404-fix_dplr_a0.patch
```

## Regression status

`202506-fix_dplr.patch` and `202404-fix_dplr_a0.patch` apply cleanly to deepmd-kit
v3.0.0 through v3.2.0 and leave a plain `run` bit-for-bit unchanged on every one of
them — each patch on its own, and both together. That range describes the patches;
the plugins themselves are supported on v3.1.1. The case is 512 waters (2048 atoms,
512 O–X bonds) under `pair_style deepmd` + `kspace_style pppm/dplr` + `fix dplr`,
100 NVT steps at 300 K, compared unpatched (`ref`) against patched (`test`) at 1 and
4 MPI ranks:

| version | commit     | np=1  | np=4  |
| ------- | ---------- | ----- | ----- |
| v3.0.0  | `e695a91c` | match | match |
| v3.1.0  | `b494a0d5` | match | match |
| v3.1.1  | `bfa62458` | match | match |
| v3.1.2  | `d798b33a` | match | match |
| v3.1.3  | `b2c8511e` | match | match |
| v3.2.0  | `8cfd46e3` | match | match |

The trajectories and thermo logs are in `tests/dp-patch-regression/`, checked by

```bash
cd tests/dp-patch-regression
python3 compare_regression.py
```

Note what this does **not** show. It covers `run` only, so it says nothing about
the `minimize` path, where `202404-fix_dplr_a0.patch` changes behaviour on purpose.
And no `fix electrode/conp` is present in the case, so it shows that patch is
harmless — not that it does what the ELECTRODE work needs it to. The
`202404-fix_dplr-lammps-*` series is outside this record altogether; it is pinned
to a LAMMPS release instead. The full list of caveats is in
`tests/dp-patch-regression/README.md`.
