// 冷读按原Common顺序核开工、真实一期答复及原知识前界；不回填位置、历史同意或死人答复。
#include "contact_inquiry_detail.hpp"
#include "contact_inquiry_time2.hpp"
#include "resident/domains/family/actor_presence_read.hpp"
#include <set>
#include <tuple>
namespace hanmo::resident {
Status validate_contact_inquiries(const KnownContactLocationAccess& read,const InteractionState& interaction,const InteractionCatalog& definitions,const ContactInquiryCatalog& catalog,const ContactInquiryState& state,const FamilyDeathOriginRead& deaths,const DynamicStepInput* phase,const ContactInquiryTime2Context*exact){
    if(state.version==2){if(!exact)return common_failure("ContactInquiryTime2ContextRequired",{},ErrorKind::unsupported_capability);if(phase||&read!=&exact->knowledge||&interaction!=&exact->interaction||&definitions!=&exact->definitions||&catalog!=&exact->catalog||&state!=&exact->state)return common_failure("ContactInquiryCandidateMismatch",{},ErrorKind::internal);return validate_contact_inquiry_time2(*exact);}if(exact||catalog.version!=1||!state.life_sources.empty()||!state.temporal_events.empty())return common_failure("ContactInquiryVersionMismatch",{},ErrorKind::internal);
    using namespace contact_inquiry_detail;const auto& common=read.common;auto valid=validate_contact_inquiry_catalog(catalog,definitions);if(!valid.ok)return valid;
    Integer observed=common.tick();if(phase){if(phase->from_tick!=common.tick()||common.tick()==INT64_MAX||phase->to_tick!=common.tick()+1)return common_failure("InvalidContactInquiryPhase");observed=phase->to_tick;}
    if(state.version!=1||state.started.size()>65536||state.finished.size()>65536||(!read.appointments&&!read.messages))return common_failure("InvalidContactInquiryState");
    Integer previous=0,last_tick=-1;std::size_t sc=0,fc=0;std::set<StableId> actions,closed;std::map<StableId,const ContactInquiryStarted*> live;std::set<std::tuple<StableId,StableId,StableId>> asked;
    // 热门只允许本人活着发问/答复；后来死亡不倒销已完成的原接触。
    const auto presence=[&](const ContactInquiryContact& contact,Integer tick,const StableId& fact)->Status{for(const auto* id:{&contact.actor_id,&contact.provider_actor_id}){auto result=read_actor_presence_before_fact(common,read.common_catalog,deaths,*id,tick,fact,phase);if(!result.ok)return {false,{},result.error};if(!result.value.alive)return common_failure("ContactInquiryParticipantNotAliveAtSource",fact);}return common_success();};
    for(const auto& row:common.state().facts.rows()){
        const auto& h=row.second;const bool opening=h.kind==contact_inquiry_started_kind,ending=h.kind==contact_inquiry_finished_kind;if(!opening&&!ending){if(h.domain==contact_inquiry_module)return common_failure("UnknownContactInquiryFact",row.first);continue;}
        auto number=serial(row.first,h.kind,common.state().next_instance_id);if(h.id!=row.first||h.domain!=contact_inquiry_module||!number||*number<=previous)return common_failure("InvalidContactInquiryFact",row.first);previous=*number;Integer tick=0;
        if(opening){
            const auto* x=state.started.find(row.first);const auto* p=x?contact_inquiry_policy(catalog,x->policy_id):nullptr;const auto* role=p?definitions.common->roles.find(p->role_id):nullptr;auto action=x?serial(x->action_id,"action",common.state().next_instance_id):std::optional<Integer>{};auto session=x?serial(x->contact.session_id,"interaction",common.state().next_instance_id):std::optional<Integer>{};
            if(!x||x->fact_id!=row.first||!p||!role||!action||*action+1!=*number||!session||*session>=*action||!actions.insert(x->action_id).second||sc>=state.started.size()||state.started.order()[sc]!=row.first)return common_failure("InvalidContactInquiryStart",row.first);
            const auto& c=x->contact;if(c.actor_id.empty()||c.actor_id==p->provider_actor_id||c.actor_id==p->subject_actor_id||c.provider_actor_id!=p->provider_actor_id||c.role_id!=p->role_id||c.place_id!=role->place_id||c.dialogue_id!=p->dialogue_id||c.node_id!=p->node_id||c.tick<0||c.tick>=2147483646||c.tick>common.tick()||!asked.emplace(c.actor_id,x->policy_id,c.session_id).second)return common_failure("InvalidContactInquiryContact",row.first);
            valid=presence(c,c.tick,row.first);if(!valid.ok)return valid;
            for(const auto& old:live){const auto& other=old.second->contact;if(other.actor_id==c.actor_id||other.actor_id==c.provider_actor_id||other.provider_actor_id==c.actor_id||other.provider_actor_id==c.provider_actor_id)return common_failure("ContactInquiryParticipantConflict",row.first);}
            tick=c.tick;live.emplace(x->action_id,x);++sc;
        }else{
            const auto* x=state.finished.find(row.first);const auto* start=x?state.started.find(x->start_fact_id):nullptr;const auto* p=start?contact_inquiry_policy(catalog,start->policy_id):nullptr;
            if(!x||x->fact_id!=row.first||!start||!p||x->action_id!=start->action_id||!live.count(x->action_id)||!closed.insert(x->action_id).second||x->tick!=start->contact.tick+p->duration_ticks||x->tick>observed||fc>=state.finished.size()||state.finished.order()[fc]!=row.first)return common_failure("InvalidContactInquiryCompletion",row.first);
            if(!x->success){if((x->reason!="ContactInquiryParticipantUnavailable"&&x->reason!="ContactInquiryMeetingChanged")||!x->reply_kind.empty()||x->answer)return common_failure("InvalidContactInquiryFailure",row.first);}
            else{
                if(!x->reason.empty())return common_failure("InvalidContactInquirySuccess",row.first);valid=presence(start->contact,x->tick,row.first);if(!valid.ok)return valid;
                if(p->disclosure=="decline"){if(x->reply_kind!="declined"||x->answer)return common_failure("ContactInquiryDeclinedAnswerMismatch",row.first);}
                else{auto expected=read_known_contact_location_before_inquiry(read,start->contact.provider_actor_id,p->subject_actor_id,start->contact.tick,start->fact_id,phase);if(!expected.ok)return {false,{},expected.error};if(expected.value){expected.value->learned_tick=x->tick;if(x->reply_kind!="known"||!x->answer||!same_answer(*x->answer,*expected.value))return common_failure("ContactInquiryAnswerSourceMismatch",row.first);}else if(x->reply_kind!="unknown"||x->answer)return common_failure("ContactInquiryUnknownSourceMismatch",row.first);}
            }
            tick=x->tick;live.erase(x->action_id);++fc;
        }
        if(tick<last_tick)return common_failure("ContactInquiryChronologyMismatch",row.first);last_tick=tick;
    }
    if(sc!=state.started.size()||fc!=state.finished.size())return common_failure("MissingContactInquiryPayload");
    for(const auto& row:live){const auto& source=*row.second;const auto* a=common.action(row.first);const auto* actor=common.actor(source.contact.actor_id);const auto* provider=common.actor(source.contact.provider_actor_id);
        if(!a||a->kind!=contact_inquiry_action_kind||a->actor_id!=source.contact.actor_id||a->start_tick!=source.contact.tick||a->remaining_ticks!=1||a->participants!=std::vector<StableId>{source.contact.actor_id,source.contact.provider_actor_id}||!actor||!provider||actor->action_id!=row.first||provider->action_id!=row.first||common.tick()!=a->start_tick)return common_failure("ContactInquiryActionMismatch",row.first);
        const InteractionSession* session=nullptr;for(const auto& x:interaction.interactions)if(x.actor_id==source.contact.actor_id){if(session)return common_failure("AmbiguousContactInquirySession",row.first);session=&x;}if(!session||!same_session(*session,source.contact))return common_failure("ContactInquirySessionMismatch",row.first);
    }
    // 只有本phase刚完成且字段仍精确的原Action可以暂留，稳定冷档绝不保孤工单。
    for(const auto& row:common.state().actions.rows())if(row.second.kind==contact_inquiry_action_kind&&!live.count(row.first)){
        const ContactInquiryStarted* source=nullptr;for(const auto& start:state.started.rows())if(start.second.action_id==row.first)source=&start.second;const auto* end=source?finished(state,source->fact_id):nullptr;
        if(!phase||!source||!end||end->tick!=phase->to_tick||row.second.remaining_ticks!=0||row.second.actor_id!=source->contact.actor_id||row.second.start_tick!=source->contact.tick||row.second.participants!=std::vector<StableId>{source->contact.actor_id,source->contact.provider_actor_id})return common_failure("OrphanContactInquiryAction",row.first);
    }
    CheckpointMap<StableId,std::vector<StableId>> expected;for(const auto&row:state.started.rows())expected[row.second.contact.actor_id].push_back(row.first);if(expected!=state.by_listener)return common_failure("ContactInquiryIndexMismatch");return common_success();
}
}
