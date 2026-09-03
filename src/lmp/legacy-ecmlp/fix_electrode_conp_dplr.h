// SPDX-License-Identifier: LGPL-3.0-or-later
#ifdef FIX_CLASS
// clang-format off
FixStyle(electrode/conp/dplr, FixElectrodeConpDPLR);
// clang-format on
#else

#ifndef LMP_FIX_ELECTRODE_CONP_DPLR_H
#define LMP_FIX_ELECTRODE_CONP_DPLR_H

#include <vector>

#include "fix_electrode_conp.h"

namespace LAMMPS_NS {

/* ----------------------------------------------------------------------
   fix electrode/conp for use together with deepmd-kit's fix dplr.

   Two deviations from the base class:

   1. No internal `fix efield`.  With ffield yes the base class defines the
      variables <fixname>_ffield_vtop / _vbot / _zfield and then creates its
      own `fix efield` from them.  Under DPLR the field has to be applied by
      fix dplr instead (so that the force on the Wannier centroids is
      routed through the DipoleChargeModifier), so that internal fix is
      removed again.  The variables stay defined and can be fed to
      `fix ... dplr ... efield 0 0 v_<fixname>_ffield_zfield`.

   2. The Gaussian charge correction contributes energy (and virial) but no
      force.  Its force would land directly in atom->f, bypassing the same
      back-propagation.  FixElectrodeConp::gausscorr() is private in LAMMPS,
      so instead of calling it with fflag=false we let the base class run
      normally and undo its write to atom->f.  That is exactly equivalent:
      within pre_reverse()/setup_pre_reverse() the base only calls ev_init(),
      gausscorr() and self_energy(), and of those only gausscorr() -- and
      only under its `if (fflag)` branch -- ever touches atom->f.
------------------------------------------------------------------------- */

class FixElectrodeConpDPLR : public FixElectrodeConp {
 public:
  FixElectrodeConpDPLR(class LAMMPS *, int, char **);
  // drop the internally created fix efield
  void post_constructor() override;
  // keep the gausscorr energy, discard the force it applies
  void setup_pre_reverse(int, int) override;
  void pre_reverse(int, int) override;

 private:
  std::vector<double> f_saved;
  void save_forces();
  void restore_forces();
};

}  // namespace LAMMPS_NS

#endif
#endif
