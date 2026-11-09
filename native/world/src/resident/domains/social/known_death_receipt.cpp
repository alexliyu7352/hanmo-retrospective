#include "known_death_receipt.hpp"
namespace hanmo::resident {namespace {
template<class T>Outcome<T>fail(const char*code,const StableId&id={}){return {false,{},common_failure(code,id,ErrorKind::internal).error};}
}
Outcome<std::optional<ActorKnownDeathReceiptView>> read_actor_known_death_receipt(const ActorReportKnowledgeContext&x,const FamilyDeathPolicy&policy,const FamilyDeathState&life,const FamilyLifeCauseSources4&causes,const StableId&observer,const ActorKnownDeathReceiptRef&ref){
 using Result=std::optional<ActorKnownDeathReceiptView>;
 if(!policy.enabled||policy.selected_version!=4)return {false,{},common_failure("KnownDeathLife4SelectionRequired",{},ErrorKind::unsupported_capability).error};
 if(!x.catalog)return {false,{},common_failure("KnownDeathCatalogIdentityRequired",{},ErrorKind::unsupported_capability).error};auto selected=validate_family_death_policy_selection(policy,*x.catalog);if(!selected.ok)return {false,{},selected.error};
 if(ref.report_id.empty()||ref.death.domain!="family_life"||ref.death.kind!="family_death"||ref.death.id.empty())return fail<Result>("InvalidKnownDeathReceiptRef",ref.report_id);
 auto acquired=read_actor_report_knowledge_receipt(x,observer,ref.report_id);if(!acquired.ok)return {false,{},acquired.error};if(!acquired.value)return {true,std::nullopt,{}};
 const auto*header=x.common.state().facts.find(ref.death.id);const auto*death=life.deaths.find(ref.death.id);const auto*report=acquired.value->report;
 if(!header||header->domain!=ref.death.domain||header->kind!=ref.death.kind||!death||!death->precise||death->precise->death_id!=ref.death.id||report->fact_id!=ref.death.id||report->payload_kind!=ref.death.kind)return fail<Result>("KnownDeathOriginalSourceMismatch",ref.death.id);
 auto valid=validate_family_death4(x.common,*death,causes,x.results.source_moment());if(!valid.ok)return {false,{},valid.error};
 for(const auto&row:life.deaths.rows())if(row.first!=ref.death.id&&row.second.actor_id==death->actor_id)return fail<Result>("ConflictingDeathSource",death->actor_id);
 auto fact=x.results.fact_v2_borrowed(ref.death.id,x.stamp);if(!fact.ok)return {false,{},fact.error};const auto*basis=fact.value?std::get_if<OwnerFactMoment>(&fact.value->time_basis):nullptr;
 if(!basis||basis->source.domain!=header->domain||basis->source.kind!=header->kind||basis->source.id!=header->id||basis->source_schema!=policy.selected_version||fact.value->subject_id!=death->actor_id)return fail<Result>("KnownDeathExactOriginalProjectionRequired",ref.death.id);
 // A late-recorded actual death cannot have been learned from this Report
 // before its original owner produced the record, even if the cause is older.
 auto recorded=compare_world_moments(death->precise->recorded_at,acquired.value->knowledge.learned_at);if(!recorded.ok)return {false,{},recorded.error};if(recorded.value>0)return fail<Result>("DeathKnowledgeBeforeOriginalRecord",ref.death.id);
 return {true,ActorKnownDeathReceiptView{header,death,std::move(*acquired.value)},{}};
}
Outcome<ActorKnownDeathReceiptView> read_actor_known_death_receipt_before_source(const ActorReportKnowledgeContext&x,const FamilyDeathPolicy&policy,const FamilyDeathState&life,const FamilyLifeCauseSources4&causes,const StableId&observer,const ActorKnownDeathReceiptRef&ref,const SourceRef&consumer,const WorldMoment&at){
 auto value=read_actor_known_death_receipt(x,policy,life,causes,observer,ref);if(!value.ok)return {false,{},value.error};if(!value.value)return {false,{},common_failure("ActorDeathKnowledgeRequired",ref.report_id,ErrorKind::rule).error};
 auto valid=validate_actor_report_knowledge_before_source(x,value.value->acquired,consumer,at);if(!valid.ok)return {false,{},valid.error};return {true,std::move(*value.value),{}};
}
}
