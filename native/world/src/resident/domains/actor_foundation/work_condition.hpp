// WorkOrStudyExertion只绑定原Growth一次实际写，不让调用者提供疲劳delta。
#pragma once
#include "resident/domains/actor_foundation/actor_foundation_refs.hpp"
#include "condition_capture.hpp"
#include "resident/domains/growth/growth_fixed_execution.hpp"
namespace hanmo::resident {
class GrowthWorkConditionBoundary {
public:struct State;GrowthWorkConditionBoundary()=default;
private:std::shared_ptr<State> state_;
 friend Outcome<GrowthWorkConditionBoundary> begin_growth_work_condition(CommonStore&,const AuthoredActorFoundationState&,const ActorFoundationCatalog&,const GrowthState&,const ActionCoreView&,const GrowthFixedStepPermit&);
 friend Outcome<std::vector<BoundActorConditionWrite>> bind_growth_work_condition(CommonStore&,AuthoredActorFoundationState&,const ActorFoundationCatalog&,const GrowthState&,const GrowthCatalog&,const CommonCatalog&,const ActionElapsedStore&,const TimeScaleRef&,const CatalogIdentity&,const ActionTimeRegistration&,const WorldMoment&,GrowthWorkConditionBoundary&);
};
Outcome<GrowthWorkConditionBoundary> begin_growth_work_condition(CommonStore&,const AuthoredActorFoundationState&,const ActorFoundationCatalog&,const GrowthState&,const ActionCoreView&,const GrowthFixedStepPermit&);
// 原结果/身体已实际应用后立即绑定，仍同一capture候选。随后原kernel释放并采用FSL终态；
// 完整coverage才从该真实terminal/F历史冷反验，禁止用收据自身代替原结果。
Outcome<std::vector<BoundActorConditionWrite>> bind_growth_work_condition(CommonStore&,AuthoredActorFoundationState&,const ActorFoundationCatalog&,const GrowthState&,const GrowthCatalog&,const CommonCatalog&,const ActionElapsedStore&,const TimeScaleRef&,const CatalogIdentity&,const ActionTimeRegistration&,const WorldMoment&,GrowthWorkConditionBoundary&);
Status validate_growth_work_condition_receipt(const CommonStore&,const ActorFoundationCatalog&,const GrowthState&,const GrowthCatalog&,const CommonCatalog&,const ActionElapsedStore&,const TimeScaleRef&,const CatalogIdentity&,const ActionTimeRegistration&,const WorldMoment&,const ConditionReceiptV2&,const InteractionState* interaction=nullptr,const InteractionCatalog* interaction_catalog=nullptr);
Status validate_growth_work_condition_coverage(const CommonStore&,const AuthoredActorFoundationState&,const ActorFoundationCatalog&,const GrowthState&,const GrowthCatalog&,const CommonCatalog&,const ActionElapsedStore&,const TimeScaleRef&,const CatalogIdentity&,const std::vector<ActionTimeRegistration>&,const WorldMoment&,const InteractionState* interaction=nullptr,const InteractionCatalog* interaction_catalog=nullptr);
}
