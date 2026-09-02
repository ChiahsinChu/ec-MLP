# deepmd-kit patches for `fix dplr`

Patches against deepmd-kit's `source/lmp/fix_dplr.cpp`, to be applied to the
deepmd-kit source tree _before_ deepmd-kit is compiled.

They are not interchangeable and they are not both required. Start with
`202506-fix_dplr.patch`; you only need `202404-fix_dplr_a0.patch` if you are
testing the ELECTRODE work.

## `202506-fix_dplr.patch` — mandatory

**If you want to use `verlet/split` with the official DPLR (no ELECTRODE yet),
this patch is mandatory.** It is the only one that path needs.

It promotes `dfele` from a local of `FixDPLR::post_force()` to a public member,
re-zeroed at the end of `pre_force()`. `verlet/split/dplr` computes the k-space
part of the force on a separate partition and has to hand the result back to
`fix dplr`; `src/lmp/verlet_split_dplr.cpp` does that by writing
`fix_dplr->dfele[...]` directly. Against an unpatched deepmd-kit that member is
private, and the ec-MLP LAMMPS plugin does not compile.

```bash
git clone -b v3.1.1 https://github.com/deepmodeling/deepmd-kit.git
cd deepmd-kit
git am -3 /path/to/ec-MLP/dp-patch/202506-fix_dplr.patch
```

## `202404-fix_dplr_a0.patch` — testing only, for now

**Not needed for the `verlet/split` path above.** This one is for testing: it is
the base of the ELECTRODE-support work, and the patches derived from it that
actually add that support will be added here later.

It moves the setup-time Wannier-centroid placement out of `setup_pre_force()` and
to the end of `setup_post_neighbor()`, and adds `MIN_POST_NEIGHBOR` to the fix
mask.

`fix dplr` writes the Wannier-centroid coordinates into `atom->x` from
`pre_force()`. During setup that used to happen in `setup_pre_force()`, which
LAMMPS calls only after _every_ fix has already run its `setup_post_neighbor()` —
so a fix that reads `atom->x` while setting itself up saw the centroids still
collapsed onto their host atoms. `fix electrode/conp` builds its elastance matrix
in exactly that hook, which is why this is the groundwork for ELECTRODE support.

Two consequences of the move:

- **`fix dplr` must be defined before any fix that depends on the centroid
  positions.** Fixes run in definition order.
- `Modify::setup_post_neighbor()` dispatches on `MIN_POST_NEIGHBOR` when
  `whichflag == 2`, so `POST_NEIGHBOR` alone left `minimize` with no centroid
  placement at all — and, already before this patch, with no restart collapse
  either. Setting both mask bits repairs that path.

Apply it on top of the mandatory patch if you want to exercise this:

```bash
git am -3 /path/to/ec-MLP/dp-patch/202404-fix_dplr_a0.patch
```

The two patches touch disjoint parts of `fix_dplr.cpp`, so the order does not
matter. Use `git apply --3way` instead of `git am -3` to apply them without
creating commits.

## Compatibility

Both patches apply cleanly to deepmd-kit **v3.0.0 through v3.2.0** and are
behaviour-preserving for a plain `run` on every one of them, at 1 and 4 MPI ranks
— each on its own, and both together. See [`regression/`](./regression/) for the
full record and for what it does not cover.

That range is about the patches. The ec-MLP plugin itself is supported on
deepmd-kit v3.1.1, which is why the commands above clone that tag; see
[Version compatibility](../README.md#version-compatibility).
