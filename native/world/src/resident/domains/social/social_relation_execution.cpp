#include "social_relation_execution.hpp"
#include "resident/kernel/native_catalog_decode.hpp"
#include "resident/checkpoint/checkpoint_catalog_rows.hpp"
#include <limits>
#include <charconv>
#include <string_view>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char*code,const StableId&id={},ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(code,id,kind).error};}
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
template<class T,class F>Outcome<T> guard(F fn){try{return {true,fn(),{}};}catch(Failure&e){return {false,{},std::move(e.error)};}}
Integer result_serial(const StableId&id){constexpr std::string_view prefix="social_relation_result:";if(id.size()<=prefix.size()||id.compare(0,prefix.size(),prefix)||id[prefix.size()]=='0')fail("InvalidOriginalRelationResultId",id);Integer n=0;auto parsed=std::from_chars(id.data()+prefix.size(),id.data()+id.size(),n);if(parsed.ec!=std::errc{}||parsed.ptr!=id.data()+id.size()||n<1)fail("InvalidOriginalRelationResultId",id);return n;}
bool same(const SourceRef&a,const FactRecord&b){return a.id==b.id&&a.domain==b.domain&&a.kind==b.kind;}
// This is the old sole clamp implementation, moved here without a second write
// leg. Both APIs below call it; selected callers cannot enter the legacy API.
RelationRecord* change_base(CommonWrite&common,SocialState&s,const StableId&from,const StableId&to,Integer trust,Integer affinity){
 RelationRecord*found=nullptr;for(auto&row:s.relations.rows())if(row.second.from_actor==from&&row.second.to_actor==to){found=&row.second;break;}
 if(!found){auto id=take(common.next_id("relation"));s.relations.insert(id,{id,from,to,0,0});found=s.relations.find(id);}
 found->trust=clamp_original_social_relation(found->trust,trust);found->affinity=clamp_original_social_relation(found->affinity,affinity);return found;
}
void selected(const OriginalSocialRelationContext&c){
 if(c.social.original_relation_version!=2||!c.sources.bound_to(c.common,c.social))fail("OriginalSocialRelationSourcesRequired",{},ErrorKind::unsupported_capability);
 if(c.catalog.identity.content_digest.empty()||c.catalog.identity.content_digest!=c.common.state().metadata.content_digest||c.catalog.identity.content_id!=c.common.state().metadata.content_id||c.catalog.identity.content_version!=c.common.state().metadata.content_version||c.catalog.identity.ruleset_id!=c.common.state().metadata.ruleset_id)fail("OriginalSocialRelationCatalogMismatch");
 take(compare_world_moments(c.actual_now,c.actual_now));
}
const std::pair<StableId,RelationRecord>* current(const SocialState&s,const StableId&from,const StableId&to){
 const std::pair<StableId,RelationRecord>*found=nullptr;for(const auto&row:s.relations.rows())if(row.second.from_actor==from&&row.second.to_actor==to){if(found)fail("DuplicateOriginalRelationDirection",row.first);found=&row;}return found;
}
OriginalSocialRelationView initial(const OriginalSocialRelationContext&c,const StableId&from,const StableId&to){
 OriginalSocialRelationView out;for(const auto&row:c.catalog.relations.rows())if(row.second.from_actor==from&&row.second.to_actor==to){if(out.relation_record_key)fail("DuplicateOriginalAuthoredRelationDirection",row.first);out.relation_record_key=&row.first;out.trust=row.second.trust;out.affinity=row.second.affinity;}return out;
}
void application(const OriginalSocialRelationContext&c,const SocialRelationResult&r,const FactRecord&h){
 take(validate_social_relation_result_shape(r));if(h.id!=r.result_id||h.domain!="social"||h.kind!="relation_applied")fail("OriginalRelationHeaderMismatch",r.result_id);
 auto source=take(c.sources.read_original_application(r.original_cause,c.actual_now));
 if(!source.from_actor||!source.to_actor||!source.occurred_at||*source.from_actor!=r.from_actor||*source.to_actor!=r.to_actor||take(compare_world_moments(*source.occurred_at,r.applied_at))||take(compare_world_moments(r.applied_at,c.actual_now))>0)fail("OriginalRelationCauseMismatch",r.result_id);
 const auto delta=take(original_social_relation_delta(source.rule));if(delta.trust!=r.trust_delta||delta.affinity!=r.affinity_delta)fail("OriginalRelationDeltaMismatch",r.result_id);
 take(c.sources.validate_original_application_before_result(r.original_cause,h,c.actual_now));
}
OriginalSocialRelationView fold(const OriginalSocialRelationContext&c,const StableId&from,const StableId&to,std::size_t limit){
 auto value=initial(c,from,to);std::size_t ordinal=0;
 for(const auto&head:c.common.state().facts.rows()){
  if(ordinal++==limit)break;if(head.second.domain!="social"||head.second.kind!="relation_applied")continue;
  const auto*r=c.social.relation_results.find(head.first);if(!r)fail("OriginalRelationPayloadRequired",head.first);
  if(r->from_actor!=from||r->to_actor!=to)continue;
  if(value.trust!=r->trust_before||value.affinity!=r->affinity_before||(value.relation_record_key&&*value.relation_record_key!=r->relation_record_key))fail("OriginalRelationPredecessorMismatch",head.first);
  if(!value.relation_record_key)value.relation_record_key=&r->relation_record_key;
  value.trust=r->trust_after;value.affinity=r->affinity_after;value.last_application=r;
 }
 return value;
}
Status validate_impl(const OriginalSocialRelationContext&c,bool complete_coverage){return guard<Done>([&]{
 selected(c);const auto total=c.common.state().facts.size();std::size_t heads=0;Integer previous_serial=0;const WorldMoment*previous_at=nullptr;
 for(const auto&seed:c.catalog.relations.rows()){
  if(seed.first.empty()||seed.second.from_actor.empty()||seed.second.to_actor.empty()||seed.second.trust<0||seed.second.trust>100||seed.second.affinity<0||seed.second.affinity>100)fail("InvalidOriginalAuthoredRelation",seed.first);
  if(!current(c.social,seed.second.from_actor,seed.second.to_actor))fail("OriginalAuthoredRelationMissing",seed.first);
 }
 for(const auto&head:c.common.state().facts.rows())if(head.second.domain=="social"&&head.second.kind=="relation_applied"){
  ++heads;const auto*r=c.social.relation_results.find(head.first);if(!r||r->result_id!=head.first)fail("OriginalRelationPayloadRequired",head.first);application(c,*r,head.second);auto serial=result_serial(r->result_id);if(serial<=previous_serial||serial>=c.common.state().next_instance_id||(previous_at&&take(compare_world_moments(*previous_at,r->applied_at))>0))fail("OriginalRelationApplicationOrderMismatch",r->result_id);previous_serial=serial;previous_at=&r->applied_at;
 }
 if(heads!=c.social.relation_results.size())fail("OriginalRelationHeaderCoverageMismatch");
 for(const auto&row:c.social.relation_results.rows()){
  if(row.first!=row.second.result_id)fail("OriginalRelationResultKeyMismatch",row.first);
  for(const auto&prior:c.social.relation_results.rows()){if(prior.first==row.first)break;if(same_original_social_relation_cause(prior.second.original_cause,row.second.original_cause))fail("DuplicateOriginalRelationApplication",row.first);}
  if(!current(c.social,row.second.from_actor,row.second.to_actor))fail("OriginalRelationCurrentMissing",row.first);
 }
 for(const auto&row:c.social.relations.rows()){
  const auto&r=row.second;if(!r.id||*r.id!=row.first||!c.common.actor(r.from_actor)||!c.common.actor(r.to_actor))fail("OriginalRelationCurrentIdentityMismatch",row.first);if(current(c.social,r.from_actor,r.to_actor)!=&row)fail("DuplicateOriginalRelationDirection",row.first);
  const auto expected=fold(c,r.from_actor,r.to_actor,total);
  if(!expected.relation_record_key||*expected.relation_record_key!=row.first||expected.trust!=r.trust||expected.affinity!=r.affinity)fail("OriginalRelationCurrentCoverageMismatch",row.first);
 }
 if(complete_coverage)take(c.sources.validate_original_application_coverage(c.actual_now));return Done{};
 });}
}
Outcome<OriginalSocialRelationDelta> original_social_relation_delta(OriginalSocialRelationRule rule){
 switch(rule){case OriginalSocialRelationRule::theft_victim:return {true,{-30,-10},{}};case OriginalSocialRelationRule::theft_actor:return {true,{-5,-10},{}};case OriginalSocialRelationRule::refusal:return {true,{-5,-2},{}};case OriginalSocialRelationRule::aid_receiver:return {true,{10,5},{}};case OriginalSocialRelationRule::aid_helper:return {true,{2,2},{}};case OriginalSocialRelationRule::heard_theft:return {true,{-10,-3},{}};case OriginalSocialRelationRule::heard_aid:return {true,{3,1},{}};case OriginalSocialRelationRule::successful_task_report:return {true,{4,1},{}};case OriginalSocialRelationRule::completed_work_service:return {true,{1,1},{}};}
 return {false,{},common_failure("UnknownOriginalRelationRule",{},ErrorKind::internal).error};
}
Status social_change_relation(CommonWrite&common,SocialState&s,const StableId&from,const StableId&to,Integer trust,Integer affinity){return guard<Done>([&]{if(s.original_relation_version!=1||!s.relation_results.empty())fail("OriginalRelationTypedSourceRequired",{},ErrorKind::unsupported_capability);change_base(common,s,from,to,trust,affinity);return Done{};});}
Status change_original_social_relation_legacy(CommonWrite&common,SocialState&s,const StableId&from,const StableId&to,OriginalSocialRelationRule rule){auto delta=original_social_relation_delta(rule);if(!delta.ok)return {false,{},delta.error};return social_change_relation(common,s,from,to,delta.value.trust,delta.value.affinity);}
Status validate_original_social_relations(const OriginalSocialRelationContext&c){return validate_impl(c,true);}
Outcome<const SocialRelationResult*> read_original_social_relation_result(const OriginalSocialRelationContext&c,const SourceRef&ref,Integer schema){return guard<const SocialRelationResult*>([&]{
 if(schema!=1||ref.domain!="social"||ref.kind!="relation_applied")fail("OriginalRelationSourceSchemaMismatch",ref.id);take(validate_impl(c,true));const auto*r=c.social.relation_results.find(ref.id);const auto*h=c.common.state().facts.find(ref.id);if(!r||!h||!same(ref,*h))fail("OriginalRelationSourceRequired",ref.id);return r;
 });}
