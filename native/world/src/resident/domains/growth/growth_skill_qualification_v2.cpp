#include "growth_skill_qualification_v2.hpp"
#include "study_permanent_contribution_read.hpp"
#include "growth_internal.hpp"
#include "growth_permanent_contributions.hpp"
#include "growth_trade_grant_read.hpp"
#include "growth_trade_grant_inputs.hpp"
#include "resident/domains/personal/personal_model.hpp"
#include "resident/domains/actor_foundation/authored_origin.hpp"
#include "resident/domains/actor_foundation/created_origin.hpp"
#include <algorithm>
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/kernel/command_types.hpp"
#include <type_traits>
namespace hanmo::resident {
namespace {
CheckpointMemoryCharge qualification_copy_memory(const GrowthSkillQualificationV2& value){

 // CheckpointVector storage and exact limbs retain their actual allocators.
 return CheckpointMemoryCharge(0);
}
}
GrowthSkillQualificationV2::GrowthSkillQualificationV2(const GrowthSkillQualificationV2& other)
 :text_memory(qualification_copy_memory(other)),source_identity(other.source_identity),actor_id(other.actor_id),skill_id(other.skill_id),source_kind(other.source_kind),origin_ref(other.origin_ref),actual_boundary_ref(other.actual_boundary_ref),observed_at(other.observed_at),minimum_xp(other.minimum_xp),minimum_level(other.minimum_level),coverage_kind(other.coverage_kind),contribution_refs(other.contribution_refs){}
// Form a valid empty value before touching the source. This construction may
// throw; all following exchanges use the original actual no-allocation swaps.
GrowthSkillQualificationV2::GrowthSkillQualificationV2(GrowthSkillQualificationV2&& other):GrowthSkillQualificationV2(){swap(other);}
GrowthSkillQualificationV2& GrowthSkillQualificationV2::operator=(const GrowthSkillQualificationV2& other){if(this!=&other){GrowthSkillQualificationV2 copy(other);swap(copy);}return *this;}
GrowthSkillQualificationV2& GrowthSkillQualificationV2::operator=(GrowthSkillQualificationV2&& other)noexcept{swap(other);return *this;}
void GrowthSkillQualificationV2::swap(GrowthSkillQualificationV2& other)noexcept{
 if(this==&other)return;
 static_assert(std::is_nothrow_swappable_v<WorldIdentity>);
 static_assert(noexcept(observed_at.sub_half_day.swap(other.observed_at.sub_half_day)));
 static_assert(noexcept(contribution_refs.swap(other.contribution_refs)));
 using std::swap;
 text_memory.swap(other.text_memory);swap(source_identity,other.source_identity);
 actor_id.swap(other.actor_id);skill_id.swap(other.skill_id);source_kind.swap(other.source_kind);origin_ref.swap(other.origin_ref);
 auto& left=actual_boundary_ref.original_source_ref;auto& right=other.actual_boundary_ref.original_source_ref;
 left.domain.swap(right.domain);left.kind.swap(right.kind);left.id.swap(right.id);
 swap(actual_boundary_ref.source_schema,other.actual_boundary_ref.source_schema);
 swap(observed_at.half_day_tick,other.observed_at.half_day_tick);
 // Deliberately not std::swap(WorldMoment/CanonicalFraction): their rvalue
 // operations preserve the canonical denominator by a possibly throwing copy.
 observed_at.sub_half_day.swap(other.observed_at.sub_half_day);
 swap(minimum_xp,other.minimum_xp);swap(minimum_level,other.minimum_level);swap(coverage_kind,other.coverage_kind);
 contribution_refs.swap(other.contribution_refs);
}
}
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
template<class F>Outcome<GrowthSkillQualificationV2> guard(F f){try{return {true,f(),{}};}catch(Failure& e){return {false,{},std::move(e.error)};}}
// 原源借用枚举；不复制全表、不构建临时索引，正净增最多原XP上限1200条。
struct Piece {const StableId* action=nullptr;Integer ordinal=0,before=0,net=0,after=0;WorldMoment at;const ConditionReceiptV2* condition=nullptr;bool adjustment=false;const PracticeAwardReceipt* practice=nullptr;const PracticeSkillReceipt* practice_skill=nullptr;const GrowthTradeGrant* trade=nullptr;};
template<class F>void pieces(const CharacterConditionSources& body,const GrowthState& growth,const StableId& actor,const StableId& skill,F visit){
 for(const auto& row:growth.facts.rows()){const auto* study=std::get_if<StudyFact>(&row.second);if(!study||study->terms.student_id!=actor||study->terms.skill_id!=skill)continue;
  if(!study->study_service_binding){if(study->phase=="completed"&&study->success&&growth::study_xp_gained(*study)>0)fail("SkillQualificationLegacyStudyBoundaryReaderRequired",row.first);continue;}
  for(std::size_t n=0;n<study->lessons.size();++n){const auto ordinal=static_cast<Integer>(n+1);auto original=take(read_study_lesson_permanent_contribution(body,growth,{row.first,ordinal}));const auto* condition=body.state.character_condition_extensions.applied_receipts.find(original.original_capability_source->student_condition_receipt_ref.result_id);if(!condition||condition->actor_id!=actor)fail("SkillQualificationStudyConditionRequired",row.first);
   visit(Piece{&row.first,ordinal,original.xp_before,original.xp_gained,original.xp_after,original.effective_at,condition,false});
   if(original.lesson->segment->adjustment){if(!body.growth_catalog)fail("SkillQualificationCatalogRequired");auto a=take(read_study_adjustment_permanent_contribution(body.common,growth,*body.growth_catalog,{row.first,ordinal},body.actual_now));visit(Piece{&row.first,ordinal,a.xp_before,a.xp_gained,a.xp_after,a.source.exact->settled_at,nullptr,true});}
  }
 }
 for(const auto& row:growth.trade_grants.rows())if(row.second.actor_id==actor&&row.second.skill_id==skill){if(!body.growth_catalog||!body.shared)fail("GrowthTradeGrantSourcesRequired",row.first);auto original=take(read_growth_trade_grant_v1(body.common,growth,*body.growth_catalog,*body.shared,{row.first},body.actual_now));const auto& grant=*original.grant;visit(Piece{&row.first,0,grant.xp_before,grant.actual_xp_gain,grant.xp_after,original.terminal->ended_at,nullptr,false,nullptr,nullptr,&grant});}
 if(body.permanent_growth){if(!body.permanent_growth_catalog||!body.work_practice)fail("PracticeSkillSourcesRequired",actor);GrowthPermanentSources sources{body,*body.permanent_growth_catalog,*body.permanent_growth,*body.work_practice};for(const auto& row:body.permanent_growth->practice_awards.rows())if(row.second.actor_id==actor){const auto* receipt=take(read_verified_practice_award(sources,{row.first}));for(const auto& part:receipt->skill_rows)if(part.skill_id==skill)visit(Piece{&row.first,0,part.xp_before,part.actual_xp_gain,part.xp_after,receipt->applied_at.exact_moment,nullptr,false,receipt,&part});}}
}
bool equal_ref(const LifeGrowthContributionRef& ref,const Piece& piece){if(piece.trade){const auto* value=std::get_if<GrowthTradeGrantRef>(&ref);return value&&value->action_id==piece.trade->action_id;}if(piece.practice){const auto* value=std::get_if<PracticeAwardRef>(&ref);return value&&value->award_id==piece.practice->award_id;}if(piece.adjustment){const auto* value=std::get_if<StudyPerformanceAdjustmentRef>(&ref);return value&&value->action_id==*piece.action&&value->lesson_ordinal==piece.ordinal;}const auto* value=std::get_if<StudyLessonResultRef>(&ref);return value&&value->action_id==*piece.action&&value->lesson_ordinal==piece.ordinal;}
const LifeFixedInputChange* input_change(const LifeFixedServiceBoundary& boundary){if(const auto* actual=std::get_if<LifeActualInputChange>(&boundary.cause))return &actual->change;if(const auto* terminal=std::get_if<LifeOriginalTerminalApplication>(&boundary.cause))if(const auto* stop=std::get_if<LifeActualStopTrigger>(&terminal->trigger))return std::get_if<LifeFixedInputChange>(&stop->source);return nullptr;}
// 迟答同M仅接受同原行动实际保存的输入采用因果；缺证据明确拒，绝不凭instance字符串排序。
bool before(const Piece& piece,const CharacterConditionSources& body,const GrowthState& growth,const LifeFixedServiceBoundary& boundary,const ActorConditionHead& head){
 const auto temporal=take(compare_world_moments(piece.at,boundary.occurred_at));if(temporal)return temporal<0;
 const LifeFixedServiceBoundary* actual=nullptr;for(const auto& row:growth.fixed_service_boundaries.rows())if(row.second.action_ref==boundary.action_ref)if(const auto* change=input_change(row.second))if(const auto* changed=std::get_if<LifeGrowthInputChange>(change);changed&&equal_ref(changed->original_result_ref,piece)){if(actual)fail("SkillQualificationDuplicateActualAdoption",row.first);actual=&row.second;}
 if(!actual){
  if(piece.trade){
   const auto* publication=take(read_growth_trade_grant_publication_v1(body,growth,{*piece.action}));
   const auto& order=body.common.state().facts.order();
   const auto applied=std::find(order.begin(),order.end(),publication->publication_id),started=std::find(order.begin(),order.end(),boundary.action_ref);
   if(applied==order.end()||started==order.end())fail("TradeGrantActualSourceCutRequired",*piece.action);
   if(applied<started)return true;
   if(boundary.kind==LifeFixedBoundaryKind::started)return false;
   if(boundary.kind==LifeFixedBoundaryKind::completed||boundary.kind==LifeFixedBoundaryKind::stopped){const GrowthTerminalPublication* ended=nullptr;for(const auto& row:growth.terminal_publications.rows())if(row.second.action_id==boundary.action_ref){if(ended)fail("SkillQualificationTerminalPublicationAmbiguous",boundary.boundary_id);ended=&row.second;}if(ended){const auto end=std::find(order.begin(),order.end(),ended->publication_id);if(end==order.end()||take(compare_world_moments(ended->occurred_at,boundary.occurred_at)))fail("SkillQualificationActualTerminalCutRequired",boundary.boundary_id);if(applied>end)return false;}}
   if(!boundary.previous_boundary_ref)fail("TradeGrantActualInputAdoptionRequired",*piece.action);
   const auto* prior=growth.fixed_service_boundaries.find(*boundary.previous_boundary_ref);
   if(!prior||prior->action_ref!=boundary.action_ref||prior->boundary_ordinal+1!=boundary.boundary_ordinal)fail("SkillQualificationActualPreviousBoundaryRequired",boundary.boundary_id);
   return before(piece,body,growth,*prior,head);
  }
  if(piece.practice){const auto& order=body.common.state().facts.order();const auto award=std::find(order.begin(),order.end(),piece.practice->original_fact_ref.id),started=std::find(order.begin(),order.end(),boundary.action_ref);if(award==order.end()||started==order.end())fail("PracticeSkillActualSourceCutRequired",*piece.action);if(award<started)return true;if(boundary.kind==LifeFixedBoundaryKind::started)return false;
   if(boundary.kind==LifeFixedBoundaryKind::completed||boundary.kind==LifeFixedBoundaryKind::stopped){const GrowthTerminalPublication* terminal=nullptr;for(const auto& row:growth.terminal_publications.rows())if(row.second.action_id==boundary.action_ref&&row.second.terminal_source_ref.id==boundary.boundary_id){if(terminal)fail("SkillQualificationTerminalPublicationAmbiguous",boundary.boundary_id);terminal=&row.second;}if(terminal){const auto end=std::find(order.begin(),order.end(),terminal->publication_id);if(end==order.end())fail("SkillQualificationActualTerminalCutRequired",boundary.boundary_id);if(award>end)return false;}}
   // An earlier same-M input boundary may precede the later actual award.
   // Complete cold closure separately requires every award before terminal to
   // have its unique real adoption; this structural recursion grants no skip.
   if(!boundary.previous_boundary_ref)fail("PracticeSkillActualInputAdoptionRequired",*piece.action);const auto* prior=growth.fixed_service_boundaries.find(*boundary.previous_boundary_ref);if(!prior||prior->action_ref!=boundary.action_ref||prior->boundary_ordinal+1!=boundary.boundary_ordinal)fail("SkillQualificationActualPreviousBoundaryRequired",boundary.boundary_id);return before(piece,body,growth,*prior,head);}
  if(!piece.adjustment){if(!piece.condition)fail("SkillQualificationConditionCutRequired",*piece.action);return piece.condition->condition_revision_after<=head.condition_revision;}
  const LifeActualCommand* command=std::get_if<LifeActualCommand>(&boundary.cause);if(const auto* terminal=std::get_if<LifeOriginalTerminalApplication>(&boundary.cause))if(const auto* stop=std::get_if<LifeActualStopTrigger>(&terminal->trigger))command=std::get_if<LifeActualCommand>(&stop->source);
  if(command){
   if(!body.growth_catalog||!body.growth_catalog->complete_exact_command_cuts)fail("SkillQualificationActualCommitCutRequired",*piece.action);
   auto adjustment=take(read_study_adjustment_permanent_contribution(body.common,growth,*body.growth_catalog,{*piece.action,piece.ordinal},body.actual_now));const auto& bound=*adjustment.source.exact;
   const auto* original=body.common.state().results.find(command->actor_id+":"+std::to_string(command->command_binding.request_seq));if(!original||!*original||!(*original)->ok||(*original)->result_id!=command->command_binding.result_id||(*original)->kind!=command->command_binding.command_kind||(*original)->tick!=boundary.occurred_at.half_day_tick)fail("SkillQualificationActualCommandCutRequired",boundary.boundary_id);
   std::optional<std::size_t> applied,observed;const auto& history=body.common.state().history;for(std::size_t n=0;n<history.size();++n){const auto& row=history[n];if(row.result_id==bound.result_id){if(applied)fail("SkillQualificationDuplicateCommitHistory",bound.result_id);applied=n;}if(row.result_id==command->command_binding.result_id){if(observed||row.actor_id!=command->actor_id||row.kind!=command->command_binding.command_kind)fail("SkillQualificationActualCommandCutRequired",boundary.boundary_id);observed=n;}}
   if(!applied||!observed||*applied==*observed)fail("SkillQualificationDistinctActualCommitRequired",boundary.boundary_id);return *applied<*observed;
  }
  if(boundary.kind==LifeFixedBoundaryKind::completed||boundary.kind==LifeFixedBoundaryKind::stopped){
   const GrowthTerminalPublication* terminal=nullptr;for(const auto& row:growth.terminal_publications.rows())if(row.second.action_id==boundary.action_ref&&row.second.terminal_source_ref.domain=="growth"&&row.second.terminal_source_ref.kind=="growth_fixed_service_boundary"&&row.second.terminal_source_ref.id==boundary.boundary_id){if(terminal)fail("SkillQualificationTerminalPublicationAmbiguous",boundary.boundary_id);terminal=&row.second;}
   if(terminal){if(!body.growth_catalog||!body.growth_catalog->complete_exact_command_cuts)fail("SkillQualificationActualCommitCutRequired",*piece.action);auto original=take(read_study_adjustment_permanent_contribution(body.common,growth,*body.growth_catalog,{*piece.action,piece.ordinal},body.actual_now));if(!original.source.exact->original_fact_prefix)fail("SkillQualificationActualCommitCutRequired",*piece.action);const auto* header=body.common.state().facts.find(terminal->publication_id);if(!header||header->domain!="growth"||header->kind!="growth_result_terminal"||take(compare_world_moments(terminal->occurred_at,boundary.occurred_at)))fail("SkillQualificationActualTerminalCutRequired",boundary.boundary_id);const auto& order=body.common.state().facts.order();const auto at=std::find(order.begin(),order.end(),terminal->publication_id);if(at==order.end())fail("SkillQualificationActualTerminalCutRequired",boundary.boundary_id);return static_cast<std::size_t>(at-order.begin())>=static_cast<std::size_t>(original.source.exact->original_fact_prefix->original_fact_prefix_size);}
  }
  // Non-command transitions cannot silently adopt an XP change. Their previous
  // actual boundary supplies the same cut until the unique Growth InputChange.
  if(!boundary.previous_boundary_ref)fail("SkillQualificationSameMomentCausalityRequired",*piece.action);const auto* prior=growth.fixed_service_boundaries.find(*boundary.previous_boundary_ref);if(!prior||prior->action_ref!=boundary.action_ref||prior->boundary_ordinal+1!=boundary.boundary_ordinal)fail("SkillQualificationActualPreviousBoundaryRequired",boundary.boundary_id);return before(piece,body,growth,*prior,head);
 }if(take(compare_world_moments(actual->occurred_at,piece.at)))fail("SkillQualificationActualAdoptionMomentMismatch",actual->boundary_id);return actual->boundary_ordinal<=boundary.boundary_ordinal;
}
struct ActualSourceCut {GrowthSkillActualBoundary actual;std::size_t source_ordinal=0;};
bool before(const Piece& piece,const CharacterConditionSources& body,const GrowthState& growth,const ActualSourceCut& cut){
 const auto temporal=take(compare_world_moments(piece.at,cut.actual.effective_at));if(temporal)return temporal<0;
 if(piece.trade){const auto* publication=take(read_growth_trade_grant_publication_v1(body,growth,{*piece.action}));const auto& order=body.common.state().facts.order();const auto at=std::find(order.begin(),order.end(),publication->publication_id);if(at==order.end())fail("TradeGrantActualSourceCutRequired",*piece.action);return static_cast<std::size_t>(at-order.begin())<cut.source_ordinal;}
 if(piece.practice){const auto& order=body.common.state().facts.order();const auto at=std::find(order.begin(),order.end(),piece.practice->original_fact_ref.id);if(at==order.end())fail("PracticeSkillActualSourceCutRequired",*piece.action);return static_cast<std::size_t>(at-order.begin())<cut.source_ordinal;}
 if(!piece.adjustment){if(!piece.condition)fail("SkillQualificationConditionCutRequired",*piece.action);return piece.condition->condition_revision_after<=cut.actual.condition_before->condition_revision;}
 if(!body.growth_catalog||!body.growth_catalog->complete_exact_command_cuts)fail("SkillQualificationActualCommitCutRequired",*piece.action);
 auto original=take(read_study_adjustment_permanent_contribution(body.common,growth,*body.growth_catalog,{*piece.action,piece.ordinal},body.actual_now));
 if(!original.source.exact->original_fact_prefix)fail("SkillQualificationActualCommitCutRequired",*piece.action);
 // The zero-time adjustment seals before the next command/domain append. Both
 // positions come from the real retained Common insertion sequence, never IDs.
 return cut.source_ordinal>=static_cast<std::size_t>(original.source.exact->original_fact_prefix->original_fact_prefix_size);
}
GrowthSkillQualificationV2 read(const CharacterConditionSources& body,const GrowthState& growth,const StableId& actor,const StableId& skill,const SkillQualificationBoundaryRefV2& boundary_ref,const LifeFixedServiceBoundary* boundary,const ActualSourceCut* source_cut=nullptr){
 if(body.growth!=&growth||!body.shared||!body.growth_catalog||!growth_skill_defined(skill,body.shared->extended_skills))fail("SkillQualificationSameCandidateRequired",actor);
 CharacterInitialValuesV2 original;
 if(source_cut){
  // The consumer has already proved the actual boundary payload and its M.
  // Reuse Origin's real Common cut as well: a later same-M creation cannot
  // supply an earlier Work/Craft/Repair qualification merely because its
  // initial Condition head has the same numerical Moment.
  auto before_source=take(read_character_initial_values_before_fact_v2(body.common,body.state,body.catalog,body.family_catalog,body.shared,actor,boundary_ref.original_source_ref,source_cut->actual.effective_at,body.actual_now,body.born_sources));
  if(!before_source.present)fail("SkillQualificationActorNotBeforeSource",actor);
  original=before_source.initial;
 }else original=take(read_character_initial_values_v2(body.common,body.state,body.catalog,body.family_catalog,body.shared,actor,body.actual_now,body.born_sources));
 const ActorSkills* initial_skills=original.initial_skills;
 take(validate_complete_character_conditions(body));
 const auto* current=body.common.state().actors.find(actor);if(!initial_skills||!current)fail("SkillQualificationOriginRequired",actor);const auto initial=growth_skill_xp(*initial_skills,skill),today=growth_skill_xp(current->skills,skill);if(initial<0||today<initial||today>1200)fail("SkillQualificationInitialRangeInvalid",actor);
 const ActorConditionHead* cut=nullptr;if(boundary){for(const auto& input:boundary->input_snapshot.actors)if(input.actor_id==actor){if(input.capability_frames.empty())fail("SkillQualificationBodyCutRequired",actor);const auto& h=input.capability_frames.front().condition_head_snapshot;if(cut&&cut->condition_revision!=h.condition_revision)fail("SkillQualificationBodyCutAmbiguous",actor);cut=&h;}if(!cut)fail("SkillQualificationActorBoundaryRequired",actor);take(read_character_condition_at_head(body,*cut,boundary->occurred_at));}
 if(source_cut){if(boundary||!source_cut->actual.condition_before||source_cut->actual.condition_before->actor_id!=actor||take(compare_world_moments(source_cut->actual.effective_at,body.actual_now))>0)fail("SkillQualificationActualSourceCutMismatch",actor);take(read_character_condition_at_head(body,*source_cut->actual.condition_before,source_cut->actual.effective_at));}
 Integer total=0;pieces(body,growth,actor,skill,[&](const Piece& p){if(p.net<0||p.before<initial||p.after>today||p.after-p.before!=p.net||p.net>1200-total)fail("SkillQualificationOriginalNetMismatch",*p.action);total+=p.net;});
 if(initial+total!=today)fail("SkillQualificationUnexplainedPermanentXP",actor);
 GrowthSkillQualificationV2 result;result.text_memory.grow(0);
 result.source_kind="effective_initial_and_verified_permanent_contributions";result.source_identity=body.common.state().identity;result.actor_id=actor;result.skill_id=skill;result.origin_ref=actor;result.actual_boundary_ref=boundary_ref;result.observed_at=boundary?boundary->occurred_at:source_cut?source_cut->actual.effective_at:body.actual_now;result.minimum_xp=initial;
 // 正净贡献的before/after链必须无空洞、无重叠；该链来自实际writer，不能用今日XP补齐。
 Integer cursor=initial;std::optional<WorldMoment> previous;while(cursor<today){bool found=false;Piece next;pieces(body,growth,actor,skill,[&](const Piece& p){if(p.net&&p.before==cursor){if(found)fail("SkillQualificationDuplicateContribution",*p.action);found=true;next=p;}});if(!found)fail("SkillQualificationContributionGap",actor);if(previous&&take(compare_world_moments(*previous,next.at))>0)fail("SkillQualificationContributionTimeOrder",actor);previous=next.at;
  if(source_cut?before(next,body,growth,*source_cut):!boundary||before(next,body,growth,*boundary,*cut)){if(result.minimum_xp!=next.before)fail("SkillQualificationHistoricalContributionGap",actor);result.text_memory.grow(0);if(next.trade)result.contribution_refs.emplace_back(VerifiedLegacyTradeGrantRef{*next.action});else if(next.practice){result.text_memory.grow(0);result.contribution_refs.emplace_back(PracticeAwardSkillContributionRef{*next.action,skill});}else if(next.adjustment)result.contribution_refs.emplace_back(StudyPerformanceAdjustmentRef{*next.action,next.ordinal});else result.contribution_refs.emplace_back(StudyLessonResultRef{*next.action,next.ordinal});result.minimum_xp=next.after;}cursor=next.after;
 }
 result.minimum_level=growth_skill_level(result.minimum_xp);return result;
}
}
Outcome<GrowthSkillQualificationV2> read_growth_skill_qualification_before_source_v2(const CharacterConditionSources& body,const GrowthState& growth,const StableId& actor,const StableId& skill,const SkillQualificationBoundaryRefV2& ref,const GrowthSkillBoundarySourceRead& original){return guard([&]{
 if(!original.bound_to(body.common)||ref.source_schema<1)fail("SkillQualificationOriginalBoundaryReaderRequired",ref.original_source_ref.id);
 const auto* header=body.common.state().facts.find(ref.original_source_ref.id);if(!header||header->id!=ref.original_source_ref.id||header->domain!=ref.original_source_ref.domain||header->kind!=ref.original_source_ref.kind)fail("SkillQualificationOriginalBoundaryHeaderRequired",ref.original_source_ref.id);
 const auto& sequence=body.common.state().facts.order();const auto position=std::find(sequence.begin(),sequence.end(),header->id);if(position==sequence.end())fail("SkillQualificationOriginalAppendCutMissing",header->id);
 ActualSourceCut cut{take(original.read_actual_skill_boundary(actor,ref)),static_cast<std::size_t>(position-sequence.begin())};return read(body,growth,actor,skill,ref,nullptr,&cut);
});}
Outcome<GrowthSkillQualificationV2> read_current_growth_skill_qualification_v2(const CharacterConditionSources& body,const GrowthState& growth,const StableId& actor,const StableId& skill,const SkillQualificationBoundaryRefV2& boundary,const ReadStamp& stamp,const ReadStampAuthority& authority,const DomainReadVersionSource& versions){return guard([&]{
 if(stamp.mode()==ReadMode::committed_history)fail("CurrentSkillQualificationStampRequired",actor);take(authority.validate(stamp,body.common.state().identity,stamp.mode(),body.common,versions));bool actor_version=false,source=false;for(const auto& v:stamp.resources())if(v.resource.kind==ResourceKey::Kind::actor&&v.resource.id==actor)actor_version=true;for(const auto& id:stamp.source_ids())if(id==boundary.original_source_ref.id)source=true;
 const auto* actual=body.common.state().facts.find(boundary.original_source_ref.id);if(!actor_version||!source||boundary.source_schema<1||!actual||actual->domain!=boundary.original_source_ref.domain||actual->kind!=boundary.original_source_ref.kind||take(compare_world_moments(stamp.span().to,body.actual_now)))fail("SkillQualificationActualBoundaryStampRequired",actor);
 return read(body,growth,actor,skill,boundary,nullptr);
});}
Outcome<GrowthSkillQualificationV2> read_growth_skill_qualification_before_fixed_boundary_v2(const CharacterConditionSources& body,const GrowthState& growth,const StableId& actor,const StableId& skill,const StableId& id){return guard([&]{const auto* boundary=growth.fixed_service_boundaries.find(id);if(!boundary||boundary->boundary_id!=id||boundary->source_schema!=1||take(compare_world_moments(boundary->occurred_at,body.actual_now))>0)fail("SkillQualificationActualFixedBoundaryRequired",id);return read(body,growth,actor,skill,{{"growth","growth_fixed_service_boundary",id},1},boundary);});}
}
#include "growth_skill_qualification_codec_fields.hpp"
namespace hanmo::resident {
Status validate_growth_skill_qualification_at_fixed_boundary_v2(const CharacterConditionSources& body,const GrowthState& growth,const StableId& boundary,const GrowthSkillQualificationV2& value){try{
 auto expected=take(read_growth_skill_qualification_before_fixed_boundary_v2(body,growth,value.actor_id,value.skill_id,boundary));const auto& identity=body.common.state().identity;
 if(value.source_identity.world_id!=identity.world_id||value.source_identity.execution_epoch<0||value.source_identity.execution_epoch>identity.execution_epoch||value.source_identity.revision<0||value.source_identity.revision>identity.revision||value.source_identity.tick<0||value.source_identity.tick>value.observed_at.half_day_tick||value.actor_id!=expected.actor_id||value.skill_id!=expected.skill_id||value.origin_ref!=expected.origin_ref||value.source_kind!=expected.source_kind||value.actual_boundary_ref.source_schema!=1||value.actual_boundary_ref.original_source_ref.domain!="growth"||value.actual_boundary_ref.original_source_ref.kind!="growth_fixed_service_boundary"||value.actual_boundary_ref.original_source_ref.id!=boundary||take(compare_world_moments(value.observed_at,expected.observed_at))||value.minimum_xp!=expected.minimum_xp||value.minimum_level!=expected.minimum_level||value.coverage_kind!=expected.coverage_kind||value.contribution_refs.size()!=expected.contribution_refs.size())fail("GrowthFixedSkillSourceMismatch",boundary);
 for(std::size_t n=0;n<expected.contribution_refs.size();++n)if(!growth_skill_qualification_codec::same_contribution(value.contribution_refs[n],expected.contribution_refs[n]))fail("GrowthFixedSkillContributionMismatch",boundary);
 return common_success();}catch(Failure& e){return {false,{},std::move(e.error)};}}

Outcome<const StableId*> read_growth_fixed_required_skill(const CharacterConditionSources& body,const GrowthState& growth,const LifeFixedServiceBoundary& boundary,const LifeFixedActorInputs& input){
 const auto* raw=growth.facts.find(boundary.action_ref);if(const auto* study=raw?std::get_if<StudyFact>(raw):nullptr){if(input.actor_id!=study->terms.student_id&&input.actor_id!=study->terms.teacher_id)return {false,{},common_failure("GrowthFixedStudySkillActorMismatch",boundary.boundary_id).error};return {true,&study->terms.skill_id,{}};}
 if(const auto* training=raw?std::get_if<TrainingFact>(raw):nullptr){if(input.actor_id!=training->training.student_id){if(input.actor_id!=training->training.teacher_id)return {false,{},common_failure("GrowthFixedTrainingActorMismatch",boundary.boundary_id).error};return {true,nullptr,{}};}if(!body.personal_catalog)return {false,{},common_failure("GrowthFixedPersonalSkillCatalogRequired",boundary.boundary_id).error};const auto* definition=body.personal_catalog->skills.find(training->training.skill_id);if(!definition)return {false,{},common_failure("UnknownPersonalSkill",training->training.skill_id).error};return {true,definition->required_skill_level?&definition->required_skill_level->skill_id:nullptr,{}};}
 if((raw&&std::holds_alternative<RestFact>(*raw))||growth.life_activities.find(boundary.action_ref))return {true,nullptr,{}};
 return {false,{},common_failure("GrowthFixedOriginalSkillMaskRequired",boundary.boundary_id).error};
}
Status capture_growth_fixed_skill_inputs_in_candidate(const CharacterConditionSources& body,GrowthState& growth,const StableId& id,const ReadStamp& stamp,const ReadStampAuthority& authority,const DomainReadVersionSource& versions){try{
 if(body.growth!=&growth||stamp.mode()==ReadMode::committed_history)fail("GrowthFixedSkillCandidateRequired",id);take(authority.validate(stamp,body.common.state().identity,stamp.mode(),body.common,versions));
 auto* boundary=growth.fixed_service_boundaries.find(id);if(!boundary||take(compare_world_moments(boundary->occurred_at,body.actual_now))||take(compare_world_moments(stamp.span().to,body.actual_now))||std::find(stamp.source_ids().begin(),stamp.source_ids().end(),id)==stamp.source_ids().end())fail("GrowthFixedSkillActualBoundaryStampRequired",id);
 for(auto& actor:boundary->input_snapshot.actors){if(!actor.skill_qualification_views.empty())fail("GrowthFixedSkillInputsAlreadyCaptured",id);const auto* skill=take(read_growth_fixed_required_skill(body,growth,*boundary,actor));if(!skill)continue;
  bool resource=false;for(const auto& version:stamp.resources())if(version.resource.kind==ResourceKey::Kind::actor&&version.resource.id==actor.actor_id)resource=true;
  const auto* current=body.state.character_condition_extensions.actor_condition_heads.find(actor.actor_id);if(!resource||!current||actor.capability_frames.empty()||current->condition_revision!=actor.capability_frames.front().condition_head_snapshot.condition_revision)fail("GrowthFixedSkillHeadChanged",actor.actor_id);
  actor.skill_qualification_views.push_back(take(read_growth_skill_qualification_before_fixed_boundary_v2(body,growth,actor.actor_id,*skill,id)));
 }
 return common_success();}catch(Failure& e){return {false,{},std::move(e.error)};}}
}

