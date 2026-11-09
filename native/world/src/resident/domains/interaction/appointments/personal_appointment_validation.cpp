// 依Common原事实序重建约期冲突与真实开工；过期只派生，冷读不补账、不借今日岗位反造本人答允。
#include "personal_appointment_detail.hpp"
#include "personal_appointment_time2.hpp"
#include <set>
namespace hanmo::resident {
void rebuild_personal_appointment_indexes(PersonalAppointmentState& state){state.by_participant.clear();for(const auto& row:state.accepted.rows()){state.by_participant[row.second.contact.actor_id].push_back(row.first);state.by_participant[row.second.contact.provider_actor_id].push_back(row.first);}}
Status validate_personal_appointments(const CommonStore& common,const InteractionState& interaction,const InteractionCatalog& definitions,const PersonalAppointmentCatalog& catalog,const PersonalAppointmentState& state,const DynamicStepInput* phase,const PersonalAppointmentTime2Context* exact){
    if(state.version==2){if(!exact)return common_failure("PersonalAppointmentTime2ContextRequired",{},ErrorKind::unsupported_capability);if(phase||&exact->common!=&common||&exact->interaction!=&interaction||&exact->definitions!=&definitions||&exact->catalog!=&catalog||&exact->state!=&state)return common_failure("PersonalAppointmentCandidateMismatch",{},ErrorKind::internal);return validate_personal_appointment_time2(*exact);}
    if(exact||!state.life_sources.empty()||!state.temporal_events.empty())return common_failure("UnselectedPersonalAppointmentTime2Sources",{},ErrorKind::internal);
    if(catalog.version!=state.version)return common_failure("PersonalAppointmentProfileMismatch",{},ErrorKind::internal);
    using namespace appointment_detail;
    auto valid=validate_personal_appointment_catalog(catalog,definitions);if(!valid.ok)return valid;
    Integer observed=common.tick();if(phase){if(phase->from_tick!=common.tick()||common.tick()==INT64_MAX||phase->to_tick!=common.tick()+1)return common_failure("InvalidPersonalAppointmentPhase");observed=phase->to_tick;}
    if(state.version!=1||state.accepted.size()>65536||state.cancelled.size()>65536||state.started.size()>65536||state.finished.size()>65536)return common_failure("InvalidPersonalAppointmentState");
    std::set<StableId> accepted,closed,started_ids,finished_ids,actions;std::map<StableId,const PersonalAppointmentStarted*> live;
    Integer previous=0,previous_tick=-1;std::size_t ac=0,cc=0,sc=0,fc=0;
    const auto check_contact=[&](const PersonalAppointmentContact& c,const PersonalAppointmentPolicy& p,const StableId& actor,const StableId& node,Integer number)->Status{
        const auto* role=definitions.common->roles.find(p.role_id);auto source=serial(c.session_id,"interaction",common.state().next_instance_id);
        if(!role||!source||*source>=number||actor==p.provider_actor_id||!common.actor(actor)||!common.actor(p.provider_actor_id)||c.actor_id!=actor||c.provider_actor_id!=p.provider_actor_id||c.place_id!=role->place_id||c.role_id!=p.role_id||c.dialogue_id!=p.dialogue_id||c.node_id!=node||c.tick<0||c.tick>common.tick())return common_failure("InvalidPersonalAppointmentContact");return common_success();
    };
    for(const auto& row:common.state().facts.rows()){
        const auto& h=row.second;const bool a=h.kind==personal_appointment_accepted_kind,c=h.kind==personal_appointment_cancelled_kind,s=h.kind==personal_appointment_started_kind,f=h.kind==personal_appointment_finished_kind;
        if(!a&&!c&&!s&&!f){if(h.domain==personal_appointment_module)return common_failure("UnknownPersonalAppointmentFact",row.first);continue;}
        const auto number=serial(row.first,h.kind,common.state().next_instance_id);if(h.id!=row.first||h.domain!=personal_appointment_module||!number||*number<=previous)return common_failure("InvalidPersonalAppointmentFact",row.first);previous=*number;Integer tick=0;
        if(a){
            const auto* entry=state.accepted.find(row.first);const auto* p=entry?personal_appointment_policy(catalog,entry->policy_id):nullptr;Integer earliest=0,latest=0;
            if(!entry||entry->fact_id!=row.first||!p||__builtin_add_overflow(entry->contact.tick,p->earliest_offset,&earliest)||__builtin_add_overflow(entry->contact.tick,p->latest_offset,&latest)||latest>2147483646||entry->earliest_tick!=earliest||entry->latest_tick!=latest||entry->duration_ticks!=p->duration_ticks)return common_failure("InvalidPersonalAppointmentTerms",row.first);
            valid=check_contact(entry->contact,*p,entry->contact.actor_id,p->accept_node_id,*number);if(!valid.ok)return valid;tick=entry->contact.tick;
            for(const auto& id:accepted)if(!closed.count(id)){const auto* old=state.accepted.find(id);if(old->latest_tick>=tick&&pair(*old,*entry)&&overlap(*old,*entry))return common_failure("PersonalAppointmentConflict",row.first);}
            accepted.insert(row.first);++ac;
        }else if(c){
            const auto* entry=state.cancelled.find(row.first);const auto* original=entry?state.accepted.find(entry->appointment_id):nullptr;const auto* p=original?personal_appointment_policy(catalog,original->policy_id):nullptr;
            if(!entry||entry->fact_id!=row.first||!original||!p||!accepted.count(original->fact_id)||closed.count(original->fact_id)||started_ids.count(original->fact_id)||entry->contact.tick<original->contact.tick||entry->contact.tick>original->latest_tick)return common_failure("InvalidPersonalAppointmentCancellation",row.first);
            valid=check_contact(entry->contact,*p,original->contact.actor_id,p->cancel_node_id,*number);if(!valid.ok)return valid;tick=entry->contact.tick;closed.insert(original->fact_id);++cc;
        }else if(s){
            const auto* entry=state.started.find(row.first);const auto* original=entry?state.accepted.find(entry->appointment_id):nullptr;const auto* p=original?personal_appointment_policy(catalog,original->policy_id):nullptr;const auto action=entry?serial(entry->action_id,"action",common.state().next_instance_id):std::optional<Integer>{};
            if(!entry||entry->fact_id!=row.first||!original||!p||!accepted.count(original->fact_id)||closed.count(original->fact_id)||!started_ids.insert(original->fact_id).second||!action||*action>=*number||*action<=*serial(original->fact_id,personal_appointment_accepted_kind,common.state().next_instance_id)||!actions.insert(entry->action_id).second||entry->contact.tick<original->earliest_tick||entry->contact.tick>original->latest_tick)return common_failure("InvalidPersonalAppointmentStart",row.first);
            valid=check_contact(entry->contact,*p,original->contact.actor_id,p->meet_node_id,*number);if(!valid.ok)return valid;tick=entry->contact.tick;live.emplace(entry->action_id,entry);++sc;
        }else{
            const auto* entry=state.finished.find(row.first);const auto* beginning=entry?state.started.find(entry->start_fact_id):nullptr;const auto* original=beginning?state.accepted.find(beginning->appointment_id):nullptr;
            if(!entry||entry->fact_id!=row.first||!beginning||!original||entry->appointment_id!=original->fact_id||entry->action_id!=beginning->action_id||!live.count(entry->action_id)||closed.count(original->fact_id)||entry->tick!=beginning->contact.tick+original->duration_ticks||entry->tick>observed||!finished_ids.insert(entry->action_id).second)return common_failure("InvalidPersonalAppointmentCompletion",row.first);
            if(entry->success?!entry->reason.empty():(entry->reason!="PersonalAppointmentParticipantUnavailable"&&entry->reason!="PersonalAppointmentMeetingChanged"))return common_failure("InvalidPersonalAppointmentReason",row.first);
            tick=entry->tick;closed.insert(original->fact_id);live.erase(entry->action_id);++fc;
        }
        if(tick<previous_tick)return common_failure("PersonalAppointmentChronologyMismatch",row.first);previous_tick=tick;
    }
    if(ac!=state.accepted.size()||cc!=state.cancelled.size()||sc!=state.started.size()||fc!=state.finished.size())return common_failure("MissingPersonalAppointmentPayload");
    for(const auto& row:live){const auto& source=*row.second;const auto* a=common.action(row.first);const auto* original=state.accepted.find(source.appointment_id);const auto* actor=common.actor(source.contact.actor_id);const auto* provider=common.actor(source.contact.provider_actor_id);
        if(!a||a->kind!=personal_appointment_action_kind||a->actor_id!=source.contact.actor_id||a->start_tick!=source.contact.tick||a->remaining_ticks!=1||a->participants!=std::vector<StableId>{source.contact.actor_id,source.contact.provider_actor_id}||!actor||!provider||actor->action_id!=row.first||provider->action_id!=row.first||common.tick()!=a->start_tick||!original)return common_failure("PersonalAppointmentActionMismatch",row.first);
        const InteractionSession* session=nullptr;for(const auto& x:interaction.interactions)if(x.actor_id==source.contact.actor_id){if(session)return common_failure("AmbiguousPersonalAppointmentSession",row.first);session=&x;}
        if(!session||!same_session(*session,source.contact))return common_failure("PersonalAppointmentSessionMismatch",row.first);
    }
    // 完成处理的同半日内部可暂留原Common Action，时钟提交后绝不接受孤立或复活的旧动作。
    for(const auto& row:common.state().actions.rows())if(row.second.kind==personal_appointment_action_kind&&!live.count(row.first)&&!(phase&&finished_ids.count(row.first)))return common_failure("OrphanPersonalAppointmentAction",row.first);
    CheckpointMap<StableId,std::vector<StableId>> expected;for(const auto&row:state.accepted.rows()){expected[row.second.contact.actor_id].push_back(row.first);expected[row.second.contact.provider_actor_id].push_back(row.first);}if(expected!=state.by_participant)return common_failure("PersonalAppointmentIndexMismatch");return common_success();
}
Outcome<PersonalAppointmentCommitments> read_personal_appointment_commitments(const CommonStore& common,const PersonalAppointmentState& state,const StableId& actor,Integer from,Integer through,const PersonalAppointmentTime2Context* exact){
    if(state.version==2&&(!exact||&exact->common!=&common||&exact->state!=&state))return {false,{},common_failure("PersonalAppointmentTime2ContextRequired",{},ErrorKind::unsupported_capability).error};if(exact){auto v=validate_personal_appointment_time2(*exact);if(!v.ok)return {false,{},v.error};}
    if(!common.actor(actor)||from<0||through<from||through>2147483647)return {false,{},common_failure("InvalidPersonalAppointmentRead").error};PersonalAppointmentCommitments out{common.state().identity,actor,from,through,{}};
    const auto found=state.by_participant.find(actor);if(found==state.by_participant.end())return {true,std::move(out), {}};
    for(const auto& id:found->second){const auto* a=state.accepted.find(id);if(!a||(a->contact.actor_id!=actor&&a->contact.provider_actor_id!=actor))return {false,{},common_failure("PersonalAppointmentIndexMismatch").error};if(a->latest_tick+a->duration_ticks<=from||a->earliest_tick>through)continue;auto status=exact?read_personal_appointment_status2(*exact,id):Outcome<StableId>{true,appointment_detail::status(state,*a,common.tick()),{}};if(!status.ok)return {false,{},status.error};out.appointments.push_back({id,a->contact.actor_id,a->contact.provider_actor_id,a->contact.place_id,a->earliest_tick,a->latest_tick,a->duration_ticks,std::move(status.value)});}return {true,std::move(out),{}};
}
}
