# DPLR patch regression results — deepmd-kit v3

Reference output showing that the two `fix dplr` patches are behaviour-preserving
for a plain `run`, on every deepmd-kit v3 branch, at 1 and 4 MPI ranks.

- **`ref/`** — built from the branch tip **without** the patches
- **`test/`** — built from the same tip **with both** patches applied

## Result

Every version matches. `test` reproduces `ref` exactly, at both rank counts.

| version | commit     | np=1  | np=4  |
| ------- | ---------- | ----- | ----- |
| v3.0.0  | `e695a91c` | match | match |
| v3.1.0  | `b494a0d5` | match | match |
| v3.1.1  | `bfa62458` | match | match |
| v3.1.2  | `d798b33a` | match | match |
| v3.1.3  | `b2c8511e` | match | match |
| v3.2.0  | `8cfd46e3` | match | match |

All 24 trajectories here share one md5 (`a1605aa87d20c4e48340589392ba7029`) and one
thermo table — the patches, the branch and the rank count all leave the result
unchanged.

## The patches under test

- **p1** `202404-fix_dplr_a0.patch` — moves setup-time Wannier-centroid placement
  from `setup_pre_force()` to the end of `setup_post_neighbor()`, and adds
  `MIN_POST_NEIGHBOR` to the fix mask.
- **p2** `202506-fix_dplr.patch` — promotes `dfele` to a public `FixDPLR` member,
  re-zeroed at the end of `pre_force()`.

`test/` has **both** applied. They were also verified individually; each alone
matched `ref` on every version too.

## The case

512 waters — 2048 atoms (512 O, 1024 H, 512 Wannier centroids X) with 512 O–X
bonds — under `pair_style deepmd` + `kspace_style pppm/dplr` + `fix dplr`,
100 NVT steps at 300 K, `thermo 1`, dump every 10 steps.

```
graph.pb     md5 654acf144a859f1d595ede9c0f5c34e1   (TF DPLR model, dipole_charge, tmap "O H")
system.data  md5 a9ea654cd944afd98f5d58909a3713cc
input.lmp    md5 797c06eeaff2817c06dbb95153276492
```

`graph.pb` is the model already in this repository, at
[`tests/lmp/verlet_split_dplr/bulk_water/graph.pb`](../../tests/lmp/verlet_split_dplr/bulk_water/graph.pb)
— the md5 above is that file's. It is stored with git-lfs, so `git lfs pull` is
needed before the bytes (rather than a pointer file) are on disk.

## Layout

```
case/{input.lmp,system.data}                the one deck behind every run
v<version>/
  ref/np1/{dump.lammpstrj,log.lammps}       unpatched
  ref/np4/...
  test/np1/...                              both patches
  test/np4/...
compare_regression.py
```

All 24 runs were driven by the same input deck and the same geometry, so those two
files are stored once under `case/` instead of in every leaf; their md5s are the
ones listed above and are pinned in `compare_regression.py`. The outputs are _not_
deduplicated — the 24 `dump.lammpstrj` files are byte-identical, but that identity
is the result being demonstrated, so each run keeps its own copy. `graph.pb` is not
copied in here (9.7 MB); it is the one already in the repository, named above.

## Checking

```bash
python3 compare_regression.py            # every version; exit 0 iff all match
python3 compare_regression.py v3.1.3     # one version
```

`case/input.lmp` and `case/system.data` are checksummed first — if the deck or the
geometry behind these outputs is not the one recorded above, the comparison proves
nothing, so a mismatch there is a hard failure (exit 2) rather than a reported
difference. Then `dump.lammpstrj` is compared
byte-wise, then number-by-number; `log.lammps`
is parsed for its thermo table and compared number-by-number (the raw log carries
timings and dates, so it cannot be byte-compared). A mismatch prints the column,
the largest absolute and relative deviation, and the step it occurred at.

## How it was produced

Per version: a detached `git worktree` at the branch tip, its own env with that
branch's **own pinned** `lammps` wheel and an era-appropriate `tensorflow-cpu`, and
one persistent scikit-build-core build dir so `ref` and `test` differ only in
`source/lmp`. Patches applied with `git apply --3way`. The built plugin is moved
out of `deepmd/lib` so each run selects its variant purely through
`LAMMPS_PLUGIN_PATH` and loads exactly one plugin.

CPU only (`DP_VARIANT=cpu`, `DP_ENABLE_PYTORCH=0`), `OMP_NUM_THREADS=1` and TF
intra/inter-op threads pinned to 1, so summation order is fixed. Runs on JURECA-DC
`dc-cpu`.

Confidence checks that back the result:

- **Determinism control** — `ref` run a second time is byte-identical on every
  version at both rank counts, so the noise floor is zero and "match" means something.
- The four plugin binaries per version have four distinct md5s, and all 60 run logs
  name their own plugin directory — nothing accidentally compared `ref` against `ref`.
- All 60 runs completed the full 100 steps with a clean exit.

## What this does _not_ cover

- **`minimize`.** The case only issues `run`. Patch p1 changes the minimize path on
  purpose: before it, `min_setup()` → `setup()` → `post_force()` never reached
  `setup_pre_force()`, so minimize placed no centroids at all, and
  `Modify::setup_post_neighbor()` dispatches on `MIN_POST_NEIGHBOR` (not
  `POST_NEIGHBOR`) when `whichflag == 2`. Equality is the _wrong_ expectation there.
- **`fix electrode/conp`.** p1 exists so that a fix defined after `fix dplr` sees
  placed centroids in its own `setup_post_neighbor()`. No such fix is present here,
  so this shows p1 is harmless — not that it fixes what it claims to.
- **Centroid coordinates directly.** `input.lmp` dumps the `real_atom` group, so the
  X sites are not in the trajectory. A misplaced centroid reaches the O/H forces and
  the thermo energies within one step, but only through that coupling.
- **Precision.** LAMMPS writes the dump with 6 significant figures and thermo with 8,
  so "identical" means identical to what LAMMPS records. Over 100 steps a real
  difference in centroid placement amplifies well past that.
