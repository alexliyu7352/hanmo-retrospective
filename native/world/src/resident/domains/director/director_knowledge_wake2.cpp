#include "director_knowledge_wake2.hpp"
namespace hanmo::resident {namespace {
template<class T>Outcome<T>fail(const char*code,const StableId&id={}){return {false,{},common_failure(code,id).error};}
bool same(const SourceRef&a,const SourceRef&b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
}
SourceRef director_known_report_wake_source2(const SocialKnowledgeKey&key){return {"social","knowledge_receipt_v2",social_time_key(key)};}
Outcome<SubjectWake2> read_director_known_report_wake2(const CommonStore&common,const SocialState&social,const ExactResultTimeRead&source,const ExactResultReadStamp&stamp,const SocialKnowledgeKey&key,const WorldMoment&actual){
 auto receipt=read_knowledge_receipt_view_v2(common,social,source,key.receiver,key.report_id,stamp);if(!receipt.ok)return {false,{},receipt.error};if(!receipt.value)return fail<SubjectWake2>("DirectorKnowledgeReceiptRequired",key.report_id);
 auto time=compare_world_moments(receipt.value->learned_at,actual);if(!time.ok)return {false,{},time.error};if(time.value>0)return fail<SubjectWake2>("DirectorFutureKnowledge",key.report_id);
 const auto*report=social.dynamic->reports.find(key.report_id);if(!report||report->id!=key.report_id||report->fact_id!=report->observation.id)return fail<SubjectWake2>("DirectorKnowledgeReportSourceMissing",key.report_id);
 auto original=source.fact_v2_borrowed(report->fact_id,stamp);if(!original.ok)return {false,{},original.error};if(!original.value||original.value->kind!=report->observation.kind||original.value->supply.has_value()!=report->observation.supply.has_value()||original.value->obligation.has_value()!=report->observation.obligation.has_value())return fail<SubjectWake2>("DirectorKnowledgeObservationMismatch",key.report_id);
 if(original.value->supply){const auto&a=*original.value->supply;const auto&b=*report->observation.supply;if(a.source_key!=b.source_key||a.source_fact_id!=b.source_fact_id||a.source_revision!=b.source_revision||a.target_amount!=b.target_amount||a.physical_amount!=b.physical_amount||a.inventory_id!=b.inventory_id||a.item_id!=b.item_id||a.organization_id!=b.organization_id||a.site_id!=b.site_id)return fail<SubjectWake2>("DirectorKnowledgeObservationMismatch",key.report_id);}
 if(original.value->obligation){const auto&a=*original.value->obligation;const auto&b=*report->observation.obligation;if(a.obligation_id!=b.obligation_id||a.source_fact_id!=b.source_fact_id||a.arrear_id!=b.arrear_id||a.principal_id!=b.principal_id||a.beneficiary_id!=b.beneficiary_id||a.due_tick!=b.due_tick||a.due_amount!=b.due_amount||a.paid_amount!=b.paid_amount||a.outstanding_amount!=b.outstanding_amount)return fail<SubjectWake2>("DirectorKnowledgeObservationMismatch",key.report_id);}
 // The original frozen observation alone identifies a disclosed quantity need. No current inventory,
 // unpaid balance, affair revision, or remote state is read to refresh a remembered observation.
 bool demand=false;if(const auto*o=report->observation.supply?&*report->observation.supply:nullptr)demand=o->physical_amount<o->target_amount;
 if(const auto*o=report->observation.obligation?&*report->observation.obligation:nullptr)demand=demand||o->outstanding_amount>0;
 return {true,SubjectWake2{key.receiver,demand?SubjectWakeKind2::known_need:SubjectWakeKind2::known_message,director_known_report_wake_source2(key),2,0},{}};
}
Status validate_director_known_report_wake2(const CommonStore&common,const SocialState&social,const ExactResultTimeRead&source,const ExactResultReadStamp&stamp,const SubjectWake2&wake,const WorldMoment&actual){
 if((wake.kind!=SubjectWakeKind2::known_need&&wake.kind!=SubjectWakeKind2::known_message)||wake.source_schema!=2||wake.source_revision!=0||wake.actual_source.domain!="social"||wake.actual_source.kind!="knowledge_receipt_v2"||!social.dynamic||!social.dynamic->time2)return common_failure("DirectorKnowledgeWakeSourceMismatch",wake.actual_source.id);
 const SocialKnowledgeKey*key=nullptr;for(const auto&row:social.dynamic->time2->source_time_bindings.rows())if(row.first==wake.actual_source.id){key=std::get_if<SocialKnowledgeKey>(&row.second.key);break;}
 if(!key||!same_social_subject(key->receiver,wake.subject))return common_failure("DirectorKnowledgeWakeReceiverMismatch",wake.actual_source.id);
 auto actual_wake=read_director_known_report_wake2(common,social,source,stamp,*key,actual);if(!actual_wake.ok)return {false,{},actual_wake.error};const auto&v=actual_wake.value;
 return v.kind==wake.kind&&v.source_schema==wake.source_schema&&v.source_revision==wake.source_revision&&same(v.actual_source,wake.actual_source)?common_success():common_failure("DirectorKnowledgeWakeSourceMismatch",wake.actual_source.id);
}
}
