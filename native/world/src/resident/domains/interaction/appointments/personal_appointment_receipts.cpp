// 原业务事实在实际result已分配后绑定请求；首history裁剪不能把更早同tick预约重新挂入缓存。
#include "personal_appointment_boundary.hpp"
#include "personal_appointment_detail.hpp"
#include "resident/kernel/command_types.hpp"
#include <map>
#include <set>
namespace hanmo::resident {namespace {
struct Source {StableId kind,actor,action;Integer tick=0;const std::optional<PersonalAppointmentPlayerReceipt>* receipt=nullptr;};
// 每种业务分别取真正的原记录，不从caller传来的kind或Action ID猜另一种事实。
Source source(const PersonalAppointmentState& state,const StableId& id){
    if(const auto* a=state.accepted.find(id))return {personal_appointment_accepted_kind,a->contact.actor_id,{},a->contact.tick,&a->player_receipt};
    if(const auto* a=state.cancelled.find(id))return {personal_appointment_cancelled_kind,a->contact.actor_id,{},a->contact.tick,&a->player_receipt};
    if(const auto* a=state.started.find(id))return {personal_appointment_started_kind,a->contact.actor_id,a->action_id,a->contact.tick,&a->player_receipt};
    if(const auto* a=state.finished.find(id)){const auto* start=state.started.find(a->start_fact_id);return {personal_appointment_finished_kind,start?start->contact.actor_id:StableId{},a->action_id,a->tick,&a->player_receipt};}return {};
}
// 完成只匹配原Advance里相同Action的精确终态；接受/取消与开工绝不互借回执形状。
Status matches(const PersonalAppointmentState& state,const StableId& id,const StableId& actor,const StableId& kind,const ReceiptDetails& details,Integer tick){
    const auto s=source(state,id);if(!s.receipt||(state.version==2&&s.kind==personal_appointment_finished_kind?s.tick>tick:s.tick!=tick))return common_failure("PersonalAppointmentActualResultSourceRequired",id);
    if(s.kind==personal_appointment_finished_kind){const auto* advance=std::get_if<AdvanceDetails>(&details.data);const auto* done=advance?advance->completed_actions.find(s.action):nullptr;const auto* f=state.finished.find(id);
        if((kind!="advance"&&kind!="player_plan_step")||!done||!done->ok||done->completed!=std::optional<bool>{true}||done->success!=std::optional<bool>{f->success}||done->code!=std::optional<StableId>{f->success?StableId{"PersonalAppointmentFulfilled"}:f->reason}||done->already_completed||done->task||done->military||done->family||done->growth||!done->military_field_order.empty())return common_failure("PersonalAppointmentActualCompletionRequired",id);
    }else if(s.actor!=actor)return common_failure("PersonalAppointmentActualResultActorMismatch",id);
    else if(s.kind==personal_appointment_started_kind){const auto* start=std::get_if<ActionStarted>(&details.data);if(kind!=personal_appointment_action_kind||!start||start->action_id!=s.action)return common_failure("PersonalAppointmentActualActionResultRequired",id);}
    else {const auto* effect=std::get_if<InteractionEffect>(&details.data);if(kind!=(s.kind==personal_appointment_accepted_kind?"personal_appointment_accept":"personal_appointment_cancel")||!effect||effect->settlement_result!=id||!effect->encounter_id.empty()||effect->party_id||effect->person_id||effect->news_ids)return common_failure("PersonalAppointmentActualBusinessResultRequired",id);}
    return common_success();
}
// A real Life4 interruption can be produced by its original command rather
// than Advance. It has its own exact terminal and life once, not a fabricated
// player completion receipt. The enclosing full Time2 validator proves it.
bool actual_life_completion(const PersonalAppointmentState&s,const StableId&id){if(s.version!=2)return false;const auto*f=s.finished.find(id);const auto*e=s.temporal_events.find(S3EventTimeKey(S3PersonalAppointmentTable::finished,id,S3EventRole::finished).value());const auto*m=e?std::get_if<S3ExactEventMoment>(&e->precision):nullptr;const auto*c=m?std::get_if<S3ActualTerminationSource>(&m->actual_source):nullptr;if(!f||!c||f->success||f->reason!="PersonalAppointmentParticipantUnavailable"||c->original_cause_ref.domain!="family_life"||c->original_cause_ref.kind!="family_death"||!c->original_action_terminal_ref||c->original_action_terminal_ref->domain!=personal_appointment_module||c->original_action_terminal_ref->kind!=personal_appointment_finished_kind||c->original_action_terminal_ref->id!=id)return false;for(const auto&row:s.life_sources.rows()){const auto&v=row.second;const auto*stopped=std::get_if<S3AppointmentParticipationStopped>(&v.effect.effect);if(v.appointment_id==f->appointment_id&&v.death_ref.id==c->original_cause_ref.id&&v.death_ref.domain==c->original_cause_ref.domain&&v.death_ref.kind==c->original_cause_ref.kind&&stopped&&stopped->original_action_terminal_ref&&stopped->original_action_terminal_ref->id==id)return true;}return false;}
// 真正的表差分只收本次新增ID，原事实删除或相同ID重复不会被当作新业务。
template<class T>bool additions(const OrderedTable<T>* before,const OrderedTable<T>& after,std::vector<StableId>& out){if(before){if(after.size()<before->size())return false;for(const auto& row:before->rows())if(!after.find(row.first))return false;}for(const auto& row:after.rows())if(!before||!before->find(row.first))out.push_back(row.first);return true;}
}
const PersonalAppointmentPlayerReceipt* personal_appointment_player_receipt(const PersonalAppointmentState& state,const StableId& id){const auto s=source(state,id);return s.receipt&&*s.receipt?&**s.receipt:nullptr;}
Outcome<std::vector<StableId>> personal_appointment_new_result_sources(const PersonalAppointmentState* before,const PersonalAppointmentState* after,const StableId& actor,const StableId& kind,const ReceiptDetails& details,Integer tick){
    const auto fail=[]()->Outcome<std::vector<StableId>>{return {false,{},common_failure("PersonalAppointmentNewPlayerSourceRequired").error};};
    if(!after){if(before||is_personal_appointment_command(kind))return fail();return {true,{}, {}};}if((after->version!=1&&after->version!=2)||!before||before->version!=after->version)return fail();
    std::vector<StableId> accepted,cancelled,started,finished;
    if(!additions(&before->accepted,after->accepted,accepted)||!additions(&before->cancelled,after->cancelled,cancelled)||!additions(&before->started,after->started,started)||!additions(&before->finished,after->finished,finished))return fail();
    std::vector<StableId> result;
    if(is_personal_appointment_command(kind)){
        const auto& actual=kind=="personal_appointment_accept"?accepted:kind=="personal_appointment_cancel"?cancelled:started;
        if(actual.size()!=1||accepted.size()+cancelled.size()+started.size()!=1||!finished.empty())return fail();result=actual;
    }else if(kind=="advance"||kind=="player_plan_step"){
        result=finished;const auto* advance=std::get_if<AdvanceDetails>(&details.data);if(!advance){if(!finished.empty())return fail();}
        else for(const auto& item:advance->completed_actions.rows()){
            const bool marked=item.second.code&&item.second.code->rfind("PersonalAppointment",0)==0;bool own=false;for(const auto& row:after->started.rows())if(row.second.action_id==item.first)own=true;if(!marked&&!own)continue;
            std::size_t count=0;for(const auto& id:finished)if(after->finished.find(id)->action_id==item.first)++count;if(count!=1)return fail();
        }
    }else{if(!accepted.empty()||!cancelled.empty()||!started.empty())return fail();for(const auto&id:finished)if(!actual_life_completion(*after,id))return fail();}
    for(const auto& id:result){const auto s=source(*after,id);if(!s.receipt||*s.receipt)return fail();auto valid=matches(*after,id,actor,kind,details,tick);if(!valid.ok)return {false,{},valid.error};}return {true,std::move(result),{}};
}
Status validate_personal_appointment_player_bindings(const CommonStore& common,const PersonalAppointmentState& state){
    using appointment_detail::serial;std::map<StableId,PersonalAppointmentPlayerReceipt> results;std::map<std::pair<StableId,Integer>,StableId> requests;std::map<StableId,StableId> result_kinds;
    for(const auto& row:common.state().facts.rows()){
        if(row.second.domain!=personal_appointment_module)continue;const auto s=source(state,row.first);if(!s.receipt)return common_failure("MissingPersonalAppointmentPayload",row.first);if(!*s.receipt)continue;const auto& p=**s.receipt;
        const auto fact=serial(row.first,s.kind,common.state().next_instance_id),result=serial(p.result_id,"result",common.state().next_instance_id);const auto* seq=common.state().request_sequences.find(p.request_actor_id);
        if(row.second.id!=row.first||row.second.kind!=s.kind||!fact||!result||*result<=*fact||!common.actor(p.request_actor_id)||!seq||p.request_sequence<1||p.request_sequence>2147483647||p.request_sequence>*seq||p.result_revision<1||p.result_revision>common.revision()||(s.kind!=personal_appointment_finished_kind&&p.request_actor_id!=s.actor))return common_failure("InvalidPersonalAppointmentPlayerBinding",row.first);
        const auto old=results.find(p.result_id);if(old!=results.end()&&(s.kind!=personal_appointment_finished_kind||result_kinds[p.result_id]!=personal_appointment_finished_kind||old->second.request_actor_id!=p.request_actor_id||old->second.request_sequence!=p.request_sequence||old->second.result_revision!=p.result_revision))return common_failure("DuplicatePersonalAppointmentPlayerResult",row.first);results[p.result_id]=p;result_kinds[p.result_id]=s.kind;
        const auto key=std::make_pair(p.request_actor_id,p.request_sequence);const auto prior=requests.find(key);if(prior!=requests.end()&&prior->second!=p.result_id)return common_failure("DuplicatePersonalAppointmentPlayerRequest",row.first);requests[key]=p.result_id;
        for(const auto& history:common.state().history)if(history.result_id==p.result_id){const bool correct_kind=s.kind==personal_appointment_finished_kind?(history.kind=="advance"||history.kind=="player_plan_step"):history.kind==(s.kind==personal_appointment_accepted_kind?"personal_appointment_accept":s.kind==personal_appointment_cancelled_kind?"personal_appointment_cancel":personal_appointment_action_kind);if(!correct_kind||history.actor_id!=p.request_actor_id||(state.version==2&&s.kind==personal_appointment_finished_kind?history.tick<s.tick:history.tick!=s.tick))return common_failure("PersonalAppointmentPlayerHistoryMismatch",row.first);}
        const auto* cached=common.state().results.find(p.request_actor_id+":"+std::to_string(p.request_sequence));if(cached){if(!*cached||(*cached)->result_id!=p.result_id||(*cached)->revision!=p.result_revision||!(*cached)->details)return common_failure("PersonalAppointmentPlayerCacheMismatch",row.first);auto valid=matches(state,row.first,p.request_actor_id,(*cached)->kind,*(*cached)->details,(*cached)->tick);if(!valid.ok)return valid;}
    }return common_success();
}
Status bind_personal_appointment_player_result(CommonStore& common,PersonalAppointmentState& state,const std::vector<StableId>& sources,const StableId& actor,Integer request,const CommitResult& result){
    if(sources.empty())return is_personal_appointment_command(result.kind)?common_failure("PersonalAppointmentNewPlayerSourceRequired"):common_success();
    const auto* cached=common.state().results.find(actor+":"+std::to_string(request));const auto* seq=common.state().request_sequences.find(actor);
    if(!result.ok||!result.details||result.tick!=common.tick()||result.revision!=common.revision()||!cached||cached->get()!=&result||!seq||*seq!=request||common.state().history.empty())return common_failure("PersonalAppointmentActualPlayerResultRequired");const auto& history=common.state().history.back();if(history.result_id!=result.result_id||history.actor_id!=actor||history.kind!=result.kind||history.tick!=result.tick)return common_failure("PersonalAppointmentActualPlayerHistoryRequired");
    std::set<StableId> seen;for(const auto& id:sources){const auto s=source(state,id);const auto* head=common.state().facts.find(id);if(!s.receipt||*s.receipt||!head||head->id!=id||head->kind!=s.kind||head->domain!=personal_appointment_module||!seen.insert(id).second)return common_failure("PersonalAppointmentNewPlayerSourceRequired",id);auto valid=matches(state,id,actor,result.kind,*result.details,result.tick);if(!valid.ok)return valid;}
    for(const auto& id:sources)const_cast<std::optional<PersonalAppointmentPlayerReceipt>*>(source(state,id).receipt)->emplace(S3PlayerReceiptBuildView{result.result_id,actor,request,result.revision});
    auto valid=validate_personal_appointment_player_bindings(common,state);if(!valid.ok)for(const auto& id:sources)const_cast<std::optional<PersonalAppointmentPlayerReceipt>*>(source(state,id).receipt)->reset();return valid;
}
}
