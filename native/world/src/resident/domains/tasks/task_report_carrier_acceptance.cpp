#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "task_report_carrier_acceptance.hpp"
#include "task_dynamic_projection.hpp"
namespace hanmo::resident {namespace {
template<class A>Status source(A&a,SourceRef&s){a.field("domain",s.domain);a.field("kind",s.kind);return a.field("id",s.id);}
template<class A>Status fields(A&a,AcceptKnownReportCarrier&q){
 a.object("authored_offer",[&](auto&c){return source(c,q.authored_offer);});a.field("recipient_actor_id",q.recipient_actor_id);
 a.object("actual_carrier_disclosure",[&](auto&c){c.object("source_ref",[&](auto&n){return source(n,q.actual_carrier_disclosure.source_ref);});return c.field("source_schema",q.actual_carrier_disclosure.source_schema);});
 a.field("funding_wallet_id",q.funding_wallet_id);a.field("funding_gross_cap",q.funding_gross_cap);return a.object("funding_until",[&](auto&c){return dynamic_codec::exact_moment(c,q.funding_until);});
}
Status shape(const AcceptKnownReportCarrier&q){if(q.authored_offer.domain!="tasks_catalog"||q.authored_offer.kind!="report_carrier_offer"||q.authored_offer.id.empty()||q.recipient_actor_id.empty()||q.actual_carrier_disclosure.source_ref.domain!="interaction"||q.actual_carrier_disclosure.source_ref.kind!=face_to_face_report_source_kind||q.actual_carrier_disclosure.source_ref.id.empty()||q.actual_carrier_disclosure.source_schema!=1||q.funding_wallet_id.empty()||q.funding_gross_cap<0)return common_failure("InvalidKnownReportCarrierAcceptance");auto moment=compare_world_moments(q.funding_until,q.funding_until);return moment.ok?common_success():Status{false,{},moment.error};}
}
Outcome<AcceptKnownReportCarrier> decode_accept_known_report_carrier(const BoundaryValue&value){AcceptKnownReportCarrier q;dynamic_codec::JsonArchive a(value);auto valid=fields(a,q);if(valid.ok)valid=a.finish();if(valid.ok)valid=shape(q);return valid.ok?Outcome<AcceptKnownReportCarrier>{true,std::move(q),{}}:Outcome<AcceptKnownReportCarrier>{false,{},valid.error};}
Status emit_accept_known_report_carrier_fields(ProjectionWriter&w,const AcceptKnownReportCarrier&q){DynamicProjectionArchive a(w);return fields(a,const_cast<AcceptKnownReportCarrier&>(q));}
Outcome<TaskReportCarrierAcceptanceQuote> preview_known_report_carrier_acceptance(const TaskReportCarrierAcceptanceContext&x,const StableId&sender,const AcceptKnownReportCarrier&q){
 auto valid=shape(q);if(!valid.ok)return {false,{},valid.error};const auto&face=x.scope.face;
 if(sender.empty()||&face.common!=&x.funding.common||&x.scope.tasks!=&x.funding.tasks)return {false,{},common_failure("CarrierAcceptanceSameCandidateRequired").error};
 TaskReportCarrierAcceptanceQuote out;out.selection={q.authored_offer,sender,q.recipient_actor_id,q.actual_carrier_disclosure};
 auto scope=read_current_task_report_carrier_scope(x.scope,out.selection);if(!scope.ok)return {false,{},scope.error};
 SelfTransportFundingProposal proposal{sender,q.funding_wallet_id,{SubjectKind::actor,scope.value.offer->carrier_actor_id},scope.value.offer->destination_site_id,q.funding_gross_cap,q.funding_until};
 auto funding=preview_self_transport_funding(x.funding,proposal);if(!funding.ok)return {false,{},funding.error};out.funding=std::move(funding.value);
 NativeTaskReportCarrierTermsRead original(x.scope,x.funding,out.selection,out.funding);
 auto acceptance=preview_task_report_carrier_agreement(face.common,face.interaction,face.catalog,face.access,original,sender,{q.authored_offer});if(!acceptance.ok)return {false,{},acceptance.error};out.acceptance=std::move(acceptance.value);
 return {true,std::move(out),{}};
}
Outcome<SourceRef> accept_known_report_carrier_in_candidate(CommonStore&common,TaskState&tasks,PrivateTransportFundingState&funding,const TaskReportCarrierAcceptanceContext&x,const StableId&sender,const TaskReportCarrierAcceptanceQuote&q){
 const auto&face=x.scope.face;if(&common!=&face.common||&tasks!=&x.scope.tasks||&common!=&x.funding.common||&tasks!=&x.funding.tasks||&funding!=&x.funding.state||sender!=q.selection.sender_actor_id||sender!=q.funding.proposal.principal_actor_id||!q.acceptance.funding_finalization_required)return {false,{},common_failure("CarrierAcceptanceSameCandidateRequired").error};
 NativeTaskReportCarrierTermsRead original(x.scope,x.funding,q.selection,q.funding);NativeTaskReportCarrierFundingWrite actual(common,tasks,funding,x.funding,q.funding,original);
 auto now=face.disclosure.clock.read_current_moment();if(!now.ok)return {false,{},now.error};
 return accept_task_carrier_agreement_in_candidate(common,tasks,face.interaction,face.catalog,face.access,original,q.acceptance,now.value,face.disclosure.clock,&actual);
}
}
