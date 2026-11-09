#include "study_lesson_capability.hpp"
#include "growth_internal.hpp"
#include "resident/domains/actor_foundation/authored_origin.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include <algorithm>
#include <type_traits>
namespace hanmo::resident {
struct PreparedStudyLessonCapability::State {
 const CommonStore* common=nullptr;const GrowthState* growth=nullptr;const AuthoredActorFoundationState* foundation=nullptr;
 std::shared_ptr<const ActorConditionCaptureIdentity> capture;ActorConditionSavepointBoundary savepoint;WorldIdentity identity;
 CharacterCapabilityView student,teacher;LessonCapabilitySource result;bool consumed=false;
};
namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> result){if(!result.ok)throw Failure{std::move(result.error)};return std::move(result.value);}
template<class T,class F>Outcome<T> guarded(F fn){try{return {true,fn(),{}};}catch(Failure& e){return {false,{},std::move(e.error)};}}
bool identity(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
bool source_equal(const CharacterConditionSourceRef& a,const CharacterConditionSourceRef& b){if(a.index()!=b.index())return false;if(const auto* seed=std::get_if<InitialConditionSeedRef>(&a))return seed->seed_key==std::get<InitialConditionSeedRef>(b).seed_key;return std::get<ConditionReceiptRefV2>(a).result_id==std::get<ConditionReceiptRefV2>(b).result_id;}
const CharacterConditionSourceRef& study_source_for_dedup(const CharacterConditionSourceRef& source)noexcept{return source;}
const CharacterConditionSourceRef& study_source_for_dedup(const CharacterConditionSourceRef* source)noexcept{return *source;}
template<class Entry,class Allocator>void add_source(std::vector<Entry,Allocator>& list,const Entry& source){static_assert(std::is_same_v<Entry,CharacterConditionSourceRef>||std::is_same_v<Entry,const CharacterConditionSourceRef*>);for(const auto& previous:list)if(source_equal(study_source_for_dedup(previous),study_source_for_dedup(source)))return;list.push_back(source);}
FrozenLessonCapability freeze(const CharacterCapabilityView& source){FrozenLessonCapability out;out.actor_id=source.actor_id;out.purpose_ref=source.purpose_ref;out.condition_revision=source.condition_head.condition_revision;out.origin_ref=source.actor_id;out.registered_efficiency_components=source.registered_efficiency_components;out.frozen_efficiency_bp=source.efficiency_bp;out.admission_rule_ref=source.rule_ref;for(const auto& component:source.registered_efficiency_components)add_source(out.condition_source_refs,component.original_source_ref);for(const auto& ref:source.limiting_condition_refs)add_source(out.condition_source_refs,ref);return out;}
bool frozen_equal(const FrozenLessonCapability& actual,const CharacterCapabilityView& expected){CheckpointVector<const CharacterConditionSourceRef*> other_sources;for(const auto& component:expected.registered_efficiency_components)add_source(other_sources,&component.original_source_ref);for(const auto& ref:expected.limiting_condition_refs)add_source(other_sources,&ref);if(actual.actor_id!=expected.actor_id||!actor_foundation_rule_equal(actual.purpose_ref,expected.purpose_ref)||actual.condition_revision!=expected.condition_head.condition_revision||actual.origin_ref!=expected.actor_id||!actual.attribute_milestone_refs.empty()||actual.condition_source_refs.size()!=other_sources.size()||actual.registered_efficiency_components.size()!=expected.registered_efficiency_components.size()||actual.frozen_efficiency_bp!=expected.efficiency_bp||!actor_foundation_rule_equal(actual.admission_rule_ref,expected.rule_ref))return false;for(std::size_t i=0;i<other_sources.size();++i)if(!source_equal(actual.condition_source_refs[i],*other_sources[i]))return false;for(std::size_t i=0;i<expected.registered_efficiency_components.size();++i){const auto& a=actual.registered_efficiency_components[i];const auto& b=expected.registered_efficiency_components[i];if(a.kind!=b.kind||a.cap_bp!=b.cap_bp||!source_equal(a.original_source_ref,b.original_source_ref)||!actor_foundation_rule_equal(a.rule_ref,b.rule_ref))return false;}return true;}
const StudyFact& study(const GrowthState& growth,const StableId& id){const auto* raw=growth.facts.find(id);const auto* fact=raw?std::get_if<StudyFact>(raw):nullptr;if(!fact)fail("StudyLessonOriginalFactRequired",id);return *fact;}
const ConditionReceiptV2& receipt(const AuthoredActorFoundationState& foundation,const StableId& action,Integer ordinal,const StableId& actor){const ConditionReceiptV2* result=nullptr;for(const auto& row:foundation.character_condition_extensions.applied_receipts.rows()){const auto* cause=std::get_if<WorkOrStudyConditionCause>(&row.second.cause);if(!cause||cause->kind!=WorkOrStudyConditionCause::Kind::study_lesson||cause->action_id!=action||cause->ordinal!=ordinal||cause->actor_id!=actor)continue;if(result)fail("DuplicateStudyLessonCondition",action);result=&row.second;}if(!result)fail("StudyLessonConditionRequired",action);return *result;}
class StudySkillInput final {
 CheckpointMemoryCharge copied_fields_;
 const std::optional<StableId> skill_;
 static CheckpointMemoryCharge reserve_fields(const StableId& source){return CheckpointMemoryCharge(0);}
public:
 explicit StudySkillInput(const StableId& source):copied_fields_(reserve_fields(source)),skill_(std::in_place,source){}
 StudySkillInput(const StudySkillInput&)=delete;
 StudySkillInput& operator=(const StudySkillInput&)=delete;
 StudySkillInput(StudySkillInput&&)=delete;
 StudySkillInput& operator=(StudySkillInput&&)=delete;
 const std::optional<StableId>& get()const noexcept{return skill_;}
};
class StudyBeforeHeadInput final {
 CheckpointMemoryCharge copied_fields_;
 const ActorConditionHead head_;
 static CheckpointMemoryCharge reserve_fields(const StudyBeforeHeadBuildView& value){return CheckpointMemoryCharge(0);}
 // The selected optional receives the genuine ref aggregate by ownership transfer.
 static_assert(std::is_nothrow_move_constructible_v<ConditionReceiptRefV2>);
 static_assert(std::is_nothrow_move_constructible_v<std::optional<ConditionReceiptRefV2>>);
public:
 explicit StudyBeforeHeadInput(const StudyBeforeHeadBuildView& value)
  :copied_fields_(reserve_fields(value)),
   head_{StableId{value.actor_id},value.condition_revision,value.initial_condition_seed_ref,
    value.latest_receipt_ref
     ?std::optional<ConditionReceiptRefV2>{ConditionReceiptRefV2{StableId{value.latest_receipt_ref->result_id}}}
     :std::optional<ConditionReceiptRefV2>{}}{}
 StudyBeforeHeadInput(const StudyBeforeHeadInput&)=delete;
 StudyBeforeHeadInput& operator=(const StudyBeforeHeadInput&)=delete;
 StudyBeforeHeadInput(StudyBeforeHeadInput&&)=delete;
 StudyBeforeHeadInput& operator=(StudyBeforeHeadInput&&)=delete;
 const ActorConditionHead& get()const noexcept{return head_;}
};
StudyBeforeHeadBuildView before_head(const AuthoredActorFoundationState& foundation,const ConditionReceiptV2& receipt){const auto* origin=foundation.character_origin_records.origins.find(receipt.actor_id);if(!origin)fail("StudyLessonOriginRequired",receipt.actor_id);StudyBeforeHeadBuildView head{receipt.actor_id,receipt.condition_revision_before,origin->initial_condition_seed_ref,{}};for(const auto& row:foundation.character_condition_extensions.applied_receipts.rows())if(row.second.actor_id==receipt.actor_id&&row.second.condition_revision_after==head.condition_revision){if(head.latest_receipt_ref)fail("StudyLessonConditionRevisionDuplicate",receipt.actor_id);head.latest_receipt_ref=ConditionReceiptRefV2BuildView{std::string_view{row.first}};}if((head.condition_revision==0)!=!head.latest_receipt_ref)fail("StudyLessonConditionPredecessorRequired",receipt.actor_id);return head;}
// 选择课时发生前最新的真实服务边界；缺失、终止或版本不符时拒绝。
const LifeFixedServiceBoundary &
study_lesson_service_boundary(const GrowthState &growth, const StableId &action,
                              const WorldMoment &at) {
  const LifeFixedServiceBoundary *selected = nullptr;
  for (const auto &row : growth.fixed_service_boundaries.rows())
    if (row.second.action_ref == action &&
        take(compare_world_moments(row.second.occurred_at, at)) <= 0) {
      if (!selected || row.second.boundary_ordinal > selected->boundary_ordinal)
        selected = &row.second;
    }
  if (!selected || selected->source_schema != 1 ||
      selected->kind == LifeFixedBoundaryKind::completed ||
      selected->kind == LifeFixedBoundaryKind::stopped)
    fail("StudyLessonActualServiceBoundaryRequired", action);
  return *selected;
}
void live(const PreparedStudyLessonCapability::State& p,const CommonStore& common,const GrowthState& growth,const GrowthFixedStepPermit& gate){if(p.consumed||p.common!=&common||p.growth!=&growth||p.capture!=common.actor_condition_capture_identity()||!identity(p.identity,common.state().identity)||p.result.action_id!=gate.action_id()||p.result.lesson_ordinal!=gate.ordinal()||take(compare_world_moments(p.result.provided_to,gate.at())))fail("StudyLessonCapabilityExpired",gate.action_id());take(common.validate_actor_condition_savepoint_boundary(p.savepoint));}
}
// 从同候选真实课时边界冻结师生能力；来源缺失或已应用时拒绝。
Outcome<PreparedStudyLessonCapability>
prepare_study_lesson_capability(const CharacterConditionSources &sources,
                                const GrowthState &growth,
                                const GrowthFixedStepPermit &gate) {
  return guarded<PreparedStudyLessonCapability>([&] {
    const auto *action = sources.common.action(gate.action_id());
    if (!action)
      fail("StudyLessonActionRequired", gate.action_id());
    take(validate_growth_fixed_step_permit(gate, sources.common, growth,
                                           *action));
    const auto &fact = study(growth, action->id);
    if (fact.lessons.size() + 1 != static_cast<std::size_t>(gate.ordinal()) ||
        take(compare_world_moments(sources.actual_now, gate.at())))
      fail("StudyLessonCapabilityBoundaryMismatch", action->id);
    for (const auto &row : growth.lesson_capabilities.lesson_capability_sources)
      if (row.action_id == action->id && row.lesson_ordinal == gate.ordinal())
        fail("StudyLessonCapabilityAlreadyApplied", action->id);
    if (!sources.common.actor_condition_capture_identity())
      fail("StudyLessonConditionCaptureRequired", action->id);
    auto state = std::make_shared<PreparedStudyLessonCapability::State>();
    state->common = &sources.common;
    state->growth = &growth;
    state->foundation = &sources.state;
    state->capture = sources.common.actor_condition_capture_identity();
    state->savepoint =
        take(sources.common.actor_condition_savepoint_boundary());
    state->identity = sources.common.state().identity;
    const auto *student =
        sources.state.character_condition_extensions.actor_condition_heads.find(
            fact.terms.student_id);
    const auto *teacher =
        sources.state.character_condition_extensions.actor_condition_heads.find(
            fact.terms.teacher_id);
    if (!student || !teacher)
      fail("StudyLessonConditionHeadRequired", action->id);
    state->student = take(read_character_capability_at_head(
        sources, *student,
        {"study", 1, sources.catalog.identity.content_digest},
        StudySkillInput{fact.terms.skill_id}.get(), gate.at()));
    state->teacher = take(read_character_capability_at_head(
        sources, *teacher,
        {"provide_teaching", 1, sources.catalog.identity.content_digest},
        StudySkillInput{fact.terms.skill_id}.get(), gate.at()));
    auto &row = state->result;
    row.action_id = action->id;
    row.lesson_ordinal = gate.ordinal();
    row.work_period_ref = {
        action->id,
        gate.ordinal(),
        {"growth", "growth_fixed_service_boundary",
         study_lesson_service_boundary(growth, action->id, gate.at())
             .boundary_id},
        1};
    row.provided_from = gate.provided_from();
    row.provided_to = gate.at();
    row.student_capability = freeze(state->student);
    row.teacher_capability = freeze(state->teacher);
    row.xp_rule_ref = state->student.rule_ref;
    row.original_lesson_result_ref = {action->id, gate.ordinal()};
    if (fact.terms.innate_source)
      row.innate_source_ref = LifeOwnedFieldRef{
          {"growth", "skill_study", action->id}, "terms.innate_source"};
    PreparedStudyLessonCapability out;
    out.state_ = std::move(state);
    return out;
  });
}
// 核对冻结能力与当前身体后读取原课时经验；能力失效或身体变化时拒绝。
Outcome<Integer> read_prepared_study_lesson_xp(
    const PreparedStudyLessonCapability &prepared, const CommonStore &common,
    const GrowthState &growth, const ActionCoreView &action,
    const GrowthFixedStepPermit &gate) {
  return guarded<Integer>([&] {
    if (!prepared.state_)
      fail("StudyLessonCapabilityRequired", action.id);
    const auto &p = *prepared.state_;
    live(p, common, growth, gate);
    take(validate_growth_fixed_step_permit(gate, common, growth, action));
    const auto &fact = study(growth, action.id);
    for (const auto *value : {&p.student, &p.teacher}) {
      const auto *actor = common.state().actors.find(value->actor_id);
      const auto *head = p.foundation->character_condition_extensions
                             .actor_condition_heads.find(value->actor_id);
      if (!actor || !head ||
          head->condition_revision !=
              value->condition_head.condition_revision ||
          actor->alive != value->body.alive || actor->hp != value->body.hp ||
          actor->fatigue != value->body.fatigue ||
          actor->injury != value->body.injury)
        fail("StudyLessonCapabilityChanged", value->actor_id);
      if (!value->allowed)
        fail("StudyNeedsRecovery", value->actor_id);
    }
    if (p.student.actor_id != fact.terms.student_id ||
        p.teacher.actor_id != fact.terms.teacher_id)
      fail("StudyLessonCapabilityActorsChanged", action.id);
    return Integer{20} * p.student.efficiency_bp / 10000;
  });
}
Status record_study_lesson_capability_in_candidate(CommonStore& common,GrowthState& growth,const AuthoredActorFoundationState& foundation,const GrowthFixedStepPermit& gate,PreparedStudyLessonCapability& prepared){return guarded<Done>([&]{if(!prepared.state_)fail("StudyLessonCapabilityRequired",gate.action_id());auto& p=*prepared.state_;live(p,common,growth,gate);if(p.foundation!=&foundation)fail("StudyLessonFoundationChanged",gate.action_id());const auto& fact=study(growth,gate.action_id());if(fact.lessons.size()+1==static_cast<std::size_t>(gate.ordinal())&&fact.phase=="completed"&&!fact.success){p.consumed=true;return Done{};}if(fact.lessons.size()!=static_cast<std::size_t>(gate.ordinal()))fail("StudyLessonOriginalResultRequired",gate.action_id());const auto& original=fact.lessons.back();if(original.fatigue_before!=p.student.body.fatigue||original.injury!=p.student.body.injury)fail("StudyLessonOriginalInputsMismatch",gate.action_id());Integer xp=20*p.student.efficiency_bp/10000;if(fact.terms.innate_source)xp=take(innate_study_lesson_xp(xp,fact.terms.innate_source->bonus_bp));if(original.xp!=xp)fail("StudyLessonOriginalXPMismatch",gate.action_id());for(const auto* capability:{&p.student,&p.teacher}){const auto& applied=receipt(foundation,gate.action_id(),gate.ordinal(),capability->actor_id);if(applied.condition_revision_before!=capability->condition_head.condition_revision||applied.condition_revision_after!=applied.condition_revision_before+1||take(compare_world_moments(applied.effective_at.exact_moment,gate.at())))fail("StudyLessonConditionMismatch",gate.action_id());if(capability==&p.student)p.result.student_condition_receipt_ref={applied.result_id};else p.result.teacher_condition_receipt_ref=ConditionReceiptRefV2{applied.result_id};}
 for(const auto& row:growth.lesson_capabilities.lesson_capability_sources)if(row.action_id==gate.action_id()&&row.lesson_ordinal==gate.ordinal())fail("DuplicateStudyLessonCapability",gate.action_id());auto* time=growth.lesson_capabilities.study_time_bindings.find(gate.action_id());if(!time||time->terminal_time||time->provided_period_refs.size()+1!=static_cast<std::size_t>(gate.ordinal()))fail("StudyLessonTimeBindingRequired",gate.action_id());
 time->provided_period_refs.push_back({gate.ordinal(),p.result.work_period_ref,p.result.original_lesson_result_ref});
 StudyConditionBoundary condition;condition.action_id=gate.action_id();condition.boundary_kind=StudyConditionBoundary::Kind::lesson_applied;condition.at=gate.at();condition.student_condition_ref=p.result.student_condition_receipt_ref;condition.teacher_condition_ref=*p.result.teacher_condition_receipt_ref;condition.original_boundary_result_ref=p.result.original_lesson_result_ref;for(const auto& previous:growth.lesson_capabilities.study_condition_boundaries)if(previous.action_id==gate.action_id())++condition.boundary_ordinal;growth.lesson_capabilities.study_condition_boundaries.push_back(std::move(condition));
 growth.lesson_capabilities.lesson_capability_sources.push_back(p.result);p.consumed=true;return Done{};});}
