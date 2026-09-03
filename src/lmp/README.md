# LAMMPS plugins

Two independent LAMMPS plugins, for two workflows that cannot be used together.
Each folder is its own CMake project, built separately, and ships the deepmd-kit
patch its own workflow needs under `patch/`.

| Folder                                      | Plugin                 | Styles                                                         | Needs                         |
| ------------------------------------------- | ---------------------- | -------------------------------------------------------------- | ----------------------------- |
| [`verlet-split-dplr/`](./verlet-split-dplr) | `ecmlpplugin.so`       | `run_style verlet/split/kspace`, `run_style verlet/split/dplr` | deepmd-kit, LAMMPS `PLUGIN`   |
| [`legacy-ecmlp/`](./legacy-ecmlp)           | `ecmlplegacyplugin.so` | `fix electrode/conp/dplr`, `fix electrode/conq/dplr`           | LAMMPS `PLUGIN` + `ELECTRODE` |

`verlet/split/dplr` splits the k-space force onto a second partition; the
constant-potential styles in `legacy-ecmlp` run on a single partition and are
rejected by `verlet/split/dplr` outright. Hence two plugins rather than one.

`LAMMPSInterfaceCXX.cmake` sits here rather than in either folder — both projects
add this directory to `CMAKE_MODULE_PATH` and include it from there, so the two
cannot drift apart.

See [the documentation](../../doc/src/lmp/) for the input syntax, and
[`doc/src/lmp/dp_patch.md`](../../doc/src/lmp/dp_patch.md) for what each patch does
and which deepmd-kit releases it applies to.
