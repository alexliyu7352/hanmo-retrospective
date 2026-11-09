// 围城到期只终止同一真实节点会战；来源为原M17本轮advanced、准备及release，不放宽wall资格。
#pragma once
#include "resident/domains/military/military_refs.hpp"
namespace hanmo::resident {
struct MilitarySiegeTimeoutRelease {
 StableId siege_id,battle_id,contact_source_id,lifecycle_source_id,preparation_source_id;
 Integer tick=0;
};
// 只选择显式M17实际围城；合包中的别处新目录不能改变旧野战/旧围城。
bool military_siege_timeout_in_scope(const MilitaryState&,const MilitaryCatalog&,const MilitaryBattleRecord&);
// 仅原preclock单步可读本轮终态，返回empty代表该会战没有本次timeout释放。
Outcome<std::optional<MilitarySiegeTimeoutRelease>> read_military_siege_timeout_release(
 const CommonState&,const MilitaryState&,const MilitaryCatalog&,const MilitaryBattleRecord&,const DynamicStepInput&);
// 完整M17结构先验后核原终止回执与释放的一一对应，稳定档不得留下半释放会战。
Status validate_military_siege_timeout_releases(
 const CommonState&,const MilitaryState&,const MilitaryCatalog&,const DynamicStepInput* phase=nullptr);
}