Outcome<const LessonCapabilitySource*> read_study_lesson_capability_source(const CharacterConditionSources& sources,const GrowthState& growth,const StableId& action,Integer ordinal){return guarded<const LessonCapabilitySource*>([&]{const LessonCapabilitySource* result=nullptr;for(const auto& row:growth.lesson_capabilities.lesson_capability_sources)if(row.action_id==action&&row.lesson_ordinal==ordinal){if(result)fail("DuplicateStudyLessonCapability",action);result=&row;}if(!result||ordinal<1)fail("StudyLessonCapabilitySourceRequired",action);const auto& row=*result;const auto& fact=study(growth,action);if(ordinal>static_cast<Integer>(fact.lessons.size())||row.work_period_ref.action_id!=action||row.work_period_ref.lesson_ordinal!=ordinal||row.work_period_ref.source_schema!=1||row.work_period_ref.original_fsl_source.domain!="growth"||row.work_period_ref.original_fsl_source.kind!="growth_fixed_service_boundary"||row.original_lesson_result_ref.action_id!=action||row.original_lesson_result_ref.lesson_ordinal!=ordinal||!row.teacher_capability||!row.teacher_condition_receipt_ref||!row.fatigue_effect_use_refs.empty())fail("StudyLessonCapabilityShapeMismatch",action);if(!sources.elapsed||!sources.scale||!sources.catalog.study_capability_rule)fail("StudyLessonOriginalClockRequired",action);const auto* fsl=growth.fixed_service_boundaries.find(row.work_period_ref.original_fsl_source.id);if(!fsl||fsl->action_ref!=action||fsl->source_schema!=1||take(compare_world_moments(fsl->occurred_at,row.provided_to))>0)fail("StudyLessonFSLSourceMismatch",action);
 const auto previous=ordinal==1?WorkUnits{}:take(fixed_duration_work_units(ordinal-1,sources.scale->units_per_half_day));const auto target=take(fixed_duration_work_units(ordinal,sources.scale->units_per_half_day));auto from=take(read_fixed_service_credit_boundary(*sources.elapsed,action,previous,sources.actual_now,*sources.scale));auto to=take(read_fixed_service_credit_boundary(*sources.elapsed,action,target,sources.actual_now,*sources.scale));if(take(compare_world_moments(from,row.provided_from))||take(compare_world_moments(to,row.provided_to)))fail("StudyLessonActualWindowMismatch",action);
 const auto& student_receipt=receipt(sources.state,action,ordinal,fact.terms.student_id);const auto& teacher_receipt=receipt(sources.state,action,ordinal,fact.terms.teacher_id);if(student_receipt.result_id!=row.student_condition_receipt_ref.result_id||teacher_receipt.result_id!=row.teacher_condition_receipt_ref->result_id)fail("StudyLessonConditionSourceMismatch",action);
 const auto student=take(read_character_capability_at_head(sources,StudyBeforeHeadInput{before_head(sources.state,student_receipt)}.get(),row.student_capability.purpose_ref,StudySkillInput{fact.terms.skill_id}.get(),row.provided_to));const auto teacher=take(read_character_capability_at_head(sources,StudyBeforeHeadInput{before_head(sources.state,teacher_receipt)}.get(),row.teacher_capability->purpose_ref,StudySkillInput{fact.terms.skill_id}.get(),row.provided_to));if(!student.allowed||!teacher.allowed||student.actor_id!=fact.terms.student_id||teacher.actor_id!=fact.terms.teacher_id||!frozen_equal(row.student_capability,student)||!frozen_equal(*row.teacher_capability,teacher)||!actor_foundation_rule_equal(row.xp_rule_ref,*sources.catalog.study_capability_rule))fail("StudyLessonFrozenCapabilityMismatch",action);
 if(bool(row.innate_source_ref)!=bool(fact.terms.innate_source)||(row.innate_source_ref&&(row.innate_source_ref->owner_ref.domain!="growth"||row.innate_source_ref->owner_ref.kind!="skill_study"||row.innate_source_ref->owner_ref.id!=action||row.innate_source_ref->field_selector!="terms.innate_source")))fail("StudyLessonInnateSourceMismatch",action);
 const auto& original=fact.lessons[static_cast<std::size_t>(ordinal-1)];Integer xp=20*student.efficiency_bp/10000;if(fact.terms.innate_source)xp=take(innate_study_lesson_xp(xp,fact.terms.innate_source->bonus_bp));if(original.fatigue_before!=student.body.fatigue||original.injury!=student.body.injury||original.xp!=xp||(original.segment&&original.segment->settled_tick!=row.provided_to.half_day_tick))fail("StudyLessonActualResultMismatch",action);return result;
});}
Status validate_study_lesson_capability_sources(const CharacterConditionSources& sources,const GrowthState& growth){return guarded<Done>([&]{for(const auto& row:growth.facts.rows())if(const auto* fact=std::get_if<StudyFact>(&row.second))for(Integer ordinal=1;ordinal<=static_cast<Integer>(fact->lessons.size());++ordinal)take(read_study_lesson_capability_source(sources,growth,row.first,ordinal));for(const auto& row:growth.lesson_capabilities.lesson_capability_sources)take(read_study_lesson_capability_source(sources,growth,row.action_id,row.lesson_ordinal));return Done{};});}
Outcome<GrowthFixedStudyApplication> apply_study_fixed_gate_in_candidate(CommonStore& common,GrowthState& growth,AuthoredActorFoundationState& foundation,const CharacterConditionSources& sources,const GrowthCatalog& catalog,const CommonCatalog& shared,SocialState& social,InteractionState& interaction,const GrowthAccess& access,const ActionTimeRegistration& registration,const GrowthFixedStepPermit& gate){return guarded<GrowthFixedStudyApplication>([&]{if(&sources.common!=&common||&sources.state!=&foundation||sources.growth!=&growth||sources.growth_catalog!=&catalog||sources.shared!=&shared||!sources.elapsed||!sources.scale)fail("StudyCapabilityCandidateSourcesMismatch",gate.action_id());const auto* active=common.action(gate.action_id());if(!active)fail("StudyLessonActionRequired",gate.action_id());const auto action=*active;auto capability=take(prepare_study_lesson_capability(sources,growth,gate));auto condition=take(begin_growth_work_condition(common,foundation,sources.catalog,growth,action,gate));GrowthFixedStudyApplication out;out.result=take(step_growth_study_at_fixed_gate(common,growth,catalog,shared,social,interaction,access,action,gate,capability));out.actual_condition_writes=take(bind_growth_work_condition(common,foundation,sources.catalog,growth,catalog,shared,*sources.elapsed,*sources.scale,sources.catalog.identity,registration,sources.actual_now,condition));take(record_study_lesson_capability_in_candidate(common,growth,foundation,gate,capability));return out;});}

}
