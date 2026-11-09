#pragma once
#include "naval_repair_exact.hpp"
namespace hanmo::resident {
struct ExactNavalRepairSourceContext {
 const MilitaryCatalog& military_catalog;const MilitaryAccess& access;
 const NavalCatalog& naval_catalog;const TimeScaleRef& scale;const CurrentMomentRead& clock;
 ExactArithmeticBudget budget{};
};
// Source-local original value/FSL check for the Naval asset predecessor fold.
// Deliberately does not call the full repair OriginalRead: that reader consumes
// the asset fold itself. This result never grants acceptance or service rights.
Outcome<ExactNavalRepairSourceView> read_exact_naval_repair_asset_source(
 const CommonStore&,const NavalState&,const MilitaryActionTimeState&,
 const ActionElapsedStore&,const ExactNavalRepairSourceContext&,const SourceRef&,Integer schema);
}
