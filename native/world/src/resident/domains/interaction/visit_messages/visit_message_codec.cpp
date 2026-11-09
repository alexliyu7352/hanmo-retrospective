#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "../report_disclosure_codec_fields.hpp"
#include "../s3_leaf_life_codec.hpp"
// 新段严格访问，不把可重建索引或临时世界epoch保存为跨档许可。
#include "visit_message_state.hpp"
#include "visit_report_custody.hpp"
#include "resident/domains/tasks/task_dynamic_projection.hpp"
namespace hanmo::resident {namespace {
template<class A>Status contact(A& a,VisitMessageContact& v){a.field("actor_id",v.actor_id);a.field("custodian_actor_id",v.custodian_actor_id);a.field("place_id",v.place_id);a.field("role_id",v.role_id);a.field("session_id",v.session_id);a.field("dialogue_id",v.dialogue_id);a.field("node_id",v.node_id);return a.field("tick",v.tick);}
template<class A>Status posted(A& a,VisitMessagePosted& v){a.field("fact_id",v.fact_id);a.field("policy_id",v.policy_id);a.field("sender_actor_id",v.sender_actor_id);a.field("recipient_actor_id",v.recipient_actor_id);return a.object("contact",[&](auto& child){return contact(child,v.contact);});}
template<class A>Status terminal(A& a,VisitMessageTerminal& v){a.field("fact_id",v.fact_id);a.field("message_fact_id",v.message_fact_id);return a.object("contact",[&](auto& child){return contact(child,v.contact);});}
template<class A>Status source(A&a,SourceRef&v){a.field("domain",v.domain);a.field("kind",v.kind);return a.field("id",v.id);}
template<class A>Status timed_source(A&a,MomentOwnerSource&v){a.object("source_ref",[&](auto&c){return source(c,v.source_ref);});return a.field("source_schema",v.source_schema);}
template<class A>Status custody_deposit(A&a,VisitReportCustodyDeposit&v){a.field("fact_id",v.fact_id);a.field("policy_id",v.policy_id);a.field("report_id",v.report_id);a.object("contact",[&](auto&c){return contact(c,v.contact);});a.object("contact_source",[&](auto&c){return timed_source(c,v.contact_source);});a.object("sender_knowledge_source",[&](auto&c){return source(c,v.sender_knowledge_source);});a.object("permission",[&](auto&c){return report_disclosure_codec::permission(c,v.permission);});return a.object("deposited_at",[&](auto&c){return dynamic_codec::exact_moment(c,v.deposited_at);});}
template<class A>Status custody_terminal(A&a,VisitReportCustodyTerminal&v){a.field("fact_id",v.fact_id);a.field("deposit_ref",v.deposit_ref);a.object("contact",[&](auto&c){return contact(c,v.contact);});a.object("contact_source",[&](auto&c){return timed_source(c,v.contact_source);});return a.object("occurred_at",[&](auto&c){return dynamic_codec::exact_moment(c,v.occurred_at);});}
template<class A>Status life_source(A&a,VisitMessageLifeSource&v){a.object("death_ref",[&](auto&c){return s3_leaf_life_codec::death(c,v.death_ref);});a.field("message_fact_id",v.message_fact_id);dynamic_codec::enumeration(a,"role",v.role,2);return a.object("effect",[&](auto&c){return s3_leaf_life_codec::fields(c,v.effect,3);});}
template<class A>Status state(A& a,VisitMessageState& v){
 a.field("version",v.version);if(v.version<1||v.version>3)return a.reject("UnsupportedVisitMessageVersion");
 auto r=dynamic_codec::table(a,"posted",v.posted,[](auto& child,auto& row){return posted(child,row);});if(!r.ok)return r;
 r=dynamic_codec::table(a,"received",v.received,[](auto& child,auto& row){return terminal(child,row);});if(!r.ok)return r;
 r=dynamic_codec::table(a,"withdrawn",v.withdrawn,[](auto& child,auto& row){return terminal(child,row);});if(!r.ok)return r;
 if(v.version>=2){r=dynamic_codec::table(a,"life_sources",v.life_sources,[](auto&c,auto&x){return life_source(c,x);});if(!r.ok)return r;}
 else if(!v.life_sources.empty())return a.reject("UnselectedVisitMessageLifeSources");
 if(v.version<3)return v.report_deposits.empty()&&v.report_pickups.empty()&&v.report_withdrawals.empty()?a.status():a.reject("UnselectedVisitReportCustodySources");
 r=dynamic_codec::table(a,"report_deposits",v.report_deposits,[](auto&c,auto&x){return custody_deposit(c,x);});if(!r.ok)return r;
 r=dynamic_codec::table(a,"report_pickups",v.report_pickups,[](auto&c,auto&x){return custody_terminal(c,x);});if(!r.ok)return r;
 return dynamic_codec::table(a,"report_withdrawals",v.report_withdrawals,[](auto&c,auto&x){return custody_terminal(c,x);});
}
// 直接typed JSON边界也先拒绝递归重复字段，不依赖文本parser代守门。
Status unique(const BoundaryValue& value,std::size_t depth=0){
    if(depth>CodecVisitor::max_depth)return common_failure("VisitMessageTooDeep");
    if(const auto* fields=value.as<BoundaryValue::Dict>()){if(fields->size()>CodecVisitor::max_items)return common_failure("VisitMessageTooLarge");std::set<StableId> seen;for(const auto& row:*fields){if(!seen.insert(row.first).second)return common_failure("DuplicateVisitMessageField",row.first);auto r=unique(row.second,depth+1);if(!r.ok)return r;}}
    if(const auto* rows=value.as<BoundaryValue::Array>()){if(rows->size()>CodecVisitor::max_items)return common_failure("VisitMessageTooLarge");for(const auto& row:*rows){auto r=unique(row,depth+1);if(!r.ok)return r;}}return common_success();
}
}
void rebuild_visit_message_indexes(VisitMessageState& state){state.posted_by_sender.clear();state.received_by_recipient.clear();for(const auto& row:state.posted.rows())state.posted_by_sender[row.second.sender_actor_id].push_back(row.first);for(const auto& row:state.received.rows())state.received_by_recipient[row.second.contact.actor_id].push_back(row.first);}
Status visit_visit_messages_checkpoint(CodecVisitor& codec,VisitMessageState& value){auto result=state(codec,value);if(result.ok&&codec.reading())rebuild_visit_message_indexes(value);return result;}
BoundaryValue encode_visit_messages(const VisitMessageState& value){auto r=dynamic_projection::encode(value,[](auto&a,auto&v){return state(a,v);});if(!r.ok){boundary::DecodeFailure failure(r.error.original.code,r.error.original.path);failure.error=r.error;throw failure;}return std::move(r.value);}
Status emit_visit_messages_projection(ProjectionWriter&w,std::string_view key,const VisitMessageState&v){return dynamic_projection::emit(w,key,v,[](auto&a,auto&x){return state(a,x);});}
Outcome<VisitMessageState> decode_visit_messages(const BoundaryValue& value){auto result=unique(value);if(!result.ok)return {false,{},result.error};VisitMessageState output;dynamic_codec::JsonArchive archive(value);result=state(archive,output);if(result.ok)result=archive.finish();if(!result.ok)return {false,{},result.error};rebuild_visit_message_indexes(output);return {true,std::move(output),{}};}
Outcome<bool> emit_visit_report_custody_fact_payload_fields(ProjectionWriter&w,const CommonStore&common,const VisitMessageState&s,const VisitMessageCatalog&policies,const InteractionState&i,const InteractionCatalog&catalog,const FactRecord&h){
 if(h.domain!="visit_messages"||(h.kind!=visit_report_deposited_kind&&h.kind!=visit_report_picked_up_kind&&h.kind!=visit_report_withdrawn_kind))return {true,false,{}};
 auto time=read_original_visit_report_custody_moment(common,s,policies,i,catalog,{h.domain,h.kind,h.id},3);if(!time.ok)return {false,false,time.error};
 dynamic_projection::Archive a(w);Status r;if(h.kind==visit_report_deposited_kind)r=custody_deposit(a,const_cast<VisitReportCustodyDeposit&>(*s.report_deposits.find(h.id)));else r=custody_terminal(a,const_cast<VisitReportCustodyTerminal&>(*(h.kind==visit_report_picked_up_kind?s.report_pickups:s.report_withdrawals).find(h.id)));return r.ok?Outcome<bool>{true,true,{}}:Outcome<bool>{false,false,r.error};
}

// Borrow original rows through their one field visitor. No whole State/Boundary
// clone is used merely to discover the three original command-source tails.
Outcome<std::vector<StableId>> read_visit_message_append_delta(const VisitMessageState&before,const VisitMessageState&after){
 std::vector<StableId> added;if(before.version!=after.version||before.version<1||before.version>3)return {false,{},common_failure("VisitMessageOriginalVersionChanged").error};
 auto table=[&](const auto&old,const auto&now,auto visit,bool collect)->Status{
  if(now.size()<old.size())return common_failure("VisitMessageOriginalPrefixChanged");
  for(std::size_t n=0;n<old.size();++n){if(old.order()[n]!=now.order()[n])return common_failure("VisitMessageOriginalPrefixChanged");const auto&id=old.order()[n];auto left=dynamic_projection::encode(*old.find(id),visit);if(!left.ok)return {false,{},left.error};auto right=dynamic_projection::encode(*now.find(id),visit);if(!right.ok)return {false,{},right.error};if(!compare_boundary(left.value,right.value).exact())return common_failure("VisitMessageOriginalRowChanged",id);}
  if(collect)for(std::size_t n=old.size();n<now.size();++n)added.push_back(now.order()[n]);return common_success();
 };
 auto r=table(before.posted,after.posted,[](auto&a,auto&v){return posted(a,v);},true);if(r.ok)r=table(before.received,after.received,[](auto&a,auto&v){return terminal(a,v);},true);if(r.ok)r=table(before.withdrawn,after.withdrawn,[](auto&a,auto&v){return terminal(a,v);},true);
 // Other real owners close their own new tails. Retained rows stay immutable;
 // their appended rows are never misclassified as one of these three receipts.
 if(r.ok)r=table(before.life_sources,after.life_sources,[](auto&a,auto&v){return life_source(a,v);},false);
 if(r.ok)r=table(before.report_deposits,after.report_deposits,[](auto&a,auto&v){return custody_deposit(a,v);},false);
 if(r.ok)r=table(before.report_pickups,after.report_pickups,[](auto&a,auto&v){return custody_terminal(a,v);},false);
 if(r.ok)r=table(before.report_withdrawals,after.report_withdrawals,[](auto&a,auto&v){return custody_terminal(a,v);},false);
 return r.ok?Outcome<std::vector<StableId>>{true,std::move(added),{}}:Outcome<std::vector<StableId>>{false,{},r.error};
}

}
