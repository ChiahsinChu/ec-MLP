# deepmd-kit patches for `fix dplr`

## Introduction

ec-MLP's LAMMPS plugin builds against deepmd-kit's `fix dplr`, and needs two
changes to it that are not upstream. Both live in
[`dp-patch/`](https://github.com/chenggroup/ec-MLP/tree/master/dp-patch) and apply
to `source/lmp/fix_dplr.cpp` in the deepmd-kit source tree, before deepmd-kit is
compiled.

### Public `dfele`

`0001-make-fele-public-and-can-be-modified-by-external-mod.patch` promotes `dfele`
from a local of `FixDPLR::post_force()` to a public member, re-zeroed at the end of
`pre_force()`.

This is a **build requirement**, not a behavioural tweak. `verlet/split/dplr` runs
the k-space part of the force on a separate partition and has to hand the result
back to `fix dplr`; `src/lmp/verlet_split_dplr.cpp` does that by writing
`fix_dplr->dfele[...]` directly. Against an unpatched deepmd-kit the member is
private and the plugin does not compile.

### Centroid placement at setup

`0001-fix-lmp-place-Wannier-centroids-in-setup_post_neighb.patch` moves the
setup-time Wannier-centroid placement out of `setup_pre_force()` and to the end of
`setup_post_neighbor()`, and adds `MIN_POST_NEIGHBOR` to the fix mask.

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

Two things follow from this:

- **`fix dplr` must be defined before any fix that depends on the centroid
  positions**, because LAMMPS runs fixes in definition order.
- `Modify::setup_post_neighbor()` dispatches on `MIN_POST_NEIGHBOR` when
  `whichflag == 2` and on `POST_NEIGHBOR` when `whichflag == 1`, so `POST_NEIGHBOR`
  alone left `minimize` with no centroid placement at all — and, already before this
  patch, with no restart collapse either. Setting both bits repairs that path.

## Usage

```bash
# get deepmd-kit source code
git clone -b v3.1.3 https://github.com/deepmodeling/deepmd-kit.git
cd deepmd-kit
# apply both patches
git am -3 /path/to/ec-MLP/dp-patch/*.patch
# then build deepmd-kit as usual
```

The two patches touch disjoint parts of `fix_dplr.cpp`, so the order does not
matter. Use `git apply --3way` instead of `git am -3` to apply them without
creating commits.

## Regression status

Both patches apply cleanly to deepmd-kit v3.0.0 through v3.2.0 and leave a plain
`run` bit-for-bit unchanged on every one of them. The case is 512 waters (2048
atoms, 512 O–X bonds) under `pair_style deepmd` + `kspace_style pppm/dplr` +
`fix dplr`, 100 NVT steps at 300 K, compared unpatched (`ref`) against patched
(`test`) at 1 and 4 MPI ranks:

| version | commit     | np=1  | np=4  |
| ------- | ---------- | ----- | ----- |
| v3.0.0  | `e695a91c` | match | match |
| v3.1.0  | `b494a0d5` | match | match |
| v3.1.1  | `bfa62458` | match | match |
| v3.1.2  | `d798b33a` | match | match |
| v3.1.3  | `b2c8511e` | match | match |
| v3.2.0  | `8cfd46e3` | match | match |

The trajectories and thermo logs are in `dp-patch/regression/`, checked by

```bash
cd dp-patch/regression
python3 compare_regression.py
```

Note what this does **not** show. It covers `run` only, so it says nothing about
the `minimize` path, where the second patch changes behaviour on purpose. And no
`fix electrode/conp` is present in the case, so it shows that the centroid-placement
patch is harmless — not that it fixes what it claims to. The full list of caveats is
in `dp-patch/regression/README.md`.
