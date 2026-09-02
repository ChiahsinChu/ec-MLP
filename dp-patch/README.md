# deepmd-kit patches for `fix dplr`

ec-MLP's LAMMPS plugin does not build against a stock deepmd-kit. Both patches
here apply to `source/lmp/fix_dplr.cpp` and must be applied to the deepmd-kit
source tree *before* deepmd-kit is compiled.

## The patches

### `0001-make-fele-public-and-can-be-modified-by-external-mod.patch`

Promotes `dfele` from a `post_force()` local to a public `FixDPLR` member, and
re-zeroes it at the end of `pre_force()`.

**Required to build.** `src/lmp/verlet_split_dplr.cpp` hands the k-space forces to
`fix dplr` by writing `fix_dplr->dfele[...]` directly; without this patch that
member does not exist and the plugin does not compile.

### `0001-fix-lmp-place-Wannier-centroids-in-setup_post_neighb.patch`

Moves the setup-time Wannier-centroid placement out of `setup_pre_force()` and to
the end of `setup_post_neighbor()`, and adds `MIN_POST_NEIGHBOR` to the fix mask.

LAMMPS runs every fix's `setup_post_neighbor()` before any `setup_pre_force()`, so
a fix that reads `atom->x` while setting itself up used to see the centroids still
collapsed onto their host atoms — `fix electrode/conp` builds its elastance matrix
in exactly that hook. Placing the centroids in `setup_post_neighbor()` fixes that.

Two consequences:

- **`fix dplr` must be defined before any fix that depends on the centroid
  positions.** Fixes run in definition order.
- `Modify::setup_post_neighbor()` dispatches on `MIN_POST_NEIGHBOR` when
  `whichflag == 2`, so `minimize` previously got no centroid placement at all (and
  no restart collapse either). Setting both mask bits repairs that path.

## Applying them

```bash
git clone -b v3.1.3 https://github.com/deepmodeling/deepmd-kit.git
cd deepmd-kit
git am -3 /path/to/ec-MLP/dp-patch/*.patch
```

The two patches touch disjoint parts of `fix_dplr.cpp`, so the order does not
matter. Use `git apply --3way` instead of `git am -3` if you would rather not
create commits.

## Compatibility

Both patches apply cleanly to deepmd-kit **v3.0.0 through v3.2.0** and are
behaviour-preserving for a plain `run` on every one of them, at 1 and 4 MPI ranks.
See [`regression/`](./regression/) for the full record and for what it does not
cover.
