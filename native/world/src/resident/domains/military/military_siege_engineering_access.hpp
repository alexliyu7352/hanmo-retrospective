// 唯一军事状态上的围城接缝；只借同候选Common/原目录/技能读，不持完整World或复制另一世界。
#pragma once
#include "military_siege_engineering_history.hpp"
#include "military_siege_timeout_source.hpp"
#include "military_siege_engineering_rules.hpp"
#include "military_siege_lifecycle_commands.hpp"
namespace hanmo::resident {
class MilitarySiegeEngineeringAccess {
 CommonStore& common_;MilitaryState& military_;const MilitaryCatalog& catalog_;
 const MilitarySiegeEngineeringSkillRead& skills_;std::optional<DynamicStepInput> phase_;
 Status selected(const StableId& topology_id)const;
public:
 // phase按值冻结，只能来自当前真实Advance栈；所有指向状态的引用不跨候选使用。
 MilitarySiegeEngineeringAccess(CommonStore&,MilitaryState&,const MilitaryCatalog&,const MilitarySiegeEngineeringSkillRead&,const DynamicStepInput* phase=nullptr);
 // 仅本轮原advanced终态可提早释放节点战，不把结果状态当通用终止授权。
 Outcome<std::optional<MilitarySiegeTimeoutRelease>> timeout_release(const MilitaryBattleRecord&)const;
 Outcome<MilitarySiegeEngineeringPreparation> preview(const SiegeRecord&)const;
 Outcome<MilitarySiegeEngineeringPreparation> prepare(const SiegeRecord&,Integer finish_tick);
 Outcome<SiegeRecord> create(const SiegeCreateRequest&,const SiegeTopology&,const SiegeObservation&,const StableId& actor,const StableId& arrival);
 Outcome<SiegeRecord> join(const SiegeRecord&,const SiegeTopology&,const SiegeObservation&,const StableId& actor,const StableId& army,Integer side,const StableId& arrival);
 Outcome<SiegeResolution> step(const SiegeRecord&,const SiegeTopology&,const SiegeObservation&,const StableId& actor,const SiegeOrders&,Integer finish_tick);
 Outcome<SiegeRecord> finish_battle(const SiegeRecord&,const SiegeTopology&,const SiegeObservation&,const StableId& node,const StableId& battle);
 Outcome<SiegeResolution> manage(const SiegeRecord&,const SiegeTopology&,const SiegeObservation&,const StableId& actor,const SiegeStrategicCommand&);
 Outcome<SiegeResolution> terminate(const SiegeRecord&,const SiegeTopology&,const SiegeObservation&,const StableId& reason);
 Outcome<SiegeRecord> close(const SiegeRecord&,const SiegeTopology&,const StableId& actor);
 Status departed(const SiegeRecord& before,const SiegeRecord& after,const SiegeTopology&);
 Status integrated(const SiegeRecord& before,const SiegeRecord& after,const SiegeTopology&,const StableId& transfer,Integer tick);
};
}
