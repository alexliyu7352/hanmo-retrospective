// 关闭前原看管链只从真实俘获、返营和转手正文复读，不取今天的 keeper 倒填过去。
#pragma once
#include "named_military_custody.hpp"
namespace hanmo::resident {
struct CommonActualFactPrefix;
class MilitaryReturnSourceRead;
struct OriginalNamedMilitaryCustodyAtRelease {
  const NamedMilitaryCustodyTerm *term = nullptr;
  const ArmyRecord *keeper_army = nullptr;
  const GarrisonDefinition *garrison = nullptr;
  SourceRef last_custody_source;
  const MilitaryCasualtyFact *actual_capture = nullptr;
};
Outcome<OriginalNamedMilitaryCustodyAtRelease>
read_named_military_custody_before_release(const NamedMilitaryCustodyContext &,
                                           const NamedMilitaryCustodyRelease &);
// Exact historical observation: the adopting S1 owner proves the M/prefix
// association. No fabricated release row and no current-keeper projection.
Outcome<OriginalNamedMilitaryCustodyAtRelease>
read_named_military_custody_at_original_prefix(
    const NamedMilitaryCustodyContext &, const NamedMilitaryCustodyTerm &,
    const WorldMoment &, const CommonActualFactPrefix &,
    const MilitaryReturnSourceRead *returns = nullptr);
} // namespace hanmo::resident
