// 同候选已落接管的有限来源；它只证明真实资产变化，不授政治任职或责任承担权限。
#pragma once
#include "naval_battle_state.hpp"
namespace hanmo::resident {
struct NavalAppliedCaptureSource {
 StableId capture_id,round_source_id,actor_id,vessel_id,inventory_id,from_army_id,to_army_id,authority_source;
 Integer tick=0;Owner from_owner,to_owner;std::vector<NavalCapturedCargo> cargo;
};
// 仅供完成capture后的同一零时长World候选使用；Common fact、现值和完整原来源都须匹配。
// 调用前完整Naval历史已验证，本口不接受caller提供事实内容或任意历史limit_tick。
Outcome<NavalAppliedCaptureSource> naval_capture_applied_in_candidate(const CommonRead&,const MilitaryState&,const MilitaryAccess&,const NavalState&,const StableId& capture_id);
}
