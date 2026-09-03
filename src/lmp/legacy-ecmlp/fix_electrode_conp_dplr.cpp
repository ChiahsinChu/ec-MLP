// SPDX-License-Identifier: LGPL-3.0-or-later
#include "fix_electrode_conp_dplr.h"

#include <algorithm>
#include <cstring>
#include <string>

#include "atom.h"
#include "modify.h"

using namespace LAMMPS_NS;

/* ---------------------------------------------------------------------- */

FixElectrodeConpDPLR::FixElectrodeConpDPLR(LAMMPS *lmp, int narg, char **arg)
    : FixElectrodeConp(lmp, narg, arg) {}

/* ----------------------------------------------------------------------
   run the base post_constructor (it validates the ffield settings and sets
   top_group and the ffield variables), then remove the `fix efield` it
   created -- under DPLR the field is applied by fix dplr instead.

   The base class never refers to that fix again, and delete_fix() only
   shifts entries above the deleted one, so the index Modify::add_fix() is
   holding for *this* fix stays valid.
------------------------------------------------------------------------- */

void FixElectrodeConpDPLR::post_constructor() {
  FixElectrodeConp::post_constructor();
  if (!ffield) {
    return;
  }
  const std::string efield_id = fixname + "_efield";
  if (modify->get_fix_by_id(efield_id)) {
    modify->delete_fix(efield_id);
  }
}

/* ----------------------------------------------------------------------
   snapshot / restore atom->f across a base-class call, so that the
   Gaussian correction contributes its energy but not its force.
   gausscorr() writes to both owned and ghost atoms, so cover nall.
------------------------------------------------------------------------- */

void FixElectrodeConpDPLR::save_forces() {
  const int nall = atom->nlocal + atom->nghost;
  f_saved.resize(static_cast<size_t>(nall) * 3);
  if (nall > 0) {
    memcpy(f_saved.data(), &atom->f[0][0],
           static_cast<size_t>(nall) * 3 * sizeof(double));
  }
}

void FixElectrodeConpDPLR::restore_forces() {
  const int nall = atom->nlocal + atom->nghost;
  // nall cannot grow inside the base call, but be defensive about shrinking
  const size_t n = std::min(f_saved.size(), static_cast<size_t>(nall) * 3);
  if (n > 0) {
    memcpy(&atom->f[0][0], f_saved.data(), n * sizeof(double));
  }
}

/* ---------------------------------------------------------------------- */

void FixElectrodeConpDPLR::setup_pre_reverse(int eflag, int vflag) {
  save_forces();
  FixElectrodeConp::setup_pre_reverse(eflag, vflag);
  restore_forces();
}

/* ---------------------------------------------------------------------- */

void FixElectrodeConpDPLR::pre_reverse(int eflag, int vflag) {
  save_forces();
  FixElectrodeConp::pre_reverse(eflag, vflag);
  restore_forces();
}
