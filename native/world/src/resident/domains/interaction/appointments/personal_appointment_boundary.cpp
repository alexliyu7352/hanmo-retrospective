// 每个成功请求必须对应本次新业务来源；相同请求序的幂等重放仍由原Common缓存处理。
#include "personal_appointment_boundary.hpp"
#include "personal_appointment_detail.hpp"
#include "resident/kernel/command_types.hpp"
#include <set>
namespace hanmo::resident {namespace {
using appointment_detail::serial;
bool fact_kind(const StableId& id){for(const char* kind:{personal_appointment_accepted_kind,personal_appointment_cancelled_kind,personal_appointment_started_kind,personal_appointment_finished_kind})if(id.rfind(StableId{kind}+":",0)==0)return true;return false;}
// 原全局紧前结果只作分配区间辅助；裁剪后的精确身份必须来自永久实际result绑定。
struct Interval {StableId actor;Integer lower=0,upper=0,request=0;};
Outcome<Interval> interval(const CommonStore& common,const StableId& key,const CommitResult& result){
    const auto split=key.rfind(':');if(split==StableId::npos)return {false,{},common_failure("InvalidPersonalAppointmentCacheKey",key).error};const auto actor=key.substr(0,split),digits=key.substr(split+1);Integer request=0;const auto p=std::from_chars(digits.data(),digits.data()+digits.size(),request);const auto* requests=common.state().request_sequences.find(actor);
    if(!common.actor(actor)||!requests||digits.empty()||p.ec!=std::errc{}||p.ptr!=digits.data()+digits.size()||request<1||request>2147483647||request>*requests||std::to_string(request)!=digits)return {false,{},common_failure("InvalidPersonalAppointmentCacheKey",key).error};
    const HistoryRecord* previous=nullptr;const HistoryRecord* selected=nullptr;const HistoryRecord* before=nullptr;for(const auto& row:common.state().history){if(row.result_id==result.result_id){if(selected)return {false,{},common_failure("PersonalAppointmentCachedHistoryMismatch",key).error};selected=&row;before=previous;}previous=&row;}
    if(!selected||selected->actor_id!=actor||selected->kind!=result.kind||selected->tick!=result.tick||result.tick<0||result.tick>common.tick())return {false,{},common_failure("PersonalAppointmentCachedHistoryMismatch",key).error};
    const auto lower=before?serial(before->result_id,"result",common.state().next_instance_id):std::optional<Integer>{0},upper=serial(result.result_id,"result",common.state().next_instance_id);if(!lower||!upper||*lower>=*upper)return {false,{},common_failure("PersonalAppointmentCachedAllocationMismatch",key).error};return {true,{actor,*lower,*upper,request},{}};
}
bool in(const CommonStore& common,const Interval& range,const StableId& id,const StableId& kind){const auto n=serial(id,kind,common.state().next_instance_id);return n&&*n>range.lower&&*n<range.upper;}
}
bool is_personal_appointment_command(const StableId& kind){return kind=="personal_appointment_accept"||kind=="personal_appointment_cancel"||kind==personal_appointment_action_kind;}
StableId personal_appointment_command_kind(const PersonalAppointmentCommand& command){return std::holds_alternative<PersonalAppointmentAccept>(command)?"personal_appointment_accept":std::holds_alternative<PersonalAppointmentCancel>(command)?"personal_appointment_cancel":personal_appointment_action_kind;}
Outcome<PersonalAppointmentCommand> decode_personal_appointment_command(const StableId& kind,const BoundaryValue& value){
    try{using namespace boundary;if(!is_personal_appointment_command(kind))unsupported("UnsupportedPersonalAppointmentCommand",kind);std::set<StableId> names;for(const auto& row:object(value,kind))if(!names.insert(row.first).second)fail("DuplicatePersonalAppointmentCommandField",row.first);const char* key=kind=="personal_appointment_accept"?"policy_id":"appointment_id";fields(value,{key},{},kind);const auto id=get_string(value,key,kind);if(id.empty()||id.size()>256)fail("InvalidPersonalAppointmentCommand",kind);if(kind=="personal_appointment_accept")return {true,PersonalAppointmentAccept{id},{}};if(kind=="personal_appointment_cancel")return {true,PersonalAppointmentCancel{id},{}};return {true,PersonalAppointmentMeet{id},{}};
    }catch(const boundary::DecodeFailure& e){return {false,{},e.error};}
}
BoundaryValue encode_personal_appointment_command(const PersonalAppointmentCommand& command){return std::visit([](const auto& c){using T=std::decay_t<decltype(c)>;if constexpr(std::is_same_v<T,PersonalAppointmentAccept>)return boundary::obj({{"policy_id",BoundaryValue{c.policy_id}}});else return boundary::obj({{"appointment_id",BoundaryValue{c.appointment_id}}});},command);}
BoundaryValue encode_personal_appointment_quote(const PersonalAppointmentQuote& q,bool action){using V=BoundaryValue;return boundary::obj({{"duration",V{action?q.duration_ticks:Integer{0}}},{"policy_id",V{q.policy_id}},{"appointment_id",V{q.appointment_id}},{"earliest_tick",V{q.earliest_tick}},{"latest_tick",V{q.latest_tick}},{"meeting_duration",V{q.duration_ticks}}});}
ActionCompletion personal_appointment_completion(const PersonalAppointmentCompletion& source){ActionCompletion result;result.completed=true;result.success=source.success;result.code=source.success?StableId{"PersonalAppointmentFulfilled"}:source.reason;return result;}
Status validate_personal_appointment_cached_results(const CommonStore& common,const PersonalAppointmentState* state){
    if(state){auto valid=validate_personal_appointment_player_bindings(common,*state);if(!valid.ok)return valid;}
    for(const auto& row:common.state().history)if(is_personal_appointment_command(row.kind)&&!state)return common_failure("PersonalAppointmentProfileRequired",row.result_id);
    std::map<StableId,const PersonalAppointmentStarted*> actions;if(state)for(const auto& row:state->started.rows())if(!actions.emplace(row.second.action_id,&row.second).second)return common_failure("DuplicatePersonalAppointmentAction",row.first);
    std::set<StableId> used,finished;
    for(const auto& row:common.state().results.rows())if(row.second){const auto& result=*row.second;const auto* effect=result.details?std::get_if<InteractionEffect>(&result.details->data):nullptr;const auto* started=result.details?std::get_if<ActionStarted>(&result.details->data):nullptr;const bool owned_effect=effect&&fact_kind(effect->settlement_result),owned_action=started&&actions.count(started->action_id);
        if(is_personal_appointment_command(result.kind)||owned_effect||owned_action){
            if(!state||!result.ok||!is_personal_appointment_command(result.kind))return common_failure("InvalidPersonalAppointmentReceipt",row.first);auto range=interval(common,row.first,result);if(!range.ok)return {false,{},range.error};const PersonalAppointmentContact* source=nullptr;StableId fact,kind;
            if(result.kind==personal_appointment_action_kind){if(!started||!owned_action)return common_failure("InvalidPersonalAppointmentReceipt",row.first);const auto* s=actions.at(started->action_id);source=&s->contact;fact=s->fact_id;kind=personal_appointment_started_kind;if(!in(common,range.value,s->action_id,"action"))return common_failure("PersonalAppointmentCachedAllocationMismatch",row.first);}
            else {if(!effect||!owned_effect||!effect->encounter_id.empty()||effect->party_id||effect->person_id||effect->news_ids)return common_failure("InvalidPersonalAppointmentReceipt",row.first);fact=effect->settlement_result;
                if(result.kind=="personal_appointment_accept"){kind=personal_appointment_accepted_kind;const auto* p=state->accepted.find(fact);if(p)source=&p->contact;}
                else {kind=personal_appointment_cancelled_kind;const auto* p=state->cancelled.find(fact);if(p)source=&p->contact;}
            }
            const auto* header=common.state().facts.find(fact);if(!source||!header||header->id!=fact||header->kind!=kind||header->domain!=personal_appointment_module||source->actor_id!=range.value.actor||source->tick!=result.tick||!in(common,range.value,fact,kind)||!used.insert(fact).second)return common_failure("PersonalAppointmentCachedSourceMismatch",row.first);
            const auto* binding=personal_appointment_player_receipt(*state,fact);if(!binding||binding->result_id!=result.result_id||binding->request_actor_id!=range.value.actor||binding->request_sequence!=range.value.request||binding->result_revision!=result.revision)return common_failure("PersonalAppointmentCachedBindingMismatch",row.first);
            const auto n=serial(fact,kind,common.state().next_instance_id);auto newer=[&](const auto& table){for(const auto& p:table.rows()){const auto other=serial(p.first,kind,common.state().next_instance_id);if(p.second.contact.actor_id==source->actor_id&&other&&*other>*n&&*other<range.value.upper)return true;}return false;};
            if((kind==personal_appointment_accepted_kind&&newer(state->accepted))||(kind==personal_appointment_cancelled_kind&&newer(state->cancelled))||(kind==personal_appointment_started_kind&&newer(state->started)))return common_failure("PersonalAppointmentCachedAllocationMismatch",row.first);
        }
        const auto* advance=result.details?std::get_if<AdvanceDetails>(&result.details->data):nullptr;if(!advance)continue;
        for(const auto& item:advance->completed_actions.rows()){const auto& completion=item.second;const auto beginning=actions.find(item.first);const bool marked=completion.code&&completion.code->rfind("PersonalAppointment",0)==0;if(beginning==actions.end()&&!marked)continue;
            if(!state||beginning==actions.end()||!result.ok||(result.kind!="advance"&&result.kind!="player_plan_step")||!completion.ok||completion.completed!=std::optional<bool>{true}||!completion.success||completion.already_completed||completion.task||completion.military||completion.family||completion.growth||!completion.military_field_order.empty())return common_failure("InvalidPersonalAppointmentCompletionReceipt",item.first);
            const auto* source=appointment_detail::finish(*state,beginning->second->appointment_id);if(!source||source->action_id!=item.first||*completion.success!=source->success||completion.code!=std::optional<StableId>{source->success?StableId{"PersonalAppointmentFulfilled"}:source->reason}||(state->version==1?source->tick!=result.tick:source->tick>result.tick)||!finished.insert(source->fact_id).second)return common_failure("PersonalAppointmentCompletionSourceMismatch",item.first);
            const auto* header=common.state().facts.find(source->fact_id);if(!header||header->id!=source->fact_id||header->kind!=personal_appointment_finished_kind||header->domain!=personal_appointment_module)return common_failure("PersonalAppointmentCompletionSourceMismatch",item.first);
            auto range=interval(common,row.first,result);if(!range.ok)return {false,{},range.error};if(!in(common,range.value,source->fact_id,personal_appointment_finished_kind))return common_failure("PersonalAppointmentCompletionAllocationMismatch",item.first);const auto* binding=personal_appointment_player_receipt(*state,source->fact_id);if(!binding||binding->result_id!=result.result_id||binding->request_actor_id!=range.value.actor||binding->request_sequence!=range.value.request||binding->result_revision!=result.revision)return common_failure("PersonalAppointmentCompletionBindingMismatch",item.first);
        }
    }
    return common_success();
}
}
