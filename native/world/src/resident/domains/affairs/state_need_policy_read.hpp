#pragma once
#include "state_need_catalog.hpp"
#include "resident/domains/family/care_requirement_rules.hpp"
#include "resident/domains/tasks/state_need_task.hpp"
namespace hanmo::resident {
// S10 must supply the actual Medical owner mapping. There is no generic goal-id
// lookup, fallback to Care, or default first policy in the catalogue.
class MedicalStateNeedPolicyRead {public:virtual ~MedicalStateNeedPolicyRead()=default;
 virtual bool bound_to(const CommonStore&)const noexcept=0;
 virtual Status validate_medical_state_need_policy(const StateNeedPolicy&,const CatalogIdentity&)const=0;
 virtual Outcome<const StableId*> read_medical_goal_policy_id(const MedicalResolutionGoal&)const=0;
};
struct StateNeedPolicySourceContext {
 const CommonStore& common;const CatalogIdentity& identity;const StateNeedCatalog& catalog;
 const StateTaskSourceRead& tasks;
 const CareRequirementCatalog* care_catalog=nullptr;const CareGoalSourceContext* care=nullptr;
 const MedicalStateNeedPolicyRead* medical=nullptr;
};
// All values are borrowed from the same locked catalogue and actual goal owner.
// The original Care mapping re-reads its immutable rule, not a mutable Need or Task.
class NativeStateNeedPolicyRead final:public StateNeedPolicyRead {
 StateNeedPolicySourceContext context_;
public:
 explicit NativeStateNeedPolicyRead(StateNeedPolicySourceContext x):context_(x){}
 bool bound_to(const CommonStore&c)const noexcept{return &context_.common==&c;}
 const CatalogIdentity& state_need_catalog_identity()const override{return context_.identity;}
 Status validate_sources()const;
 Outcome<const StateNeedPolicy*> state_need_policy(const StateGoalRef&)const override;
};
}
