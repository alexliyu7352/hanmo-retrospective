// 原真实草稿给操作顺序；原业务给合法原因。二者缺一不可，原source头可能早于或晚于写入。
#include "growth_condition_binding.hpp"
#include "growth_study_condition_source.hpp"
#include "growth_training_condition_source.hpp"
#include "growth_recovery_condition_source.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
namespace hanmo::resident {
struct GrowthConditionWriteBoundary::State {
    const CommonStore* common=nullptr;
    std::shared_ptr<const ActorConditionCaptureIdentity> capture;
    WorldIdentity identity;
    GrowthPersonalConditionBusinessKey business;
    DynamicStepInput step;
    std::size_t offset=0;
    Integer next_instance=0;
    bool consumed=false;
};
namespace {
bool identity(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
bool key(const GrowthPersonalConditionBusinessKey& a,const GrowthPersonalConditionBusinessKey& b){return a.kind==b.kind&&a.original_business_key==b.original_business_key&&a.effective_tick==b.effective_tick&&a.ordinal==b.ordinal;}
Status check_boundary(CommonStore& c,const std::shared_ptr<GrowthConditionWriteBoundary::State>& p,GrowthPersonalConditionBusiness kind){if(!p||p->consumed||p->common!=&c||!p->capture||p->capture!=c.actor_condition_capture_identity()||!identity(p->identity,c.state().identity)||p->business.kind!=kind||c.state().next_instance_id<p->next_instance)return common_failure("GrowthConditionWriteBoundaryRequired");return common_success();}
Outcome<GrowthConditionBoundWrite> actual(CommonStore& c,GrowthConditionWriteBoundary::State& state,GrowthPersonalConditionCause cause){
 auto fail=[&](const StableId& code)->Outcome<GrowthConditionBoundWrite>{return {false,{},common_failure(code,state.business.original_business_key).error};};
 if(!identity(cause.source_identity,state.identity)||!key(cause.business,state.business))return fail("GrowthConditionBusinessMismatch");
 const auto& drafts=c.actor_condition_execution_drafts();if(state.offset>drafts.size()||drafts.size()-state.offset!=cause.actor_ids.size()||cause.actor_ids.empty())return fail("GrowthConditionActualWriteCountMismatch");
 GrowthConditionBoundWrite out;out.capture_identity=state.capture;out.cause=std::move(cause);out.actual.source_identity=state.identity;out.actual.business=state.business;out.actual.capture_sequence=Integer(state.offset+1);Integer prior_id=state.next_instance;
 for(std::size_t n=state.offset;n<drafts.size();++n){const auto& row=drafts[n];if(row.sequence!=Integer(n+1)||row.kind!=ActorConditionExecutionDraft::Kind::mutation||!row.before||!identity(row.identity,state.identity)||row.actor_id!=out.cause.actor_ids[n-state.offset]||(!row.hp_written&&!row.fatigue_written)||(!row.hp_written&&row.before->hp!=row.after.hp)||(!row.fatigue_written&&row.before->fatigue!=row.after.fatigue)||row.next_instance_before<prior_id||row.next_instance_after!=row.next_instance_before||row.next_instance_after>c.state().next_instance_id)return fail("GrowthConditionActualWriteMismatch");
  GrowthPersonalConditionActorEffect effect;effect.actor_id=row.actor_id;if(row.hp_written){effect.before.hp=row.before->hp;effect.after.hp=row.after.hp;}if(row.fatigue_written){effect.before.fatigue=row.before->fatigue;effect.after.fatigue=row.after.fatigue;}out.actual.actors.push_back(std::move(effect));prior_id=row.next_instance_after;
 }
 return {true,std::move(out),{}};
}
}
Outcome<GrowthConditionWriteBoundary> begin_growth_condition_write_in_candidate(CommonStore& c,const GrowthPersonalConditionBusinessKey& business,const DynamicStepInput& step){
 auto fail=[](const StableId& code)->Outcome<GrowthConditionWriteBoundary>{return {false,{},common_failure(code).error};};
 if(c.tick()<0||c.tick()>=9007199254740991LL||step.from_tick!=c.tick()||step.to_tick!=c.tick()+1||business.effective_tick!=step.to_tick||business.ordinal<1||std::find(step.existing_action_ids.begin(),step.existing_action_ids.end(),business.original_business_key)==step.existing_action_ids.end())return fail("GrowthConditionActualStepRequired");
 const auto* action=c.action(business.original_business_key);if(!action||action->id!=business.original_business_key)return fail("GrowthConditionOriginalActionRequired");
 const bool kind=business.kind==GrowthPersonalConditionBusiness::study_lesson?(action->kind=="study_skill"||action->kind=="teach_study"):business.kind==GrowthPersonalConditionBusiness::training_completion?(action->kind=="learn_skill"||action->kind=="teach_skill"||action->kind=="practice_skill"):business.kind==GrowthPersonalConditionBusiness::recovery_credit?action->kind=="service":false;
 if(!kind)return fail("GrowthConditionBusinessKindUnsupported");
 // Training成功分支在原apply_action扣至0之后、Actor写之前；其余两原步骤尚未扣时。
 if(business.kind==GrowthPersonalConditionBusiness::training_completion?action->remaining_ticks!=0:action->remaining_ticks<1)return fail("GrowthConditionOriginalActionPhaseMismatch");auto captured=c.actor_condition_capture_identity();if(!captured)return fail("GrowthConditionActualCaptureRequired");
 auto state=std::make_shared<GrowthConditionWriteBoundary::State>();state->common=&c;state->capture=std::move(captured);state->identity=c.state().identity;state->business=business;state->step=step;state->offset=c.actor_condition_execution_drafts().size();state->next_instance=c.state().next_instance_id;GrowthConditionWriteBoundary out;out.state_=std::move(state);return {true,std::move(out),{}};
}
Outcome<GrowthConditionBoundWrite> bind_growth_study_condition_in_candidate(CommonStore& c,const GrowthState& growth,GrowthConditionWriteBoundary& marked){
 auto valid=check_boundary(c,marked.state_,GrowthPersonalConditionBusiness::study_lesson);if(!valid.ok)return {false,{},valid.error};auto& p=*marked.state_;auto cause=read_growth_study_condition_cause(c.state(),growth,p.business.original_business_key,p.business.ordinal,&p.step);if(!cause.ok)return {false,{},cause.error};auto result=actual(c,p,std::move(cause.value));if(!result.ok)return result;valid=validate_growth_study_condition_effects(c.state(),growth,result.value.cause,result.value.actual.actors,&p.step);if(!valid.ok)return {false,{},valid.error};p.consumed=true;return result;
}
Outcome<GrowthConditionBoundWrite> bind_growth_training_condition_in_candidate(CommonStore& c,const GrowthState& growth,GrowthConditionWriteBoundary& marked){
 auto valid=check_boundary(c,marked.state_,GrowthPersonalConditionBusiness::training_completion);if(!valid.ok)return {false,{},valid.error};auto& p=*marked.state_;auto cause=read_growth_training_condition_cause(c.state(),growth,p.business.original_business_key,&p.step);if(!cause.ok)return {false,{},cause.error};auto result=actual(c,p,std::move(cause.value));if(!result.ok)return result;valid=validate_growth_training_condition_effects(c.state(),growth,result.value.cause,result.value.actual.actors,&p.step);if(!valid.ok)return {false,{},valid.error};p.consumed=true;return result;
}
Outcome<GrowthConditionBoundWrite> bind_growth_recovery_condition_in_candidate(CommonStore& c,const GrowthState& growth,const GrowthRecoveryCatalog& catalog,const GrowthRecoveryState& recovery,const StableId& credit,GrowthConditionWriteBoundary& marked){
 auto valid=check_boundary(c,marked.state_,GrowthPersonalConditionBusiness::recovery_credit);if(!valid.ok)return {false,{},valid.error};auto& p=*marked.state_;auto cause=read_growth_recovery_condition_cause(c.state(),growth,catalog,recovery,credit,&p.step);if(!cause.ok)return {false,{},cause.error};auto result=actual(c,p,std::move(cause.value));if(!result.ok)return result;valid=validate_growth_recovery_condition_effects(c.state(),growth,catalog,recovery,result.value.cause,result.value.actual.actors,&p.step);if(!valid.ok)return {false,{},valid.error};p.consumed=true;return result;
}
}