Outcome<OriginalSocialRelationView> read_original_social_relation_at_cut(const OriginalSocialRelationContext&c,const StableId&from,const StableId&to,const OriginalSocialRelationCut&cut){return guard<OriginalSocialRelationView>([&]{
 take(validate_impl(c,true));if(from.empty()||to.empty()||from==to||take(compare_world_moments(cut.adopted_at,c.actual_now))>0)fail("OriginalRelationAdoptionCutInvalid");
 const auto limit=take(read_actual_common_fact_prefix(c.common,cut.original_prefix));
 std::size_t ordinal=0;for(const auto&head:c.common.state().facts.rows()){if(head.second.domain=="social"&&head.second.kind=="relation_applied"){
  const auto&r=*c.social.relation_results.find(head.first);const auto order=take(compare_world_moments(r.applied_at,cut.adopted_at));if((ordinal<limit&&order>0)||(ordinal>=limit&&order<0))fail("OriginalRelationAdoptionTimeMismatch",head.first);
 }++ordinal;}
 return fold(c,from,to,limit);
 });}
Outcome<const SocialRelationResult*> apply_original_social_relation_in_candidate(CommonStore&common,SocialState&s,const OriginalSocialRelationContext&c,const OriginalSocialRelationCause&cause,const WorldMoment&at){return guard<const SocialRelationResult*>([&]{
 if(&c.common!=&common||&c.social!=&s||take(compare_world_moments(c.actual_now,at)))fail("OriginalRelationSameCandidateRequired");
 // The new business cause exists already, so its reverse coverage is completed
 // by this call. Existing immutable results/current base are still fully folded.
 take(validate_impl(c,false));const auto source=take(c.sources.read_original_application(cause,c.actual_now));
 if(!source.from_actor||!source.to_actor||!source.occurred_at||*source.from_actor==*source.to_actor||!common.actor(*source.from_actor)||!common.actor(*source.to_actor)||take(compare_world_moments(*source.occurred_at,at)))fail("OriginalRelationActualApplicationRequired");
 for(const auto&row:s.relation_results.rows())if(same_original_social_relation_cause(row.second.original_cause,cause)){const auto*h=common.state().facts.find(row.first);if(!h)fail("OriginalRelationHeaderRequired",row.first);application(c,row.second,*h);return &row.second;}
 const auto delta=take(original_social_relation_delta(source.rule));const auto*before=current(s,*source.from_actor,*source.to_actor);
 SocialRelationResult result;result.from_actor=*source.from_actor;result.to_actor=*source.to_actor;result.original_cause=cause;result.trust_before=before?before->second.trust:0;result.affinity_before=before?before->second.affinity:0;result.trust_delta=delta.trust;result.affinity_delta=delta.affinity;result.applied_at=at;
 auto*after=change_base(common,s,result.from_actor,result.to_actor,delta.trust,delta.affinity);if(!before&&source.rule==OriginalSocialRelationRule::completed_work_service)after->field_order={"id","from_actor","to_actor","affinity","trust"};const auto*actual=current(s,result.from_actor,result.to_actor);if(!actual||&actual->second!=after)fail("OriginalRelationActualRowRequired");result.relation_record_key=actual->first;result.trust_after=after->trust;result.affinity_after=after->affinity;result.result_id=take(common.next_id("social_relation_result"));const auto id=result.result_id;
 take(validate_social_relation_result_shape(result));if(!s.relation_results.insert(id,std::move(result)))fail("DuplicateOriginalRelationResult",id);take(common.append_fact({id,"relation_applied","social"}));const auto*stored=s.relation_results.find(id);application(c,*stored,*common.state().facts.find(id));return stored;
 });}
