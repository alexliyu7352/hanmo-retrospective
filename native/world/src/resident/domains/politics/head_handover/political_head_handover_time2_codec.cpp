// Explicit source/2 only. Legacy /1 field tables and bytes are not widened.
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "political_head_handover_time2.hpp"
#include "resident/domains/organization/organization_lifecycle_codec_detail.hpp"
namespace hanmo::resident {namespace {
using namespace organization_lifecycle_fields;
template<class C>Status receipt(C& c,PoliticalSuccessionPlayerReceipt& r){c.field("actor_id",r.actor_id);c.field("result_id",r.result_id);c.field("request_sequence",r.request_sequence);return c.field("result_revision",r.result_revision);}
template<class C>Status exact(C& c,PoliticalHeadHandoverExactIssue& r){
 c.object("occurred_at",[&](auto& v){return exact_fields::moment(v,r.occurred_at);});
 return c.optional("authority_basis",r.authority_basis,[](auto& v,auto& x){v.object("issued_source",[&](auto& q){return source(q,x.issued_source);});return v.field("source_schema",x.source_schema);});
}
template<class C,class T>Status tail(C& c,T& r){
 c.optional("player_receipt",r.player_receipt,[](auto& v,auto& x){return receipt(v,x);});
 if(!c.reading()&&!r.exact_issue)return c.reject("HeadHandoverTime2PayloadRequired");
 if(c.reading())r.exact_issue.emplace();return c.object("exact_issue",[&](auto& v){return exact(v,*r.exact_issue);});
}
template<class C>Status offer(C& c,PoliticalHeadHandoverOffer& r){
 c.field("id",r.id);c.field("policy_id",r.policy_id);c.field("organization_id",r.organization_id);c.field("office_id",r.office_id);
 c.field("issuer_actor_id",r.issuer_actor_id);c.field("candidate_actor_id",r.candidate_actor_id);c.field("grant_id",r.grant_id);
 c.field("office_epoch",r.office_epoch);c.field("issued_tick",r.issued_tick);c.field("response_until",r.response_until);return tail(c,r);
}
template<class C>Status answer(C& c,PoliticalHeadHandoverAnswer& r){
 c.field("id",r.id);c.field("offer_fact_id",r.offer_fact_id);c.field("actor_id",r.actor_id);c.field("office_change_fact_id",r.office_change_fact_id);
 c.field("tick",r.tick);c.field("expected_office_epoch",r.expected_office_epoch);c.field("installed_office_epoch",r.installed_office_epoch);c.field("accepted",r.accepted);return tail(c,r);
}
template<class C>Status withdrawal(C& c,PoliticalHeadHandoverWithdrawal& r){
 c.field("id",r.id);c.field("offer_fact_id",r.offer_fact_id);c.field("actor_id",r.actor_id);c.field("grant_id",r.grant_id);
 c.field("office_epoch",r.office_epoch);c.field("tick",r.tick);return tail(c,r);
}
template<class C>Status fields(C& c,PoliticalHeadHandoverState& s){
 if(!c.reading()&&s.version!=2)return c.reject("HeadHandoverTime2ProfileRequired");
 Integer version=2;c.field("version",version);if(!c.status().ok)return c.status();if(version!=2)return c.reject("HeadHandoverTime2ProfileRequired");
 if(c.reading())s.version=version;
 table(c,"offers",s.offers,[](auto& v,auto& x){return offer(v,x);});
 table(c,"answers",s.answers,[](auto& v,auto& x){return answer(v,x);});
 table(c,"withdrawals",s.withdrawals,[](auto& v,auto& x){return withdrawal(v,x);});
 if(s.offers.size()>4096||s.answers.size()>4096||s.withdrawals.size()>4096)return c.reject("PoliticalHeadHandoverHistoryCapacity");
 return c.status();
}
}
Status visit_political_head_handover2_checkpoint(CodecVisitor& v,PoliticalHeadHandoverState& s){return fields(v,s);}
Status emit_political_head_handover2_projection(ProjectionWriter& sink,std::string_view name,const PoliticalHeadHandoverState& s){return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,s,[](auto& v,auto& x){return fields(v,x);});}
Outcome<BoundaryValue> encode_political_head_handover2_state(const PoliticalHeadHandoverState& s){BoundaryValue out;BoundaryProjectionWriter sink(out);auto valid=emit_political_head_handover2_projection(sink,"",s);if(valid.ok)valid=sink.finish();return {valid.ok,std::move(out),valid.error};}
Outcome<PoliticalHeadHandoverState> decode_political_head_handover2_state(const BoundaryValue& value){PoliticalHeadHandoverState out;ActorFoundationJsonVisitor reader(value);auto valid=fields(reader,out);if(valid.ok)valid=reader.finish();return {valid.ok,std::move(out),valid.error};}
}
