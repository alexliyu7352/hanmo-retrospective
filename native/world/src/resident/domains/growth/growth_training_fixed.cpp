#include "resident/domains/growth/growth_model.hpp"
#include "growth_training_fixed.hpp"
namespace hanmo::resident {
Outcome<GrowthFixedStudyApplication> apply_training_fixed_gate_in_candidate(CommonStore& common,GrowthState& growth,AuthoredActorFoundationState& foundation,const CharacterConditionSources& body,const GrowthCatalog& catalog,const CommonCatalog& shared,SocialState& social,InteractionState& interaction,const GrowthAccess& access,const ActionTimeRegistration& registration,const GrowthFixedStepPermit& gate){
 auto failure=[](const RuleError& error)->Outcome<GrowthFixedStudyApplication>{return {false,{},error};};
 if(&body.common!=&common||&body.state!=&foundation||body.growth!=&growth||body.growth_catalog!=&catalog||body.shared!=&shared||!body.elapsed||!body.scale)return failure(common_failure("GrowthTrainingSameCandidateRequired",gate.action_id()).error);
 const auto* action=common.action(gate.action_id());if(!action||(action->kind!="learn_skill"&&action->kind!="practice_skill"&&action->kind!="teach_skill")||registration.owner_domain!="growth"||registration.action_kind!=action->kind||!gate.final_gate())return failure(common_failure("GrowthTrainingActualGateRequired",gate.action_id()).error);
 auto clock=compare_world_moments(gate.at(),body.actual_now);if(!clock.ok)return failure(clock.error);if(clock.value)return failure(common_failure("GrowthTrainingActualClockRequired",gate.action_id()).error);
 auto before=begin_growth_work_condition(common,foundation,body.catalog,growth,*action,gate);if(!before.ok)return failure(before.error);
 auto result=step_growth_at_fixed_gate(common,growth,catalog,shared,social,interaction,access,*action,gate);if(!result.ok)return failure(result.error);
 if(!result.value.completed)return failure(common_failure("GrowthTrainingResultNotTerminal",gate.action_id()).error);
 auto actual=bind_growth_work_condition(common,foundation,body.catalog,growth,catalog,shared,*body.elapsed,*body.scale,body.catalog.identity,registration,body.actual_now,before.value);if(!actual.ok)return failure(actual.error);
 return {true,GrowthFixedStudyApplication{std::move(result.value),std::move(actual.value)},{}};
}
}
