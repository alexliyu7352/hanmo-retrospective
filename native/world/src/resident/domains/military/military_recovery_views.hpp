// 恢复页面只展现本人当前驻地的可见部曲及永久合格半日，不推演未来供给或自动发令。
#pragma once
#include "military_recovery_rules.hpp"
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident {
Outcome<BoundaryValue> military_wounded_recovery_page(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const StableId& observer);
Outcome<MilitaryRecoveryPolicy> decode_military_recovery_policy(const BoundaryValue&,const MilitaryCatalog&);
}
