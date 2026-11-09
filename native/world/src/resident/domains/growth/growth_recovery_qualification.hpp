#include <functional>
// 正规作者明确设施资格，原服务履约与具名军事空闲源共同决定可恢复；不由今日技能推医馆。
#pragma once
#include "growth_recovery_model.hpp"
#include "resident/domains/growth/growth_refs.hpp"
#include "resident/domains/military/military_actor_occupation.hpp"
namespace hanmo::resident {
// before须借同候选且已验完整军史；热to边界由World绑定原严格phase重载，不伪造Common时钟。
struct GrowthRecoveryOccupationRead {
    std::function<Outcome<MilitaryActorOccupation>(const StableId&)> current;
    std::function<Outcome<MilitaryActorOccupation>(const StableId&,Integer,const StableId&)> before;
};
Outcome<GrowthRecoveryCatalog> decode_growth_recovery_catalog(const BoundaryValue&,const CommonCatalog&,const GrowthCatalog&);
Status validate_growth_recovery_catalog(const CommonCatalog&,const GrowthCatalog&,const GrowthRecoveryCatalog&);
// current用于开始与报价；每期完成使用已生成实际credit的before入口核，不用今日岗位倒填旧期。
// 显式phase仅可观察同候选严格from→to；未传phase的稳定口继续只认Common.tick。
Status validate_growth_recovery_occupation_now(const CommonStore&,const GrowthRecoveryOccupationRead&,const StableId& actor,const DynamicStepInput* phase=nullptr);
Status validate_growth_recovery_qualification_now(const CommonStore&,const CommonCatalog&,const GrowthCatalog&,const GrowthRecoveryCatalog&,const GrowthState&,const GrowthRecoveryOccupationRead&,const GrowthRecoveryService&);
Status validate_growth_recovery_qualification_now(const CommonStore&,const CommonCatalog&,const GrowthCatalog&,const GrowthRecoveryCatalog&,const GrowthState&,const GrowthRecoveryOccupationRead&,const GrowthRecoveryService&,const DynamicStepInput&);
Status validate_growth_recovery_qualification_before(const CommonStore&,const CommonCatalog&,const GrowthCatalog&,const GrowthRecoveryCatalog&,const GrowthState&,const GrowthRecoveryState&,const GrowthRecoveryOccupationRead&,const GrowthRecoveryService&,Integer,const StableId& actual_credit_fact_id);
}