Outcome<OriginalSocialRelationCatalog> decode_original_social_relation_catalog(const BoundaryValue&data,const CatalogIdentity&identity){try{using namespace boundary;
 if(identity.content_digest.empty())boundary::fail("OriginalRelationLockedCatalogRequired",{});OriginalSocialRelationCatalog out;out.identity=copy_native_catalog_identity_for_decode(identity);
 const auto& source_rows=object(field(data,"relations","data"),"data/relations");CheckpointCatalogRows<OriginalSocialRelationSeed> rows(source_rows.size(),"data/relations");
 for(const auto&row:source_rows){
  const auto&v=row.second;OriginalSocialRelationSeed seed{get_string(v,"from_actor",row.first),get_string(v,"to_actor",row.first),get_int(v,"trust",row.first),get_int(v,"affinity",row.first)};
  if(row.first.empty()||seed.from_actor.empty()||seed.to_actor.empty()||seed.trust<0||seed.trust>100||seed.affinity<0||seed.affinity>100)boundary::fail("InvalidOriginalAuthoredRelation",row.first);
  // Every preceding source row has passed this same decoder already. Borrow
  // those fields for the original direction check, before the original ID check.
  for(const auto&prior:source_rows){if(&prior==&row)break;if(get_string_ref(prior.second,"from_actor",prior.first)==seed.from_actor&&get_string_ref(prior.second,"to_actor",prior.first)==seed.to_actor)boundary::fail("DuplicateOriginalAuthoredRelationDirection",row.first);}
  if(!rows.insert(row.first,std::move(seed)))boundary::fail("DuplicateOriginalAuthoredRelation",row.first);
 }rows.publish(out.relations);return {true,std::move(out),{}};
 }catch(boundary::DecodeFailure&e){return {false,{},std::move(e.error)};}}
}