namespace hanmo::resident {
Status validate_growth_skill_qualification_before_source_v2(const CharacterConditionSources& body,const GrowthState& growth,const GrowthSkillQualificationV2& value,const GrowthSkillBoundarySourceRead& original){try{
 auto expected=take(read_growth_skill_qualification_before_source_v2(body,growth,value.actor_id,value.skill_id,value.actual_boundary_ref,original));const auto& identity=body.common.state().identity;
 if(value.source_identity.world_id!=identity.world_id||value.source_identity.execution_epoch<0||value.source_identity.execution_epoch>identity.execution_epoch||value.source_identity.revision<0||value.source_identity.revision>identity.revision||value.source_identity.tick<0||value.source_identity.tick>value.observed_at.half_day_tick||value.origin_ref!=expected.origin_ref||value.source_kind!=expected.source_kind||take(compare_world_moments(value.observed_at,expected.observed_at))||value.minimum_xp!=expected.minimum_xp||value.minimum_level!=expected.minimum_level||value.coverage_kind!=expected.coverage_kind||value.contribution_refs.size()!=expected.contribution_refs.size())fail("GrowthHistoricalSkillQualificationMismatch",value.actual_boundary_ref.original_source_ref.id);
 for(std::size_t n=0;n<expected.contribution_refs.size();++n)if(!growth_skill_qualification_codec::same_contribution(value.contribution_refs[n],expected.contribution_refs[n]))fail("GrowthHistoricalSkillContributionMismatch",value.actual_boundary_ref.original_source_ref.id);return common_success();
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
}

namespace hanmo::resident {
Outcome<GrowthVerifiedSkillNetV2> read_verified_growth_skill_net_v2(const CharacterConditionSources& body,const GrowthState& growth,const StableId& actor,const StableId& skill){try{
 if(body.growth!=&growth||!body.shared||!growth_skill_defined(skill,body.shared->extended_skills))fail("SkillQualificationSameCandidateRequired",actor);auto origin=take(read_character_initial_values_v2(body.common,body.state,body.catalog,body.family_catalog,body.shared,actor,body.actual_now,body.born_sources));const auto* current=body.common.state().actors.find(actor);if(!current||!origin.initial_skills)fail("SkillQualificationOriginRequired",actor);GrowthVerifiedSkillNetV2 value{growth_skill_xp(*origin.initial_skills,skill),0,growth_skill_xp(current->skills,skill)};if(value.initial_xp<0||value.current_xp<value.initial_xp||value.current_xp>1200)fail("SkillQualificationInitialRangeInvalid",actor);
 pieces(body,growth,actor,skill,[&](const Piece& p){if(p.net<0||p.before<value.initial_xp||p.after>value.current_xp||p.after-p.before!=p.net||p.net>1200-value.known_net)fail("SkillQualificationOriginalNetMismatch",*p.action);value.known_net+=p.net;});if(value.initial_xp+value.known_net!=value.current_xp)fail("SkillQualificationUnexplainedPermanentXP",actor);
 Integer cursor=value.initial_xp;std::optional<WorldMoment> previous;while(cursor<value.current_xp){bool found=false;Piece next;pieces(body,growth,actor,skill,[&](const Piece& p){if(p.net&&p.before==cursor){if(found)fail("SkillQualificationDuplicateContribution",*p.action);found=true;next=p;}});if(!found)fail("SkillQualificationContributionGap",actor);if(previous&&take(compare_world_moments(*previous,next.at))>0)fail("SkillQualificationContributionTimeOrder",actor);previous=next.at;cursor=next.after;}return {true,value,{}};
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
}
