// SPDX-License-Identifier: LGPL-3.0-or-later
/* ----------------------------------------------------------------------
   The bodies below are a verbatim copy of
   LAMMPS/src/ELECTRODE/fix_electrode_conq.cpp, re-parented onto
   FixElectrodeConpDPLR so that the DPLR force routing is inherited rather
   than duplicated.  Re-sync when upstream changes that file.

   Contributing authors of the copied code:
     Ludwig Ahrens-Iwers (TUHH), Shern Tee (UQ), Robert Meissner (TUHH)
------------------------------------------------------------------------- */

#include "fix_electrode_conq_dplr.h"

#include "comm.h"
#include "error.h"
#include "group.h"
#include "input.h"
#include "variable.h"

using namespace LAMMPS_NS;

/* ---------------------------------------------------------------------- */

FixElectrodeConqDPLR::FixElectrodeConqDPLR(LAMMPS *lmp, int narg, char **arg)
    : FixElectrodeConpDPLR(lmp, narg, arg) {
  // FixElectrodeConp rejects qtotal by comparing the style string against
  // "electrode/conq" exactly, which our style name does not match.  Re-arm
  // the guard here: qtotal would otherwise be accepted and would override
  // the per-group charge constraint below.
  if (qtotal_var_style != VarStyle::UNSET) {
    error->all(FLERR, "qtotal keyword not available for {}", style);
  }

  // copy const-style values across because update_psi will change group_psi
  group_q = group_psi_const;

  if (symm) {
    if (num_of_groups == 1) {
      error->all(
          FLERR,
          "Keyword symm on not allowed in electrode/conq with only one "
          "electrode");
    }
    if (comm->me == 0) {
      error->warning(FLERR,
                     "Fix electrode/conq with keyword symm ignores the charge "
                     "setting for the last electrode listed");
    }
    if (algo != Algo::MATRIX_INV) {
      double last_q = 0.;
      for (int g = 0; g < num_of_groups - 1; g++) {
        last_q -= group_q[g];
      }
      group_q.back() = last_q;  // needed for CG algos
    }
  }
}

/* ---------------------------------------------------------------------- */

void FixElectrodeConqDPLR::update_psi() {
  const int numsymm = num_of_groups - ((symm) ? 1 : 0);
  bool symm_update_back = false;
  for (int g = 0; g < numsymm; g++) {
    if (group_psi_var_styles[g] == VarStyle::CONST) {
      continue;
    }
    group_q[g] = input->variable->compute_equal(group_psi_var_ids[g]);
    symm_update_back = true;
  }
  if (algo == Algo::MATRIX_INV) {
    std::vector<double> group_remainder_q(num_of_groups, 0.);
    for (int g = 0; g < numsymm; g++) {
      group_remainder_q[g] = group_q[g] - sb_charges[g];
    }
    for (int g = 0; g < num_of_groups; g++) {
      double vtmp = 0;
      for (int h = 0; h < num_of_groups; h++) {
        vtmp += macro_elastance[g][h] * group_remainder_q[h];
      }
      group_psi[g] = vtmp;
    }
  } else {
    if (symm && symm_update_back) {  // needed for CG algos
      double last_q = 0.;
      for (int g = 0; g < num_of_groups - 1; g++) {
        last_q -= group_q[g];
      }
      group_q.back() = last_q;
    }
    for (double &g : group_psi) {
      g = 0;
    }
  }
}

/* ----------------------------------------------------------------------
   Correct charge of each electrode to target charge by adding a homogeneous
   charge
------------------------------------------------------------------------- */

std::vector<double>
FixElectrodeConqDPLR::constraint_correction(std::vector<double> x) {
  const int n = x.size();
  auto sums = std::vector<double>(num_of_groups, 0);
  for (int i = 0; i < n; i++) {
    sums[iele_to_group_local[i]] += x[i];
  }
  MPI_Allreduce(MPI_IN_PLACE, sums.data(), num_of_groups, MPI_DOUBLE, MPI_SUM,
                world);
  for (int g = 0; g < num_of_groups; g++) {
    sums[g] -= group_q[g];
    sums[g] /= group->count(groups[g]);
  }
  for (int i = 0; i < n; i++) {
    x[i] -= sums[iele_to_group_local[i]];
  }
  return x;
}

/* ----------------------------------------------------------------------
   Project into direction that conserves charge of each electrode
   (cf. M. Shariff (1995))
------------------------------------------------------------------------- */

std::vector<double>
FixElectrodeConqDPLR::constraint_projection(std::vector<double> x) {
  const int n = x.size();
  auto sums = std::vector<double>(num_of_groups, 0);
  for (int i = 0; i < n; i++) {
    sums[iele_to_group_local[i]] += x[i];
  }
  MPI_Allreduce(MPI_IN_PLACE, sums.data(), num_of_groups, MPI_DOUBLE, MPI_SUM,
                world);
  for (int g = 0; g < num_of_groups; g++) {
    sums[g] /= group->count(groups[g]);
  }
  for (int i = 0; i < n; i++) {
    x[i] -= sums[iele_to_group_local[i]];
  }
  return x;
}

/* ----------------------------------------------------------------------
   Recompute group potential as average for output if using cg algo
------------------------------------------------------------------------- */

void FixElectrodeConqDPLR::recompute_potential(
    const std::vector<double> &b, const std::vector<double> &q_local) {
  const int n = b.size();
  auto a = ele_ele_interaction(q_local);
  auto psi_sums = std::vector<double>(num_of_groups, 0);
  for (int i = 0; i < n; i++) {
    psi_sums[iele_to_group_local[i]] += (a[i] + b[i]) / evscale;
  }
  MPI_Allreduce(MPI_IN_PLACE, psi_sums.data(), num_of_groups, MPI_DOUBLE,
                MPI_SUM, world);
  for (int g = 0; g < num_of_groups; g++) {
    group_psi[g] = psi_sums[g] / group->count(groups[g]);
  }
}
