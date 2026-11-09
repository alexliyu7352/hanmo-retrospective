#include "voluntary_path_source.hpp"
#include "resident/domains/interaction/face_to_face_contact.hpp"
#include <charconv>
#include <limits>
#include <set>
#include <tuple>
namespace hanmo::resident {namespace {
bool reference(const StableId& id){return !id.empty()&&id.size()<=256;}
Integer serial(const CommonStore& c,const StableId& id,const StableId& prefix){
    const auto lead=prefix+":";if(id.rfind(lead,0)!=0)return -1;
    const auto digits=id.substr(lead.size());Integer n=0;auto p=std::from_chars(digits.data(),digits.data()+digits.size(),n);
    return p.ec==std::errc{}&&p.ptr==digits.data()+digits.size()&&n>0&&n<c.state().next_instance_id&&std::to_string(n)==digits?n:-1;
}
bool identity(const WorldIdentity& a,const WorldIdentity& b){return std::tie(a.world_id,a.execution_epoch,a.revision,a.tick)==std::tie(b.world_id,b.execution_epoch,b.revision,b.tick);}
bool context(const InteractionContext& a,const InteractionContext& b){return std::tie(a.place_id,a.provider_actor_id,a.role_id,a.kind,a.encounter_id,a.input_error)==std::tie(b.place_id,b.provider_actor_id,b.role_id,b.kind,b.encounter_id,b.input_error);}
bool origin(const KnownPersonalActorOrigin& a,const KnownPersonalActorOrigin& b){return std::tie(a.kind,a.actor_id,a.birth_fact_id,a.creation_fact_id,a.prototype_id)==std::tie(b.kind,b.actor_id,b.birth_fact_id,b.creation_fact_id,b.prototype_id);}
bool proof_equal(const KnownPersonalDialogueProof& a,const KnownPersonalDialogueProof& b){
    const auto& x=a.declaration.meeting;const auto& y=b.declaration.meeting;
    return a.opening_fact_id==b.opening_fact_id&&a.template_id==b.template_id&&a.declaration.speaker_actor_id==b.declaration.speaker_actor_id&&
        std::tie(x.actor_id,x.provider_actor_id,x.session_id,x.dialogue_id,x.node_id,x.node_kind)==std::tie(y.actor_id,y.provider_actor_id,y.session_id,y.dialogue_id,y.node_id,y.node_kind)&&
        identity(x.source_identity,y.source_identity)&&context(x.context,y.context)&&origin(a.listener_origin,b.listener_origin)&&origin(a.provider_origin,b.provider_origin);
}
bool selected(const KnownPersonalDialogueSelector& selector,const KnownPersonalActorOrigin& o,const StableId& actor){
    if(o.actor_id!=actor)return false;
    if(selector.kind=="installed_actor")return o.kind=="installed_actor"&&selector.source_id==actor&&o.birth_fact_id.empty()&&o.creation_fact_id.empty()&&o.prototype_id.empty();
    return selector.kind=="born_prototype"&&o.kind=="recorded_birth"&&selector.source_id==o.prototype_id&&reference(o.birth_fact_id)&&o.creation_fact_id==actor;
}
BiologicalControlOrigin biological(const BiologicalChildOrigin& r){
    return {r.kind,r.parent_actor_id,r.child_actor_id,r.original_catalog_actor_id,r.birth_fact_id,r.creation_fact_id,r.source_available_tick,{}};
}
bool relation_matches(const VoluntaryPathSource& s,const SuccessionConsentSource& a,const VoluntaryRelationSource& r){
    if(s.relation_kind!=r.kind||s.relation_established_tick!=r.established_tick||s.disciple_skill_id!=r.skill_id||
       a.relation_fact_id!=r.relation_fact_id||a.predecessor_birth_tick!=r.predecessor_birth_tick||a.successor_birth_tick!=r.successor_birth_tick||
       bool(s.biological)!=bool(r.biological))return false;
    return !s.biological||biological_control_origin_equal(*s.biological,biological(*r.biological));
}
bool selection_matches(const SuccessionPersonalSelection& selected,const SuccessionPersonalTerms& terms){
    if(selected.money!=terms.money)return false;OrderedTable<Integer> totals;
    for(const auto& lot:terms.lots){auto* previous=totals.find(lot.item_id);
        if(lot.quantity<=0||(previous&&*previous>std::numeric_limits<Integer>::max()-lot.quantity))return false;
        totals.set(lot.item_id,(previous?*previous:0)+lot.quantity);}
    if(totals.size()!=selected.goods.size())return false;
    std::set<StableId> seen;for(const auto& item:selected.goods){const auto* amount=totals.find(item.item_id);
        if(!amount||*amount!=item.quantity||!seen.insert(item.item_id).second)return false;}
    return true;
}
Status bound(const VoluntaryPathContext& x,const SuccessionConsentSource& a,const VoluntaryPathSource& s,const StableId& before){
    auto valid=validate_voluntary_path_source_shape(s);if(!valid.ok)return valid;
    valid=validate_voluntary_path_catalog(x.catalog,x.original_requests,x.interactions,x.templates,x.other_declarations);
    if(!valid.ok)return valid;const auto& c=x.people.common;
    if(x.interactions.common.get()!=&x.people.catalog||!x.meetings)return common_failure("VoluntaryPathCandidateRequired");
    const auto* d=x.catalog.definitions.find(a.request_id);
    if(!d||a.terms_version!=d->terms_version||s.relation_kind!=d->relation_kind||s.disciple_skill_id!=d->disciple_skill_id||
       a.world_id!=c.state().identity.world_id||std::all_of(a.world_id.begin(),a.world_id.end(),[](auto b){return b==0;})||
       a.scenario_id!=c.state().metadata.scenario_id||a.accepted_tick<0||a.accepted_tick>c.tick()||a.accepted_revision<0||a.accepted_revision>c.revision())
        return common_failure("VoluntaryPathSourceMismatch",a.request_id);
    valid=validate_succession_personal_terms(a.personal);if(!valid.ok)return valid;
    if(!selection_matches(d->personal,a.personal))return common_failure("VoluntaryPathSelectedAssetsMismatch",a.request_id);
    const auto& first=a.personal.predecessor_actor_id;const auto& second=a.personal.successor_actor_id;
    const auto* predecessor=c.state().actors.find(first);const auto* successor=c.state().actors.find(second);
    if(first==second||!predecessor||!successor||a.personal.source_wallet_id!=predecessor->wallet_id||
       a.personal.source_inventory_id!=predecessor->inventory_id||a.personal.destination_wallet_id!=successor->wallet_id||
       a.personal.destination_inventory_id!=successor->inventory_id)return common_failure("VoluntaryPathContainerSourceMismatch");
    const Integer limit=before.empty()?c.state().next_instance_id:serial(c,before,"family");
    for(const auto& lot:a.personal.lots){const auto n=serial(c,lot.lot_id,"lot");
        if(n<1||n>=limit||!x.people.catalog.items.find(lot.item_id))return common_failure("VoluntaryPathOriginalLotRequired",lot.lot_id);}
    const auto& p=s.statement;const auto& m=p.declaration.meeting;
    auto t=x.templates(d->template_id);if(!t.ok)return {false,{},t.error};
    if(p.template_id!=d->template_id||p.declaration.speaker_actor_id!=second||m.actor_id!=first||m.provider_actor_id!=second||
       !reference(m.session_id)||m.dialogue_id!=t.value.dialogue_id||m.node_id!=d->accept_node_id||m.node_kind!="say"||
       m.source_identity.world_id!=a.world_id||m.source_identity.execution_epoch<1||m.source_identity.tick!=a.accepted_tick||m.source_identity.revision!=a.accepted_revision||
       !face_to_face_contact_context_valid(m.context)||m.context.kind!=std::optional<StableId>{"narrative"}||!m.context.role_id.empty()||
       m.context.provider_actor_id!=second||!selected(t.value.listener,p.listener_origin,first)||!selected(t.value.provider,p.provider_origin,second)||
       a.session_id!=m.session_id||a.dialogue_id!=m.dialogue_id||a.node_id!=m.node_id||a.node_kind!=m.node_kind||!context(a.context,m.context))
        return common_failure("VoluntaryPathProviderAcceptanceRequired");
    const auto opening=serial(c,p.opening_fact_id,known_personal_dialogue_opened_kind);const auto* header=c.state().facts.find(p.opening_fact_id);
    if(opening<1||!header||header->id!=p.opening_fact_id||header->kind!=known_personal_dialogue_opened_kind||header->domain!=known_personal_dialogue_module)
        return common_failure("VoluntaryPathOpeningRequired");
    if(!before.empty()){const auto& ids=c.state().facts.order();const auto i=std::find(ids.begin(),ids.end(),p.opening_fact_id),j=std::find(ids.begin(),ids.end(),before);
        if(limit<1||opening>=limit||i==ids.end()||j==ids.end()||i>=j)return common_failure("VoluntaryPathOpeningBoundaryMismatch");}
    auto relation=before.empty()?read_current_voluntary_relation(x.people,d->relation_kind,first,second,a.relation_fact_id):
        read_voluntary_relation_before_fact(x.people,d->relation_kind,first,second,a.relation_fact_id,a.accepted_tick,before);
    if(!relation.ok)return {false,{},relation.error};
    return relation_matches(s,a,relation.value)?common_success():common_failure("VoluntaryPathRelationSourceMismatch");
}
}
Outcome<VoluntaryPathConsentQuote> preview_voluntary_path_consent(const VoluntaryPathContext& x,const StableId& first,
    const StableId& second,const StableId& request,const StableId& relation_id){
    auto valid=validate_voluntary_path_catalog(x.catalog,x.original_requests,x.interactions,x.templates,x.other_declarations);
    if(!valid.ok)return {false,{},valid.error};const auto& c=x.people.common;const auto* d=x.catalog.definitions.find(request);
    if(!d||!x.meetings||first!=c.state().metadata.player_actor_id)return {false,{},common_failure("VoluntaryPathCurrentPlayerRequired").error};
    auto meeting=x.meetings->known_personal_dialogue(first,second,d->template_id);if(!meeting.ok)return {false,{},meeting.error};
    if(!identity(meeting.value.declaration.meeting.source_identity,c.state().identity))return {false,{},common_failure("StaleVoluntaryPathMeeting").error};
    auto relation=read_current_voluntary_relation(x.people,d->relation_kind,first,second,relation_id);if(!relation.ok)return {false,{},relation.error};
    auto personal=preview_succession_personal_assets(c,first,second,d->personal);if(!personal.ok)return {false,{},personal.error};
    VoluntaryPathConsentQuote out;out.personal=std::move(personal.value);const auto& m=meeting.value.declaration.meeting;
    auto& a=out.agreement;a.request_id=request;a.relation_fact_id=relation.value.relation_fact_id;a.dialogue_id=m.dialogue_id;
    a.node_id=m.node_id;a.node_kind=m.node_kind;a.session_id=m.session_id;a.context=m.context;a.world_id=c.state().identity.world_id;
    a.scenario_id=c.state().metadata.scenario_id;a.terms_version=d->terms_version;a.accepted_tick=c.tick();a.accepted_revision=c.revision();
    a.predecessor_birth_tick=relation.value.predecessor_birth_tick;a.successor_birth_tick=relation.value.successor_birth_tick;a.personal=out.personal.terms;
    out.path.relation_kind=relation.value.kind;out.path.relation_established_tick=relation.value.established_tick;
    out.path.disciple_skill_id=relation.value.skill_id;if(relation.value.biological)out.path.biological=biological(*relation.value.biological);
    out.path.statement=std::move(meeting.value);valid=bound(x,a,out.path,{});
    return valid.ok?Outcome<VoluntaryPathConsentQuote>{true,std::move(out),{}}:Outcome<VoluntaryPathConsentQuote>{false,{},valid.error};
}
Status revalidate_voluntary_path_consent(const VoluntaryPathContext& x,const VoluntaryPathConsentQuote& old){
    auto fresh=preview_voluntary_path_consent(x,old.agreement.personal.predecessor_actor_id,old.agreement.personal.successor_actor_id,
        old.agreement.request_id,old.agreement.relation_fact_id);if(!fresh.ok)return {false,{},fresh.error};
    auto recipe=succession_personal_asset_recipe(x.people.common,old.personal);if(!recipe.ok)return {false,{},recipe.error};
    if(!succession_personal_terms_equal(old.personal.terms,old.agreement.personal)||
       !succession_consent_source_equal(old.agreement,fresh.value.agreement)||!voluntary_path_source_equal(old.path,fresh.value.path))
        return common_failure("StaleVoluntaryPathConsent");return common_success();
}
Status validate_voluntary_path_consent_before_fact(const VoluntaryPathContext& x,const SuccessionConsentSource& a,
    const VoluntaryPathSource& s,const StableId& fact){
    const auto& c=x.people.common;const auto* head=c.state().facts.find(fact);const auto* value=x.people.family.facts.find(fact);
    const auto* original=value?std::get_if<SuccessionConsentFact>(value):nullptr;
    if(serial(c,fact,"family")<1||!head||head->id!=fact||head->kind!="family_succession_consent"||head->domain!="family"||
       !original||original->kind!=head->kind||!succession_consent_source_equal(original->agreement,a))
        return common_failure("VoluntaryPathOriginalConsentFactRequired",fact);
    auto valid=bound(x,a,s,fact);if(!valid.ok)return valid;
    if(!x.player_before)return common_failure("VoluntaryPathPlayerHistoryRequired");
    auto player=x.player_before(a.accepted_tick,fact);if(!player.ok)return {false,{},player.error};
    if(player.value!=a.personal.predecessor_actor_id)return common_failure("VoluntaryPathHistoricalPlayerMismatch");
    return x.meetings->known_personal_dialogue_before_fact(s.statement,a.accepted_tick,fact);
}
bool voluntary_path_source_equal(const VoluntaryPathSource& a,const VoluntaryPathSource& b){
    return std::tie(a.relation_kind,a.relation_established_tick,a.disciple_skill_id)==std::tie(b.relation_kind,b.relation_established_tick,b.disciple_skill_id)&&
        bool(a.biological)==bool(b.biological)&&(!a.biological||biological_control_origin_equal(*a.biological,*b.biological))&&proof_equal(a.statement,b.statement);
}
}
