// 个人恢复仅消费原战斗领域的永久伤因；此合同无状态、无写入且不改变军籍或伤亡账。
#pragma once
#include "growth_recovery_progress.hpp"
#include "resident/model/dynamic_contract.hpp"
namespace hanmo::resident {
enum class GrowthInjuryCauseKind { military_wound,personal_defeat };
struct GrowthInjuryCause {
    WorldIdentity source_identity;
    StableId actor_id,source_fact_id;
    Integer tick=0;
    GrowthInjuryCauseKind kind=GrowthInjuryCauseKind::military_wound;
    Integer magnitude=0;
    // 个人战只借原PersonalFact.result_id的终局分配号；battle头在开战分配、结束追加。
    // 旧Recovery消费可缺省；新个人入场前界必须具备。军伤即时casualty留空，无存档字段。
    StableId completion_id;
};
// 仅借已完整验证的同候选Military/Naval/Personal原账；缺原payload或未知写者须error。
// 返回该人物全部非零效应（含封顶而未再加重者），按Common真实事实序，不能扫今天wounded名册。
// phase仅严格from=Common.tick/to=from+1，读真实已写来源；缺phase则不收未来tick。
class GrowthInjurySourceRead {
public:
    virtual ~GrowthInjurySourceRead()=default;
    virtual Outcome<std::vector<GrowthInjuryCause>> injury_causes(const StableId& actor_id,const DynamicStepInput* phase=nullptr) const=0;
};
// 只核同候选戳、原事实身份和效应范围；实际actor_changes/defeated及原objective由原领域先核。
Status validate_growth_injury_cause(const CommonStore&,const GrowthInjuryCause&,const DynamicStepInput* phase=nullptr);
Status validate_growth_injury_cause_sequence(const CommonStore&,const StableId& actor_id,const std::vector<GrowthInjuryCause>&,const DynamicStepInput* phase=nullptr);
// 原军事max(1)和个人战min(3,current+N)只算一次；每份真实非零新伤打断未完成的本级恢复点，封顶也不免；不直接写Common。
Outcome<GrowthRecoveryProgress> apply_growth_injury_cause(const GrowthRecoveryProgress&,const GrowthInjuryCause&);
}
