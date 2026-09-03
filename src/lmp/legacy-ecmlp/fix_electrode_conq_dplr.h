// SPDX-License-Identifier: LGPL-3.0-or-later
#ifdef FIX_CLASS
// clang-format off
FixStyle(electrode/conq/dplr, FixElectrodeConqDPLR);
// clang-format on
#else

#ifndef LMP_FIX_ELECTRODE_CONQ_DPLR_H
#define LMP_FIX_ELECTRODE_CONQ_DPLR_H

#include <vector>

#include "fix_electrode_conp_dplr.h"

namespace LAMMPS_NS {

/* ----------------------------------------------------------------------
   fix electrode/conq for use together with deepmd-kit's fix dplr.

   This is to fix electrode/conq what FixElectrodeConpDPLR is to
   fix electrode/conp, but it is not derived from FixElectrodeConq: it
   derives from FixElectrodeConpDPLR, so that the DPLR force routing lives
   in exactly one place and any change made there reaches conq as well.

       FixElectrodeConp                     (LAMMPS ELECTRODE)
             |
       FixElectrodeConpDPLR                 the two DPLR deviations
             |
       FixElectrodeConqDPLR                 constant-charge constraint

   The price is that the five members below are a verbatim copy of
   LAMMPS/src/ELECTRODE/fix_electrode_conq.cpp.  They must be re-synced
   whenever upstream changes that file.  Everything they touch is protected
   in FixElectrodeConp, so the copy compiles unchanged in a grandchild.

   Contributing authors of the copied code:
     Ludwig Ahrens-Iwers (TUHH), Shern Tee (UQ), Robert Meissner (TUHH)
------------------------------------------------------------------------- */

class FixElectrodeConqDPLR : public FixElectrodeConpDPLR {
 public:
  FixElectrodeConqDPLR(class LAMMPS *, int, char **);

 protected:
  void update_psi() override;
  void recompute_potential(const std::vector<double> &,
                           const std::vector<double> &) override;
  std::vector<double> constraint_projection(std::vector<double>) override;
  std::vector<double> constraint_correction(std::vector<double>) override;

 private:
  std::vector<double> group_q;
};

}  // namespace LAMMPS_NS

#endif
#endif
