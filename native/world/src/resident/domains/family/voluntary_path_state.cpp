#include "voluntary_path_state.hpp"
#include "voluntary_succession.hpp"
#include "voluntary_succession_candidate.hpp"
#include <charconv>
#include <set>
namespace hanmo::resident {namespace {
Integer serial(const CommonStore& c,const StableId& id,const StableId& prefix){const auto lead=prefix+":";if(id.rfind(lead,0)!=0)return -1;const auto digits=id.substr(lead.size());Integer n=0;const auto p=std::from_chars(digits.data(),digits.data()+digits.size(),n);return p.ec==std::errc{}&&p.ptr==digits.data()+digits.size()&&n>0&&n<c.state().next_instance_id&&std::to_string(n)==digits?n:-1;}
bool prior(const CommonStore& c,const StableId& a,const StableId& b){const auto& order=c.state().facts.order();auto i=std::find(order.begin(),order.end(),a),j=std::find(order.begin(),order.end(),b);auto number=[&](const StableId& id){const auto colon=id.find(':');return colon==StableId::npos?-1:serial(c,id,id.substr(0,colon));};const auto x=number(a),y=number(b);return x>0&&x<y&&i!=order.end()&&j!=order.end()&&i<j;}
const SuccessionConsentFact* plan(const FamilyState& f,const StableId& id){const auto* value=f.facts.find(id);return value?std::get_if<SuccessionConsentFact>(value):nullptr;}
Status historical_player(const VoluntaryPathContext& x,const StableId& actor,Integer tick,const StableId& fact){if(!x.player_before)return common_failure("VoluntaryPathPlayerHistoryRequired");auto p=x.player_before(tick,fact);return p.ok?(p.value==actor?common_success():common_failure("VoluntaryPathHistoricalPlayerMismatch",fact)):Status{false,{},p.error};}
Status living_actor(const VoluntaryPathContext& x,const StableId& actor,Integer tick,const StableId& before){
    if(!x.people.death_origin)return common_failure("VoluntaryRelationDeathReadRequired");const auto& c=x.people.common;
    auto present=before.empty()?read_actor_presence(c,x.people.catalog,x.people.death_origin,actor):read_actor_presence_before_fact(c,x.people.catalog,x.people.death_origin,actor,tick,before);
    if(!present.ok)return {false,{},present.error};if(!present.value.alive)return common_failure("VoluntaryPathLivingActorRequired",actor);
    if(!before.empty()){
        if(!present.value.creation_fact_id.empty()&&!prior(c,present.value.creation_fact_id,before))return common_failure("VoluntaryPathActorBoundaryMismatch",actor);
        auto death=x.people.death_origin(actor);if(!death.ok)return {false,{},death.error};
        if(death.value&&!prior(c,before,death.value->casualty_fact_id))return common_failure("VoluntaryPathDeathBoundaryMismatch",actor);
    }return common_success();
}
Status withdrawal(const VoluntaryPathContext& x,const SuccessionConsentSource& a,const KnownPersonalDialogueProof& p,Integer tick,Integer revision,const StableId& before){
    const auto* d=x.catalog.definitions.find(a.request_id);if(!d||!x.meetings)return common_failure("VoluntaryPathWithdrawalSourceRequired");
    const auto& c=x.people.common;auto t=x.templates(d->template_id);if(!t.ok)return {false,{},t.error};const auto& m=p.declaration.meeting;
    if(p.template_id!=d->template_id||m.dialogue_id!=t.value.dialogue_id||m.node_id!=d->withdraw_node_id||m.node_kind!="say"||
       m.actor_id!=a.personal.predecessor_actor_id||m.provider_actor_id!=a.personal.successor_actor_id||p.declaration.speaker_actor_id!=m.provider_actor_id||
       p.listener_origin.actor_id!=m.actor_id||p.provider_origin.actor_id!=m.provider_actor_id||m.source_identity.world_id!=a.world_id||
       m.source_identity.execution_epoch<1||m.source_identity.tick!=tick||m.source_identity.revision!=revision||revision<a.accepted_revision||
       tick<a.accepted_tick||tick>c.tick()||revision>c.revision()||m.session_id.empty()||m.context.kind!=std::optional<StableId>{"narrative"}||
       !m.context.role_id.empty()||m.context.provider_actor_id!=m.provider_actor_id||m.context.place_id.empty()||m.context.encounter_id||m.context.input_error)
        return common_failure("VoluntaryPathWithdrawalSourceMismatch");
    const auto* head=c.state().facts.find(p.opening_fact_id);
    if(serial(c,p.opening_fact_id,known_personal_dialogue_opened_kind)<1||!head||head->id!=p.opening_fact_id||head->kind!=known_personal_dialogue_opened_kind||head->domain!=known_personal_dialogue_module||
       (!before.empty()&&!prior(c,p.opening_fact_id,before)))return common_failure("VoluntaryPathWithdrawalOpeningRequired");
    for(const auto* actor:{&m.actor_id,&m.provider_actor_id}){auto live=living_actor(x,*actor,tick,before);if(!live.ok)return live;}
    if(!before.empty())return x.meetings->known_personal_dialogue_before_fact(p,tick,before);
    const auto& now=c.state().identity;
    if(m.source_identity.execution_epoch!=now.execution_epoch||revision!=now.revision||tick!=now.tick)return common_failure("StaleVoluntaryPathWithdrawal");
    auto actual=x.meetings->known_personal_dialogue(m.actor_id,m.provider_actor_id,d->template_id);if(!actual.ok)return {false,{},actual.error};
    return compare_boundary(encode_known_personal_dialogue_proof(p),encode_known_personal_dialogue_proof(actual.value)).exact()?common_success():common_failure("StaleVoluntaryPathWithdrawal");
}
Status same_candidate(CommonStore& c,FamilyState& f,const VoluntaryPathContext& x){return &c==&x.people.common&&&f==&x.people.family?common_success():common_failure("VoluntaryPathCandidateRequired");}
}
Status validate_voluntary_path_history(const VoluntaryPathContext& x,const VoluntaryPathState& state){
    auto valid=validate_voluntary_path_state_shape(state);if(!valid.ok)return valid;
    valid=validate_voluntary_path_catalog(x.catalog,x.original_requests,x.interactions,x.templates,x.other_declarations);if(!valid.ok)return valid;
    const auto& c=x.people.common;const auto& f=x.people.family;
    valid=validate_voluntary_succession_structure(c,f);if(!valid.ok)return valid;
    std::set<StableId> consumed;std::set<StableId> cancelled;
    for(const auto& row:state.cancellations.rows()){
        const auto& end=row.second;const auto* source=plan(f,end.plan_id);const auto* head=c.state().facts.find(row.first);
        if(serial(c,row.first,"voluntary_path")<1||!head||head->id!=row.first||head->kind!=voluntary_path_cancelled_kind||head->domain!=voluntary_path_module||
           !source||!source->conclusion||source->conclusion->kind!="cancelled"||source->conclusion->tick!=end.tick||end.tick<source->agreement.accepted_tick||
           end.tick>c.tick()||end.revision<source->agreement.accepted_revision||end.revision>c.revision()||end.actor_id!=source->agreement.personal.predecessor_actor_id||
           !prior(c,end.plan_id,row.first)||!cancelled.insert(end.plan_id).second)return common_failure("VoluntaryPathCancellationMismatch",row.first);
        valid=historical_player(x,end.actor_id,end.tick,row.first);if(!valid.ok)return valid;
        valid=living_actor(x,end.actor_id,end.tick,row.first);if(!valid.ok)return valid;
        if(end.withdrawal){valid=withdrawal(x,source->agreement,*end.withdrawal,end.tick,end.revision,row.first);if(!valid.ok)return valid;}
    }
    for(const auto& row:f.facts.rows())if(const auto* p=std::get_if<SuccessionConsentFact>(&row.second)){
        const auto* d=x.catalog.definitions.find(p->agreement.request_id);const auto* source=state.consents.find(row.first);
        if(d){if(!source)return common_failure("VoluntaryPathConsentSourceMissing",row.first);
            valid=validate_voluntary_path_consent_before_fact(x,p->agreement,*source,row.first);if(!valid.ok)return valid;consumed.insert(row.first);
        }else{
            if(source||!x.original_requests.enabled)return common_failure("VoluntaryPathUnknownOriginalRequest",row.first);
            valid=validate_succession_consent_source(c,f,x.people.catalog,x.interactions,x.original_requests,p->agreement);if(!valid.ok)return valid;
            if(!prior(c,p->agreement.relation_fact_id,row.first))return common_failure("SuccessionConsentBeforeRelationship",row.first);
            valid=historical_player(x,p->agreement.personal.predecessor_actor_id,p->agreement.accepted_tick,row.first);if(!valid.ok)return valid;
        }
        if(p->conclusion&&p->conclusion->kind=="cancelled"&&!cancelled.count(row.first))return common_failure("VoluntaryPathCancellationSourceMissing",row.first);
        if(p->conclusion&&p->conclusion->kind=="completed"){
            const auto& id=*p->conclusion->succession_fact_id;
            if(!prior(c,row.first,id))return common_failure("VoluntaryPathCompletionOrderMismatch",id);
            auto relation=read_voluntary_relation_before_fact(x.people,d?d->relation_kind:VoluntaryRelationKind::adoption,
                p->agreement.personal.predecessor_actor_id,p->agreement.personal.successor_actor_id,p->agreement.relation_fact_id,p->conclusion->tick,id);
            if(!relation.ok)return {false,{},relation.error};
            if(d&&relation.value.skill_id!=d->disciple_skill_id)return common_failure("VoluntaryPathDiscipleSkillMismatch",id);
            valid=historical_player(x,p->agreement.personal.predecessor_actor_id,p->conclusion->tick,id);if(!valid.ok)return valid;
        }
    }
    if(consumed.size()!=state.consents.size())return common_failure("VoluntaryPathUnexpectedConsentSource");
    for(const auto& row:c.state().facts.rows())if(row.second.domain==voluntary_path_module&&
        (row.second.kind!=voluntary_path_cancelled_kind||!state.cancellations.find(row.first)))return common_failure("VoluntaryPathMissingPayload",row.first);
    return common_success();
}
Outcome<StableId> accept_voluntary_path_in_candidate(CommonStore& c,FamilyState& f,VoluntaryPathState& state,
    const VoluntaryPathContext& x,const VoluntaryPathConsentQuote& quote){
    auto valid=same_candidate(c,f,x);if(!valid.ok)return {false,{},valid.error};
    valid=validate_voluntary_path_history(x,state);if(!valid.ok)return {false,{},valid.error};
    valid=revalidate_voluntary_path_consent(x,quote);if(!valid.ok)return {false,{},valid.error};
    auto id=record_succession_consent_in_candidate(c,f,quote.agreement);if(!id.ok)return id;
    if(const auto* old=state.consents.find(id.value)){if(!voluntary_path_source_equal(*old,quote.path))return {false,{},common_failure("VoluntaryPathConsentSourceMismatch").error};}
    else if(!state.consents.insert(id.value,quote.path))return {false,{},common_failure("DuplicateVoluntaryPathConsentSource").error};
    valid=validate_voluntary_path_history(x,state);return valid.ok?id:Outcome<StableId>{false,{},valid.error};
}
Outcome<StableId> accept_original_voluntary_path_in_candidate(CommonStore& c,FamilyState& f,VoluntaryPathState& state,
    const VoluntaryPathContext& x,const InteractionState& interactions,const InteractionAccess& access,const SuccessionConsentQuote& quote){
    auto valid=same_candidate(c,f,x);if(!valid.ok)return {false,{},valid.error};valid=validate_voluntary_path_history(x,state);if(!valid.ok)return {false,{},valid.error};
    if(x.catalog.definitions.find(quote.agreement.request_id))return {false,{},common_failure("VoluntaryOriginalRequestRequired").error};
    valid=revalidate_succession_consent(c,f,interactions,x.interactions,access,x.original_requests,quote);if(!valid.ok)return {false,{},valid.error};
    auto id=record_succession_consent_in_candidate(c,f,quote.agreement);if(!id.ok)return id;
    valid=validate_voluntary_path_history(x,state);return valid.ok?id:Outcome<StableId>{false,{},valid.error};
}
Outcome<SuccessionPersonalQuote> preview_accepted_voluntary_path_assets(const VoluntaryPathContext& x,const VoluntaryPathState& state,const StableId& id){
    auto valid=validate_voluntary_path_history(x,state);if(!valid.ok)return {false,{},valid.error};
    const auto* p=plan(x.people.family,id);const auto* source=state.consents.find(id);
    if(!p||p->conclusion)return {false,{},common_failure("VoluntaryPathPendingPlanRequired",id).error};
    const auto* d=x.catalog.definitions.find(p->agreement.request_id);const auto* original=x.original_requests.definitions.find(p->agreement.request_id);
    if((d!=nullptr)!=(source!=nullptr)||(!d&&!original))return {false,{},common_failure("VoluntaryPathPolicyRequired",id).error};
    const auto& first=p->agreement.personal.predecessor_actor_id;const auto& second=p->agreement.personal.successor_actor_id;
    if(first!=x.people.common.state().metadata.player_actor_id)return {false,{},common_failure("VoluntaryPathCurrentPlayerRequired").error};
    auto relation=read_current_voluntary_relation(x.people,d?d->relation_kind:VoluntaryRelationKind::adoption,first,second,p->agreement.relation_fact_id);
    if(!relation.ok)return {false,{},relation.error};
    if(d&&relation.value.skill_id!=d->disciple_skill_id)return {false,{},common_failure("VoluntaryPathDiscipleSkillMismatch",id).error};
    if(!x.stable_parties)return {false,{},common_failure("VoluntaryPathOriginalStableReadRequired").error};
    valid=x.stable_parties(first,second);if(!valid.ok)return {false,{},valid.error};
    auto assets=preview_succession_personal_assets(x.people.common,first,second,d?d->personal:original->personal);if(!assets.ok)return assets;
    return succession_personal_terms_equal(assets.value.terms,p->agreement.personal)?assets:Outcome<SuccessionPersonalQuote>{false,{},common_failure("SuccessionAcceptedAssetsChanged",id).error};
}
Outcome<StableId> complete_voluntary_path_in_candidate(CommonStore& c,FamilyState& f,VoluntaryPathState& state,const VoluntaryPathContext& x,const StableId& id,const SuccessionPersonalQuote& quote){
    auto valid=same_candidate(c,f,x);if(!valid.ok)return {false,{},valid.error};
    auto fresh=preview_accepted_voluntary_path_assets(x,state,id);if(!fresh.ok)return {false,{},fresh.error};
    if(!succession_personal_terms_equal(fresh.value.terms,quote.terms))return {false,{},common_failure("StaleSuccessionPersonalQuote").error};
    const Integer revision=c.revision();auto done=execute_succession_plan_in_candidate(c,f,id,quote);if(!done.ok)return done;
    if(!state.completion_revisions.insert(done.value,revision))return {false,{},common_failure("DuplicateVoluntaryPathCompletionSource").error};
    valid=validate_voluntary_path_history(x,state);return valid.ok?done:Outcome<StableId>{false,{},valid.error};
}
Outcome<KnownPersonalDialogueProof> preview_voluntary_path_withdrawal(const VoluntaryPathContext& x,const VoluntaryPathState& state,const StableId& id){
    auto valid=validate_voluntary_path_history(x,state);if(!valid.ok)return {false,{},valid.error};const auto* p=plan(x.people.family,id);
    if(!p||p->conclusion||!state.consents.find(id)||p->agreement.personal.predecessor_actor_id!=x.people.common.state().metadata.player_actor_id)
        return {false,{},common_failure("VoluntaryPathPendingPlanRequired",id).error};
    const auto* d=x.catalog.definitions.find(p->agreement.request_id);if(!d||!x.meetings)return {false,{},common_failure("VoluntaryPathWithdrawalSourceRequired").error};
    auto proof=x.meetings->known_personal_dialogue(p->agreement.personal.predecessor_actor_id,p->agreement.personal.successor_actor_id,d->template_id);if(!proof.ok)return proof;
    valid=withdrawal(x,p->agreement,proof.value,x.people.common.tick(),x.people.common.revision(),{});
    return valid.ok?proof:Outcome<KnownPersonalDialogueProof>{false,{},valid.error};
}
Outcome<StableId> cancel_voluntary_path_in_candidate(CommonStore& c,FamilyState& f,VoluntaryPathState& state,const VoluntaryPathContext& x,const StableId& plan_id,const std::optional<KnownPersonalDialogueProof>& proof){
    auto valid=same_candidate(c,f,x);if(!valid.ok)return {false,{},valid.error};valid=validate_voluntary_path_history(x,state);if(!valid.ok)return {false,{},valid.error};
    const auto* p=plan(f,plan_id);if(!p||p->conclusion||p->agreement.personal.predecessor_actor_id!=c.state().metadata.player_actor_id)return {false,{},common_failure("VoluntaryPathPendingPlanRequired",plan_id).error};
    const StableId actor=p->agreement.personal.predecessor_actor_id;valid=living_actor(x,actor,c.tick(),{});if(!valid.ok)return {false,{},valid.error};
    if(proof){valid=withdrawal(x,p->agreement,*proof,c.tick(),c.revision(),{});if(!valid.ok)return {false,{},valid.error};}
    auto id=c.next_id("voluntary_path");if(!id.ok)return id;valid=cancel_succession_plan(c,f,actor,plan_id);if(!valid.ok)return {false,{},valid.error};
    VoluntaryPathCancellation end;end.plan_id=plan_id;end.actor_id=actor;end.reason=proof?"successor_withdrawal":"predecessor_cancel";end.tick=c.tick();end.revision=c.revision();end.withdrawal=proof;
    if(!state.cancellations.insert(id.value,std::move(end)))return {false,{},common_failure("DuplicateVoluntaryPathCancellation").error};
    valid=c.append_fact({id.value,voluntary_path_cancelled_kind,voluntary_path_module});if(!valid.ok)return {false,{},valid.error};
    valid=validate_voluntary_path_history(x,state);return valid.ok?id:Outcome<StableId>{false,{},valid.error};
}
}
