// 实际Common事实序折叠唯一保管责任；一帖只能领读或撤回一次，历史不读取今日人物去向。
#include "visit_message.hpp"
#include "resident/kernel/read_stamp.hpp"
#include <charconv>
#include <set>
namespace hanmo::resident {namespace {
std::optional<Integer> serial(const StableId& id,const StableId& prefix,Integer next){const auto start=prefix+":";if(id.compare(0,start.size(),start)!=0)return {};const auto text=id.substr(start.size());Integer number=0;const auto result=std::from_chars(text.data(),text.data()+text.size(),number);if(text.empty()||result.ec!=std::errc{}||result.ptr!=text.data()+text.size()||number<=0||number>=next||std::to_string(number)!=text)return {};return number;}
const VisitMessagePolicy* policy(const VisitMessageCatalog& catalog,const StableId& id){for(const auto& row:catalog.policies)if(row.id==id)return &row;return nullptr;}
Status contact(const CommonStore& common,const InteractionCatalog& interactions,const VisitMessagePolicy& policy,const VisitMessageContact& value,const StableId& actor,const StableId& node,const StableId& fact,const StableId& prefix,Integer actual_tick){
    const auto* role=interactions.common->roles.find(policy.role_id);const auto fact_id=serial(fact,prefix,common.state().next_instance_id),session=serial(value.session_id,"interaction",common.state().next_instance_id);
    if(!common.actor(actor)||!common.actor(policy.custodian_actor_id)||!role||value.actor_id!=actor||value.custodian_actor_id!=policy.custodian_actor_id||value.place_id!=role->place_id||value.role_id!=policy.role_id||value.dialogue_id!=policy.dialogue_id||value.node_id!=node||value.tick<0||value.tick>actual_tick||!fact_id||!session||*session>=*fact_id)return common_failure("InvalidVisitMessageContact",fact);
    return common_success();
}
}
Status validate_visit_messages(const CommonStore& common,const VisitMessageState& state,const VisitMessageCatalog& catalog,const InteractionCatalog& interactions,const CurrentMomentRead* actual_clock){
    Integer actual_tick=common.tick();if(state.version>=2){if(!actual_clock)return common_failure("VisitMessageActualClockRequired",{},ErrorKind::unsupported_capability);auto now=actual_clock->read_current_moment();if(!now.ok)return {false,{},now.error};actual_tick=now.value.half_day_tick;}
    if((state.version<1||state.version>3)||state.version!=catalog.version)return common_failure("UnsupportedVisitMessageVersion");if(state.version<3&&(!state.report_deposits.empty()||!state.report_pickups.empty()||!state.report_withdrawals.empty()))return common_failure("UnselectedVisitReportCustodySources");if(state.version==1&&!state.life_sources.empty())return common_failure("UnselectedVisitMessageLifeSources");for(const auto&row:state.life_sources.rows()){if(row.first!=row.second.effect.result_id)return common_failure("VisitMessageLifeKeyMismatch",row.first);auto shape=validate_s3_leaf_life_effect_shape(row.second.effect,3);if(!shape.ok)return shape;}auto checked=validate_visit_message_catalog(catalog,interactions);if(!checked.ok)return checked;
    if(state.posted.size()>65536||state.received.size()>65536||state.withdrawn.size()>65536)return common_failure("VisitMessageHistoryLimit");
    std::map<StableId,StableId> pending;std::set<StableId> seen;Integer previous=0,previous_tick=-1;std::size_t posted_count=0,received_count=0,withdrawn_count=0;
    for(const auto& row:common.state().facts.rows()){
        const auto& header=row.second;const bool posted=header.kind==visit_message_posted_kind,received=header.kind==visit_message_received_kind,withdrawn=header.kind==visit_message_withdrawn_kind;
        if(!posted&&!received&&!withdrawn){if(state.version==3&&header.domain=="visit_messages"&&(header.kind==visit_report_deposited_kind||header.kind==visit_report_picked_up_kind||header.kind==visit_report_withdrawn_kind))continue;if(header.domain=="visit_messages")return common_failure("UnknownVisitMessageFact",row.first);continue;}
        auto current=serial(row.first,header.kind,common.state().next_instance_id);
        if(header.id!=row.first||header.domain!="visit_messages"||!current||*current<=previous||!seen.insert(row.first).second)return common_failure("InvalidVisitMessageFact",row.first);previous=*current;
        if(posted){
            const auto* message=state.posted.find(row.first);const auto* source=message?policy(catalog,message->policy_id):nullptr;
            if(!message||message->fact_id!=row.first||!source||message->sender_actor_id!=source->sender_actor_id||message->recipient_actor_id!=source->recipient_actor_id||!common.actor(message->recipient_actor_id)||pending.count(source->id))return common_failure("InvalidVisitMessageDeposit",row.first);
            checked=contact(common,interactions,*source,message->contact,source->sender_actor_id,source->deposit_node_id,row.first,visit_message_posted_kind,actual_tick);if(!checked.ok)return checked;
            if(message->contact.tick<previous_tick)return common_failure("VisitMessageChronologyMismatch",row.first);previous_tick=message->contact.tick;
            pending[source->id]=row.first;++posted_count;
        }else{
            const auto* result=(received?state.received:state.withdrawn).find(row.first);const auto* message=result?state.posted.find(result->message_fact_id):nullptr;const auto* source=message?policy(catalog,message->policy_id):nullptr;
            if(!result||result->fact_id!=row.first||!message||!source||pending.find(source->id)==pending.end()||pending.at(source->id)!=message->fact_id||result->contact.tick<message->contact.tick)return common_failure("InvalidVisitMessageTerminal",row.first);
            checked=contact(common,interactions,*source,result->contact,received?source->recipient_actor_id:source->sender_actor_id,received?source->receive_node_id:source->withdraw_node_id,row.first,header.kind,actual_tick);if(!checked.ok)return checked;
            if(result->contact.tick<previous_tick)return common_failure("VisitMessageChronologyMismatch",row.first);previous_tick=result->contact.tick;
            pending.erase(source->id);if(received)++received_count;else ++withdrawn_count;
        }
    }
    if(posted_count!=state.posted.size()||received_count!=state.received.size()||withdrawn_count!=state.withdrawn.size())return common_failure("MissingVisitMessageFact");
    for(const auto& row:state.posted.rows())if(row.first!=row.second.fact_id)return common_failure("InvalidVisitMessageRecordKey",row.first);
    for(const auto* table:{&state.received,&state.withdrawn})for(const auto& row:table->rows())if(row.first!=row.second.fact_id)return common_failure("InvalidVisitMessageRecordKey",row.first);
    CheckpointMap<StableId,std::vector<StableId>> posted_index,received_index;for(const auto&row:state.posted.rows())posted_index[row.second.sender_actor_id].push_back(row.first);for(const auto&row:state.received.rows())received_index[row.second.contact.actor_id].push_back(row.first);if(posted_index!=state.posted_by_sender||received_index!=state.received_by_recipient)return common_failure("VisitMessageObserverIndexMismatch");return common_success();
}
// 缓存只引用这次命令新分配的原业务来源；无额外影子账，也不拿当前角色重造历史。
Status validate_visit_message_cached_source(const CommonStore& common,const VisitMessageState& state,const StableId& key,const StableId& kind,const StableId& result_id,Integer tick,const StableId& fact,const CurrentMomentRead*actual_clock){
    Integer actual_tick=common.tick();if(state.version>=2){if(!actual_clock)return common_failure("VisitMessageActualClockRequired",{},ErrorKind::unsupported_capability);auto now=actual_clock->read_current_moment();if(!now.ok)return {false,{},now.error};actual_tick=now.value.half_day_tick;}
    const auto split=key.rfind(':');if(split==StableId::npos)return common_failure("InvalidVisitMessageCacheKey",key);
    const auto actor=key.substr(0,split),digits=key.substr(split+1);Integer request=0;const auto parsed=std::from_chars(digits.data(),digits.data()+digits.size(),request);const auto* sequence=common.state().request_sequences.find(actor);
    if(!common.actor(actor)||!sequence||digits.empty()||parsed.ec!=std::errc{}||parsed.ptr!=digits.data()+digits.size()||request<1||request>2147483647||request>*sequence||std::to_string(request)!=digits)return common_failure("InvalidVisitMessageCacheKey",key);
    const HistoryRecord* previous=nullptr;const HistoryRecord* selected=nullptr;const HistoryRecord* before=nullptr;
    for(const auto& row:common.state().history){if(row.result_id==result_id){if(selected)return common_failure("VisitMessageCachedHistoryMismatch",key);selected=&row;before=previous;}previous=&row;}
    if(!selected||selected->actor_id!=actor||selected->kind!=kind||selected->tick!=tick||tick<0||tick>actual_tick)return common_failure("VisitMessageCachedHistoryMismatch",key);
    const VisitMessageContact* source=nullptr;StableId prefix;
    if(kind=="leave_visit_message"){prefix=visit_message_posted_kind;const auto* p=state.posted.find(fact);if(p)source=&p->contact;}
    else if(kind=="receive_visit_message"||kind=="withdraw_visit_message"){prefix=kind=="receive_visit_message"?visit_message_received_kind:visit_message_withdrawn_kind;const auto* p=(kind=="receive_visit_message"?state.received:state.withdrawn).find(fact);if(p)source=&p->contact;}
    else if(state.version==3&&kind=="deposit_known_report"){prefix=visit_report_deposited_kind;const auto*p=state.report_deposits.find(fact);if(p)source=&p->contact;}
    else if(state.version==3&&(kind=="pickup_known_report"||kind=="withdraw_known_report")){prefix=kind=="pickup_known_report"?visit_report_picked_up_kind:visit_report_withdrawn_kind;const auto*p=(kind=="pickup_known_report"?state.report_pickups:state.report_withdrawals).find(fact);if(p)source=&p->contact;}
    if(!source||source->actor_id!=actor||source->tick!=tick)return common_failure("VisitMessageCachedSourceMismatch",key);
    const auto next=common.state().next_instance_id;const auto upper=serial(result_id,"result",next),lower=before?serial(before->result_id,"result",next):std::optional<Integer>{0},origin=serial(fact,prefix,next);
    if(!upper||!lower||!origin||*lower>=*upper||*origin<=*lower||*origin>=*upper)return common_failure("VisitMessageCachedAllocationMismatch",key);
    // 首条保留history的更早下界可能已裁掉；原本次最新同人同类来源仍不能换成更旧的一条。
    auto newer=[&](const StableId& id,const VisitMessageContact& c){const auto n=serial(id,prefix,next);return c.actor_id==actor&&n&&*n>*origin&&*n<*upper;};
    if(kind=="leave_visit_message"){for(const auto& row:state.posted.rows())if(newer(row.first,row.second.contact))return common_failure("VisitMessageCachedAllocationMismatch",key);}
    else if(kind=="receive_visit_message"||kind=="withdraw_visit_message"){for(const auto&row:(kind=="receive_visit_message"?state.received:state.withdrawn).rows())if(newer(row.first,row.second.contact))return common_failure("VisitMessageCachedAllocationMismatch",key);}
    else if(kind=="deposit_known_report"){for(const auto&row:state.report_deposits.rows())if(newer(row.first,row.second.contact))return common_failure("VisitMessageCachedAllocationMismatch",key);}
    else for(const auto&row:(kind=="pickup_known_report"?state.report_pickups:state.report_withdrawals).rows())if(newer(row.first,row.second.contact))return common_failure("VisitMessageCachedAllocationMismatch",key);
    return common_success();
}

}
