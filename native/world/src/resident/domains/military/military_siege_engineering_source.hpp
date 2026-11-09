// 工程事实读取使用原军籍和真实围城参与来源；没有生命周期证明不得以当前armies补历史。
#pragma once
#include "military_siege_engineering.hpp"
#include "resident/domains/military/military_refs.hpp"
#include "resident/storage/common_store.hpp"
#include "resident/model/dynamic_contract.hpp"
namespace hanmo::resident {
struct MilitarySiegeParticipationObservation {
 WorldIdentity source_identity;
 StableId siege_id,topology_id,before_fact_id;
 Integer observed_tick=0,round=0;
 std::vector<StableId> unit_ids,lifecycle_source_ids;
};
// 本读由原围城创建/加入/退出永久链派生，不接受外部JSON名单或任意bool许可。
class MilitarySiegeParticipationRead {
public:
 virtual ~MilitarySiegeParticipationRead()=default;
 virtual Outcome<MilitarySiegeParticipationObservation> participation_before_engineering(const StableId& siege_id,const StableId& preparation_fact_id)const=0;
};
// 工程开始先只读原候选；真正追加Common头仍由一次原军旅命令/半日候选负责。
Outcome<MilitarySiegeEngineeringPreparation> preview_military_siege_engineering_preparation(
 const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const SiegeRecord&,
 const MilitarySiegeEngineeringSkillRead&,const DynamicStepInput&);
// 稳定只读报价使用今日已验证XP；不写准备头，真正siege_step仍重新冻结原phase资格。
Outcome<MilitarySiegeEngineeringPreparation> preview_current_military_siege_engineering_preparation(
 const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const SiegeRecord&,const MilitarySiegeEngineeringSkillRead&);
// 早门要求完整军事历史、真实生命周期和同界名册；晚门才借完整Family/Birth/Growth技艺。
Status validate_military_siege_engineering_bindings(
 const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitarySiegeEngineeringPreparation&,
 const MilitarySiegeParticipationRead&,const DynamicStepInput* phase=nullptr,
 const PopulationMilitaryState* population=nullptr,const PopulationMilitaryCatalog* population_catalog=nullptr);
Status validate_military_siege_engineering_actor_sources(
 const CommonStore&,const MilitarySiegeEngineeringPreparation&,const MilitarySiegeEngineeringSkillRead&);
}
