// SPDX-License-Identifier: LGPL-3.0-or-later
/* ----------------------------------------------------------------------
   Plugin entry point for the constant-potential DPLR fixes.

   These two styles derive from the LAMMPS ELECTRODE package and have no
   deepmd-kit dependency at all, which is why they ship as their own plugin
   rather than as part of a deepmd-kit patch.  The kspace style they are
   used with, pppm/electrode/dplr, does have one -- FixDPLR::post_force()
   has to name its type to call get_fele() -- and so lives in deepmd-kit,
   registered by deepmdplugin.cpp.  Do not register it here as well.
------------------------------------------------------------------------- */

#include "lammpsplugin.h"
#include "version.h"

#include "fix_electrode_conp_dplr.h"
#include "fix_electrode_conq_dplr.h"

namespace LAMMPS_NS {

static Fix *FixElectrodeConpDPLR_creator(LAMMPS *lmp, int narg, char **arg) {
  return new FixElectrodeConpDPLR(lmp, narg, arg);
}

static Fix *FixElectrodeConqDPLR_creator(LAMMPS *lmp, int narg, char **arg) {
  return new FixElectrodeConqDPLR(lmp, narg, arg);
}

}  // namespace LAMMPS_NS

extern "C" void lammpsplugin_init(void *lmp_ptr, void *handle, void *regfunc) {
  lammpsplugin_regfunc register_plugin = (lammpsplugin_regfunc)regfunc;
  lammpsplugin_t plugin;

  plugin.version = LAMMPS_VERSION;
  plugin.handle = handle;
  plugin.author = "Jia-Xin Zhu";

  plugin.style = "fix";
  plugin.name = "electrode/conp/dplr";
  plugin.info = "fix electrode/conp/dplr";
  plugin.creator.v2 =
      (lammpsplugin_factory2 *)&LAMMPS_NS::FixElectrodeConpDPLR_creator;
  (*register_plugin)(&plugin, lmp_ptr);

  plugin.style = "fix";
  plugin.name = "electrode/conq/dplr";
  plugin.info = "fix electrode/conq/dplr";
  plugin.creator.v2 =
      (lammpsplugin_factory2 *)&LAMMPS_NS::FixElectrodeConqDPLR_creator;
  (*register_plugin)(&plugin, lmp_ptr);
}
