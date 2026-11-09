// 所有消息递交只发生在原会话中的显式本人确认；远方收件人状态不影响托留报价或泄露其处境。
#include "visit_message.hpp"
#include "visit_message_life.hpp"
#include <set>
namespace hanmo::resident {namespace {
template<class T>Outcome<T> fail(const StableId& code){return {false,{},common_failure(code).error};}
const VisitMessagePolicy* find_policy(const VisitMessageCatalog& catalog,const StableId& id){for(const auto& policy:catalog.policies)if(policy.id==id)return &policy;return nullptr;}
const VisitMessageTerminal* terminal(const OrderedTable<VisitMessageTerminal>& table,const StableId& message){for(const auto& row:table.rows())if(row.second.message_fact_id==message)return &row.second;return nullptr;}
VisitMessageContact contact(const AuthoredProviderSourceProof& proof,Integer tick){const auto& m=proof.meeting;return {m.actor_id,proof.speaker_actor_id,m.context.place_id,m.context.role_id,m.session_id,m.dialogue_id,m.node_id,tick};}
}
StableId visit_message_command_kind(const VisitMessageCommand& command){return std::holds_alternative<LeaveVisitMessage>(command)?"leave_visit_message":std::holds_alternative<ReceiveVisitMessage>(command)?"receive_visit_message":"withdraw_visit_message";}
BoundaryValue encode_visit_message_command(const VisitMessageCommand& command){using V=BoundaryValue;if(const auto* leave=std::get_if<LeaveVisitMessage>(&command))return boundary::obj({{"policy_id",V{leave->policy_id}}});const auto id=std::holds_alternative<ReceiveVisitMessage>(command)?std::get<ReceiveVisitMessage>(command).message_fact_id:std::get<WithdrawVisitMessage>(command).message_fact_id;return boundary::obj({{"message_fact_id",V{id}}});}
Outcome<VisitMessageCommand> decode_visit_message_command(const StableId& kind,const BoundaryValue& params){try{
    using namespace boundary;std::set<StableId> keys;for(const auto& row:object(params,kind))if(!keys.insert(row.first).second)fail("DuplicateVisitMessageField",row.first);
    if(kind=="leave_visit_message"){fields(params,{"policy_id"},{},kind);auto id=get_string(params,"policy_id",kind);if(id.empty())fail("InvalidVisitMessagePolicy",kind);return {true,LeaveVisitMessage{id},{}};}
    if(kind!="receive_visit_message"&&kind!="withdraw_visit_message")unsupported("UnsupportedVisitMessageCommand",kind);fields(params,{"message_fact_id"},{},kind);auto id=get_string(params,"message_fact_id",kind);if(id.empty())fail("InvalidVisitMessageId",kind);return {true,kind=="receive_visit_message"?VisitMessageCommand{ReceiveVisitMessage{id}}:VisitMessageCommand{WithdrawVisitMessage{id}},{}};
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
Outcome<VisitMessageQuote> preview_visit_message(const CommonStore& common,const VisitMessageState& state,const VisitMessageCatalog& catalog,const InteractionState& interactions,const InteractionCatalog& definitions,const InteractionAccess& access,const StableId& actor,const VisitMessageCommand& command,const VisitMessageLifeContext* life){
    if(state.version>=2){if(!life)return {false,{},common_failure("VisitMessageLifeReadRequired",{},ErrorKind::unsupported_capability).error};if(&life->common!=&common||&life->state!=&state||&life->policies!=&catalog||&life->interaction!=&definitions||&life->interactions!=&interactions)return {false,{},common_failure("VisitMessageLifeCandidateMismatch",{},ErrorKind::internal).error};auto gate=validate_visit_message_life_command(*life,command);if(!gate.ok)return {false,{},gate.error};}
    auto valid=validate_visit_messages(common,state,catalog,definitions,life?&life->clock:nullptr);if(!valid.ok)return {false,{},valid.error};
    const bool leave=std::holds_alternative<LeaveVisitMessage>(command),receive=std::holds_alternative<ReceiveVisitMessage>(command);const VisitMessagePolicy* policy=nullptr;const VisitMessagePosted* message=nullptr;
    if(leave)policy=find_policy(catalog,std::get<LeaveVisitMessage>(command).policy_id);
    else{const auto id=receive?std::get<ReceiveVisitMessage>(command).message_fact_id:std::get<WithdrawVisitMessage>(command).message_fact_id;message=state.posted.find(id);if(message)policy=find_policy(catalog,message->policy_id);}
    if(!policy||actor!=(receive?policy->recipient_actor_id:policy->sender_actor_id))return fail<VisitMessageQuote>("VisitMessageNotKnown");
    auto proof=preview_authored_provider_source(common,interactions,definitions,access,actor,policy->custodian_actor_id);if(!proof.ok){if(proof.error.kind!=ErrorKind::rule)return {false,{},proof.error};return leave?Outcome<VisitMessageQuote>{false,{},proof.error}:fail<VisitMessageQuote>("VisitMessageNotKnown");}const auto& source=proof.value.meeting;
    const auto node=leave?policy->deposit_node_id:receive?policy->receive_node_id:policy->withdraw_node_id;
    if(source.dialogue_id!=policy->dialogue_id||source.node_id!=node||source.context.role_id!=policy->role_id||source.context.kind||source.context.encounter_id)return fail<VisitMessageQuote>(leave?"VisitMessageDeclarationRequired":"VisitMessageNotKnown");
    VisitMessageQuote quote;quote.source=std::move(proof.value);quote.command=command;quote.policy_id=policy->id;
    if(leave){
        for(const auto& row:state.posted.rows())if(row.second.policy_id==policy->id&&!terminal(state.received,row.first)&&!terminal(state.withdrawn,row.first))return fail<VisitMessageQuote>("VisitMessageAlreadyDeposited");
        if(state.posted.size()>=65536)return fail<VisitMessageQuote>("VisitMessageHistoryLimit");
    }else{
        quote.message_fact_id=message->fact_id;const auto* taken=terminal(state.received,message->fact_id);const auto* withdrawn=terminal(state.withdrawn,message->fact_id);
        if(receive&&withdrawn)return fail<VisitMessageQuote>("VisitMessageWithdrawn");if(!receive&&taken)return fail<VisitMessageQuote>("VisitMessageAlreadyDelivered");
        if(receive&&taken)return fail<VisitMessageQuote>("VisitMessageAlreadyDelivered");if(!receive&&withdrawn)return fail<VisitMessageQuote>("VisitMessageAlreadyWithdrawn");
    }
    return {true,std::move(quote),{}};
}
Outcome<StableId> apply_visit_message_in_candidate(CommonStore& common,VisitMessageState& state,const VisitMessageCatalog& catalog,const InteractionState& interactions,const InteractionCatalog& definitions,const InteractionAccess& access,const StableId& actor,const VisitMessageCommand& command,const VisitMessageQuote& quote,const VisitMessageLifeContext* life){
    if(actor!=quote.source.meeting.actor_id||!compare_boundary(encode_visit_message_command(command),encode_visit_message_command(quote.command)).exact()||visit_message_command_kind(command)!=visit_message_command_kind(quote.command))return fail<StableId>("StaleVisitMessageQuote");
    auto valid=revalidate_authored_provider_source(common,interactions,definitions,access,quote.source);if(!valid.ok)return {false,{},valid.error};
    auto fresh=preview_visit_message(common,state,catalog,interactions,definitions,access,actor,command,life);if(!fresh.ok)return {false,{},fresh.error};
    if(fresh.value.policy_id!=quote.policy_id||fresh.value.message_fact_id!=quote.message_fact_id||fresh.value.existing_result_fact_id!=quote.existing_result_fact_id)return fail<StableId>("StaleVisitMessageQuote");
    // 同request_seq重放由Common缓存处理；新的成功业务永远产生本次来源。
    if(!quote.existing_result_fact_id.empty())return fail<StableId>("StaleVisitMessageQuote");
    const auto* policy=find_policy(catalog,quote.policy_id);if(!policy)return fail<StableId>("VisitMessageNotKnown");
    const bool leave=std::holds_alternative<LeaveVisitMessage>(command),receive=std::holds_alternative<ReceiveVisitMessage>(command);const StableId kind=leave?visit_message_posted_kind:receive?visit_message_received_kind:visit_message_withdrawn_kind;
    auto id=common.next_id(kind);if(!id.ok)return id;auto written=common.append_fact({id.value,kind,"visit_messages"});if(!written.ok)return {false,{},written.error};
    Integer actual_tick=common.tick();if(life){auto now=life->clock.read_current_moment();if(!now.ok)return {false,{},now.error};actual_tick=now.value.half_day_tick;}
    bool inserted=false;if(leave)inserted=state.posted.insert(id.value,{id.value,policy->id,policy->sender_actor_id,policy->recipient_actor_id,contact(fresh.value.source,actual_tick)});
    else inserted=(receive?state.received:state.withdrawn).insert(id.value,{id.value,quote.message_fact_id,contact(fresh.value.source,actual_tick)});
    if(!inserted)return fail<StableId>("DuplicateVisitMessageFact");rebuild_visit_message_indexes(state);if(state.version>=2){auto complete=validate_visit_message_life_sources(*life);if(!complete.ok)return {false,{},complete.error};}return {true,id.value,{}};
}
}
