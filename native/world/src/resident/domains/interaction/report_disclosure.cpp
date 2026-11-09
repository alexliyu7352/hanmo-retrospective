#include "report_disclosure.hpp"
#include "resident/storage/common_store.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void reject(const char*c,const StableId&id={},ErrorKind kind=ErrorKind::rule){throw Failure{common_failure(c,id,kind).error};}
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
void take(Status v){if(!v.ok)throw Failure{std::move(v.error)};}
template<class T,class F>Outcome<T> guard(F f){try{return {true,f(),{}};}catch(Failure&e){return {false,{},std::move(e.error)};}}
bool same(const SourceRef&a,const SourceRef&b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
bool participant(const Report&r,const SubjectRef&s){for(const auto&p:r.observation.participants)if(same_social_subject(p.subject,s))return true;return false;}
const Report& source(const ReportDisclosureReadContext&x,const StableId&id,const SubjectRef&sender,const WorldMoment&at){
 if(!x.social.dynamic||!x.social.dynamic->time2)reject("ReportDisclosureKnowledgeCapabilityRequired",id,ErrorKind::unsupported_capability);
 if(sender.kind!=SubjectKind::actor||sender.id.empty())reject("ReportDisclosureKnowledgeCapabilityRequired",id);
 const auto*known=find_social_knowledge(*x.social.dynamic,sender,id);if(!known)reject("ReportDisclosureNotKnown",id);
 const auto*r=x.social.dynamic->reports.find(id);if(!r)reject("ReportDisclosureSourceMissing",id);
 auto receipt=take(read_knowledge_receipt_view_v2(x.common,x.social,x.results,sender,id,x.result_stamp));if(!receipt||take(compare_world_moments(receipt->learned_at,at))>0)reject("ReportDisclosureNotYetKnown",id);
 take(validate_exact_report_source(x.common,*r,x.results,x.result_stamp));
 const auto*actual=take(x.results.fact_v2_borrowed(r->fact_id,x.result_stamp));if(!actual)reject("ReportDisclosureProjectionMissing",id);
 if(take(compare_world_moments(actual->occurred_at,at))>0)reject("ReportDisclosureBeforeObservation",id);
 return *r;
}
MomentOwnerSource original(const ReportDisclosureReadContext&x,const Report&r,CheckpointMemoryCharge* memory=nullptr){const auto*h=x.common.state().facts.find(r.fact_id);if(!h)reject("ReportDisclosureHeaderMissing",r.fact_id);const auto*f=take(x.results.fact_v2_borrowed(r.fact_id,x.result_stamp));if(!f)reject("ReportDisclosureProjectionMissing",r.fact_id);if(const auto*t=std::get_if<OwnerFactMoment>(&f->time_basis)){if(memory){(*memory).grow(0);}return {t->source,t->source_schema};}const auto*t=std::get_if<LegacyFactBoundary>(&f->time_basis);if(!t||t->original_source_schema<1)reject("ReportDisclosureOriginalSchemaMissing",r.fact_id);if(memory){(*memory).grow(0);}return {{h->domain,h->kind,h->id},t->original_source_schema};}
void receiver_shape(const SubjectRef&sender,const SubjectRef&receiver){if(receiver.kind!=SubjectKind::actor||receiver.id.empty()||same_social_subject(sender,receiver))reject("ReportDisclosureNamedRecipientRequired",receiver.id);}
}
static Outcome<ReportDisclosurePermission> prepare_permission_owned(const ReportDisclosureReadContext&x,const StableId&id,const SubjectRef&sender,const SubjectRef&receiver,const ReadStamp&stamp,CheckpointMemoryCharge* memory){return guard<ReportDisclosurePermission>([&]{
 if(stamp.mode()!=ReadMode::phase_current&&stamp.mode()!=ReadMode::committed_current)reject("CurrentReportDisclosureStampRequired",id);take(x.authority.validate(stamp,x.common.state().identity,stamp.mode(),x.common,x.versions));receiver_shape(sender,receiver);auto at=take(x.clock.read_current_moment());const auto&r=source(x,id,sender,at);
 if(r.visibility_scope==ReportVisibility::local_public&&r.observation.publicly_visible)return ReportDisclosurePermission{ReportDisclosureBasisKind::public_report,original(x,r,memory)};
 if(participant(r,receiver))return ReportDisclosurePermission{ReportDisclosureBasisKind::original_participant,original(x,r,memory)};
 if(!x.originals)reject("ReportDisclosureOriginalPermissionRequired",id,ErrorKind::unsupported_capability);auto permission=take(x.originals->read_report_disclosure_permission(r,sender,receiver,at,stamp));if(!permission)reject("ReportDisclosureNotPermitted",id);
 if(permission->source_schema<1||permission->source_ref.domain.empty()||permission->source_ref.kind.empty()||permission->source_ref.id.empty())reject("ReportDisclosurePermissionSourceMissing",id);
 // 此原provider的拥有返回值直接移动，不能在复制完成后补计为分配前限制。
 return ReportDisclosurePermission{ReportDisclosureBasisKind::original_business_permission,std::move(*permission)};
});}
// 两入口只共用原权限检查；显式owner不改变可披露的业务集合。
Outcome<ReportDisclosurePermission> prepare_report_disclosure_permission(const ReportDisclosureReadContext&x,const StableId&id,const SubjectRef&s,const SubjectRef&r,const ReadStamp&stamp){return prepare_permission_owned(x,id,s,r,stamp,nullptr);}
Outcome<ReportDisclosurePermission> prepare_report_disclosure_permission(const ReportDisclosureReadContext&x,const StableId&id,const SubjectRef&s,const SubjectRef&r,const ReadStamp&stamp,CheckpointMemoryCharge& memory){return prepare_permission_owned(x,id,s,r,stamp,&memory);}
Status validate_report_disclosure_permission_history(const ReportDisclosureReadContext&x,const StableId&id,const SubjectRef&sender,const SubjectRef&receiver,const ReportDisclosurePermission&permission,const WorldMoment&at,const SourceRef&disclosure){return guard<Done>([&]{
 receiver_shape(sender,receiver);const auto&r=source(x,id,sender,at);CheckpointMemoryCharge original_memory;const auto original_source=original(x,r,&original_memory);
 if(permission.kind==ReportDisclosureBasisKind::public_report){if(r.visibility_scope!=ReportVisibility::local_public||!r.observation.publicly_visible||!same(permission.original_source.source_ref,original_source.source_ref)||permission.original_source.source_schema!=original_source.source_schema)reject("ReportPublicDisclosureSourceChanged",id);}
 else if(permission.kind==ReportDisclosureBasisKind::original_participant){if(!participant(r,receiver)||!same(permission.original_source.source_ref,original_source.source_ref)||permission.original_source.source_schema!=original_source.source_schema)reject("ReportParticipantDisclosureSourceChanged",id);}
 else if(permission.kind==ReportDisclosureBasisKind::original_business_permission){if(!x.originals)reject("ReportDisclosureOriginalPermissionRequired",id,ErrorKind::unsupported_capability);take(x.originals->validate_report_disclosure_permission(r,sender,receiver,permission.original_source,at,disclosure));}
 else reject("UnknownReportDisclosureBasis",id);return Done{};
});}
}
