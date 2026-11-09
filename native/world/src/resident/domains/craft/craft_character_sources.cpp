#include "craft_character_sources.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include <limits>
#include <new>
#include <type_traits>
#include "resident/domains/growth/growth_skill_api.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};[[noreturn]]void fail(const char*c,const StableId&id={},ErrorKind kind=ErrorKind::internal){auto error=common_failure(c,id).error;error.kind=kind;throw Failure{std::move(error)};}
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
bool same_head(const ActorConditionHead&a,const ActorConditionHead&b){return a.actor_id==b.actor_id&&a.condition_revision==b.condition_revision&&a.initial_condition_seed_ref.seed_key==b.initial_condition_seed_ref.seed_key&&a.latest_receipt_ref.has_value()==b.latest_receipt_ref.has_value()&&(!a.latest_receipt_ref||a.latest_receipt_ref->result_id==b.latest_receipt_ref->result_id);}
void context(const CraftCharacterSourceContext&x){if(x.body.growth!=&x.growth||take(compare_world_moments(x.body.actual_now,take(x.clock.read_current_moment())))||x.jobs.schema_version!=1)fail("CraftCharacterSourceContextMismatch");}
ActorFoundationRuleRef purpose(const CraftCharacterSourceContext&x){return {"craft_job_worker",1,x.body.catalog.identity.content_digest};}
void capable(const CraftCharacterSourceContext&x,const ActorConditionHead&head,const WorldMoment&at,ErrorKind denial=ErrorKind::rule){auto body=take(read_character_capability_at_head(x.body,head,purpose(x),std::nullopt,at));if(!body.allowed)fail("CraftParticipantCannotWork",head.actor_id,denial);}
void actual_job(const CraftCharacterSourceContext&x,const CraftJob&j){
 context(x);const auto*h=x.body.common.state().facts.find(j.id);
 if(x.jobs.jobs.find(j.id)!=&j||j.terms.job_id!=j.id||j.terms.maker.kind!=SubjectKind::actor||j.terms.maker.id.empty()||!same_source_ref(j.terms.acceptance_ref,craft_job_source(j.id))||!h||h->id!=j.id||h->domain!="crafting"||h->kind!="craft_job")fail("CraftBodyActualJobRequired",j.id);
}
bool participant(const CraftJob&j,const StableId&actor){return actor==j.terms.maker.id||std::find(j.terms.accepted_helpers.begin(),j.terms.accepted_helpers.end(),actor)!=j.terms.accepted_helpers.end();}
void proof_scope(const CraftCharacterSourceContext&x,const CraftJob&j,const WorkServiceIntervalProof&p){
 actual_job(x,j);if(!same_work_binding(j.execution_binding,p.execution_binding)||take(compare_world_moments(p.actual_span.from,p.actual_span.to))>=0||take(compare_world_moments(p.actual_span.from,j.terms.accepted_at))<0||p.actual_actor_roles.size()!=j.terms.accepted_helpers.size()+1)fail("CraftBodyActualServiceScopeMismatch",j.id);
 for(std::size_t i=0;i<p.actual_actor_roles.size();++i){const auto&r=p.actual_actor_roles[i];if(r.role.empty()||!participant(j,r.actor_id))fail("CraftBodyActualParticipantMismatch",j.id);for(std::size_t k=0;k<i;++k)if(r.actor_id==p.actual_actor_roles[k].actor_id)fail("DuplicateCraftBodyParticipant",r.actor_id);}
}
}
Outcome<CraftParticipantQualification> read_current_craft_participant_qualification(const CraftCharacterSourceContext&x,const StableId&maker,const std::vector<StableId>&helpers,const CraftRecipeRef&ref){try{
 context(x);const auto*recipe=take(read_craft_recipe(x.recipes,ref));if(maker.empty())fail("CraftMakerRequired",{},ErrorKind::rule);CraftParticipantQualification out;
 auto add=[&](const StableId&actor){if(actor.empty())fail("CraftParticipantRequired",{},ErrorKind::rule);for(const auto&head:out.actual_condition_heads)if(head.actor_id==actor)fail("DuplicateCraftParticipant",actor,ErrorKind::rule);auto current=take(read_current_character_condition(x.body,actor));capable(x,current.head,x.body.actual_now);auto source=take(read_character_condition_head_source(x.body,current.head,x.body.actual_now));out.actual_body_sources.push_back({source.source,source.source_schema});out.actual_condition_heads.push_back(std::move(current.head));};
 add(maker);for(const auto&helper:helpers)add(helper);const auto net=take(read_verified_growth_skill_net_v2(x.body,x.growth,maker,recipe->required_skill_id));out.maker_skill_level=growth_skill_level(net.current_xp);if(out.maker_skill_level<recipe->minimum_skill_level)fail("CraftMinimumSkillRequired",maker,ErrorKind::rule);return {true,std::move(out),{}};
}catch(Failure&e){return {false,{},std::move(e.error)};}}
bool NativeCraftSkillBoundaryRead::bound_to(const CommonStore&common)const noexcept{return &common==&common_;}
Outcome<GrowthSkillActualBoundary> NativeCraftSkillBoundaryRead::read_actual_skill_boundary(const StableId&actor,const SkillQualificationBoundaryRefV2&ref)const{try{
 if(ref.source_schema!=1||ref.original_source_ref.domain!="crafting"||ref.original_source_ref.kind!="craft_job")fail("UnsupportedCraftSkillBoundary",ref.original_source_ref.id,ErrorKind::unsupported_capability);
 const auto*job=jobs_.jobs.find(ref.original_source_ref.id);const auto*header=common_.state().facts.find(ref.original_source_ref.id);if(jobs_.schema_version!=1||!job||job->id!=ref.original_source_ref.id||job->terms.job_id!=job->id||!same_source_ref(job->terms.acceptance_ref,ref.original_source_ref)||!header||header->id!=job->id||header->domain!="crafting"||header->kind!="craft_job"||job->terms.maker.kind!=SubjectKind::actor||job->terms.participant_condition_heads.size()!=job->terms.accepted_helpers.size()+1)fail("CraftSkillAcceptanceSourceRequired",ref.original_source_ref.id);
 const ActorConditionHead*found=nullptr;for(std::size_t i=0;i<job->terms.participant_condition_heads.size();++i){const auto&head=job->terms.participant_condition_heads[i];const auto&expected=i?job->terms.accepted_helpers[i-1]:job->terms.maker.id;if(head.actor_id!=expected)fail("CraftSkillParticipantHeadMismatch",job->id);if(head.actor_id==actor){if(found)fail("DuplicateCraftSkillParticipant",actor);found=&head;}}
 if(!found)fail("CraftSkillBoundaryActorMismatch",actor);return {true,{job->terms.accepted_at,found},{}};
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Status validate_craft_participant_qualification(const CraftCharacterSourceContext&x,const CraftTerms&terms,const SourceRef*actual){try{
 context(x);const auto*recipe=take(read_craft_recipe(x.recipes,terms.recipe_ref));if(terms.maker.kind!=SubjectKind::actor||terms.participant_condition_heads.size()!=terms.accepted_helpers.size()+1||take(compare_world_moments(terms.accepted_at,x.body.actual_now))>0)fail("CraftParticipantAdoptionMismatch",terms.job_id);
 NativeCraftSkillBoundaryRead boundary(x.body.common,x.jobs);
 if(actual){const auto*row=x.jobs.jobs.find(actual->id);if(!row||&row->terms!=&terms||!same_source_ref(*actual,craft_job_source(terms.job_id)))fail("CraftQualificationActualTermsRequired",terms.job_id);}
 else if(!terms.job_id.empty()||!terms.acceptance_ref.id.empty()||take(compare_world_moments(terms.accepted_at,x.body.actual_now)))fail("CraftQualificationProspectiveTermsRequired",terms.job_id);
 for(std::size_t i=0;i<terms.participant_condition_heads.size();++i){const auto&head=terms.participant_condition_heads[i];const auto&actor=i?terms.accepted_helpers[i-1]:terms.maker.id;if(head.actor_id!=actor)fail("CraftParticipantAdoptedHeadMismatch",terms.job_id);for(std::size_t k=0;k<i;++k)if(head.actor_id==terms.participant_condition_heads[k].actor_id)fail("DuplicateCraftParticipant",actor);capable(x,head,terms.accepted_at,actual?ErrorKind::internal:ErrorKind::rule);auto source=take(read_character_condition_head_source(x.body,head,terms.accepted_at));std::size_t count=0;for(const auto&frozen:terms.qualification_sources)if(frozen.source_schema==source.source_schema&&same_source_ref(frozen.source_ref,source.source))++count;if(count!=1)fail("CraftQualificationBodySourceCoverageMismatch",actor);
  if(actual)take(boundary.read_actual_skill_boundary(actor,{*actual,1}));else if(!same_head(head,take(read_current_character_condition(x.body,actor)).head))fail("StaleCraftConditionQualification",actor,ErrorKind::rule);
 }
 Integer level=0;if(actual)level=take(read_growth_skill_qualification_before_source_v2(x.body,x.growth,terms.maker.id,recipe->required_skill_id,{*actual,1},boundary)).minimum_level;else level=growth_skill_level(take(read_verified_growth_skill_net_v2(x.body,x.growth,terms.maker.id,recipe->required_skill_id)).current_xp);
 if(level!=terms.quality_inputs.skill_level)fail("CraftFrozenSkillQualificationMismatch",terms.job_id);if(level<recipe->minimum_skill_level)fail("CraftMinimumSkillRequired",terms.maker.id,actual?ErrorKind::internal:ErrorKind::rule);return common_success();
}catch(Failure&e){return {false,{},std::move(e.error)};}}
bool is_craft_participant_body_source(const MomentOwnerSource&s)noexcept{return s.source_ref.domain=="actor_foundation"&&(s.source_ref.kind=="initial_condition_seed"||s.source_ref.kind=="condition_receipt");}
CheckpointMemoryCharge CapturedCraftParticipantBodyInputs::storage_memory(std::size_t count){
 return CheckpointMemoryCharge(0);
}
CapturedCraftParticipantBodyInputs::CapturedCraftParticipantBodyInputs(std::size_t count)
 :copied_fields_(storage_memory(count)),capacity_(count){sources_.reserve(count);}
void CapturedCraftParticipantBodyInputs::append(const SourceRef& source,Integer schema){
 static_assert(std::is_nothrow_move_constructible_v<MomentOwnerSource>,"Craft body input requires nonthrowing payload transfer");
 if(sources_.size()>=capacity_)throw std::bad_alloc();
 (copied_fields_).grow(0);
 sources_.push_back(MomentOwnerSource{source,schema});
}
Outcome<CapturedCraftParticipantBodyInputs> capture_craft_participant_body_inputs(const CraftCharacterSourceContext&x,const CraftJob&j,const MomentSpan&span){try{
 actual_job(x,j);if(j.phase!=CraftJobPhase::working||take(compare_world_moments(span.from,x.body.actual_now))||take(compare_world_moments(span.from,span.to))>=0||take(compare_world_moments(j.terms.accepted_at,span.from))>0)fail("CraftBodyActualFromRequired",j.id);
 if(j.terms.accepted_helpers.size()==std::numeric_limits<std::size_t>::max())throw std::bad_alloc();
 CapturedCraftParticipantBodyInputs out(j.terms.accepted_helpers.size()+1);auto add=[&](const StableId&actor){const auto current=take(read_current_character_condition(x.body,actor));capable(x,current.head,span.from,ErrorKind::internal);const auto source=take(read_character_condition_head_source(x.body,current.head,span.from));for(const auto&old:out.sources())if(same_source_ref(old.source_ref,source.source))fail("DuplicateCraftBodyInput",actor);out.append(source.source,source.source_schema);};add(j.terms.maker.id);for(const auto&helper:j.terms.accepted_helpers)add(helper);return {true,std::move(out),{}};
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Status validate_prepared_craft_participant_body_source(const CraftCharacterSourceContext&x,const CraftJob&j,const WorkServiceIntervalProof&p,const MomentOwnerSource&s){try{
 proof_scope(x,j,p);if(!is_craft_participant_body_source(s)||take(compare_world_moments(p.actual_span.from,x.body.actual_now)))fail("CraftBodyPreparedBoundaryMismatch",j.id);
 std::size_t adopted=0;for(const auto&ref:p.participation_sources)if(ref.source_schema==s.source_schema&&same_source_ref(ref.source_ref,s.source_ref))++adopted;if(adopted!=1)fail("CraftBodyPreparedSourceNotAdopted",s.source_ref.id);
 const auto actual=take(read_character_condition_source(x.body,s.source_ref,s.source_schema));if(!participant(j,actual.actor_id))fail("CraftBodyPreparedActorMismatch",s.source_ref.id);const auto current=take(read_current_character_condition(x.body,actual.actor_id));const auto source=take(read_character_condition_head_source(x.body,current.head,p.actual_span.from));if(source.source_schema!=s.source_schema||!same_source_ref(source.source,s.source_ref))fail("CraftBodyPreparedHeadChanged",actual.actor_id);capable(x,current.head,p.actual_span.from,ErrorKind::internal);return common_success();
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Status validate_craft_participant_body_span(const CraftCharacterSourceContext&x,const CraftJob&j,const WorkServiceIntervalProof&p){try{
 proof_scope(x,j,p);if(take(compare_world_moments(p.actual_span.to,x.body.actual_now))>0)fail("CraftBodyActualToRequired",j.id);
 std::size_t body_count=0;
 for(const auto&s:p.participation_sources)if(is_craft_participant_body_source(s)){
  const auto actual=take(read_character_condition_source(x.body,s.source_ref,s.source_schema));if(!participant(j,actual.actor_id))fail("CraftBodySpanUnexpectedActor",s.source_ref.id);++body_count;
  const auto adopted=take(read_character_condition_at_source(x.body,s.source_ref,s.source_schema,p.actual_span.from));const auto value=take(read_character_capability_for_span(x.body,adopted.head,purpose(x),std::nullopt,p.actual_span));if(!value.allowed)fail("CraftBodySpanIneligible",actual.actor_id);
 }
 if(body_count!=p.actual_actor_roles.size())fail("CraftBodySpanSourceCoverageMismatch",j.id);
 for(const auto&role:p.actual_actor_roles){std::size_t count=0;for(const auto&s:p.participation_sources)if(is_craft_participant_body_source(s)&&take(read_character_condition_source(x.body,s.source_ref,s.source_schema)).actor_id==role.actor_id)++count;if(count!=1)fail("CraftBodySpanParticipantSourceMismatch",role.actor_id);}
 return common_success();
}catch(Failure&e){return {false,{},std::move(e.error)};}}
}
