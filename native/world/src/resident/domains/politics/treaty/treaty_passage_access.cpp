#include "resident/domains/military/military_model.hpp"
// 通行只在原条款期限、原授方实际控制和真实成员资格同时成立时派生，不变更任何资产。
#include "treaty_passage_access.hpp"
#include "treaty_passage_detail.hpp"
#include "resident/domains/politics/takeover/politics_takeover_detail.hpp"
#include "resident/domains/organization/named_membership_rules.hpp"
namespace hanmo::resident {
Outcome<TreatyPassageControlSource> TreatyPassageTerritoryAccess::treaty_control_now(const StableId& id)const{
    const auto* initial=catalog_.jurisdictions.find(id);const auto* current=state_.jurisdictions.find(id);if(!initial||!current||!catalog_.enabled)return {false,{},common_failure("UnknownTreatyPassageJurisdiction",id).error};
    TreatyPassageControlSource result{common_.state().identity,{id,initial->controlling_organization_id,{},initial->revision}};
    if(takeover_){if(&takeover_->common!=&common_||&takeover_->politics_catalog!=&catalog_||takeover_->phase)return {false,{},common_failure("TreatyPassageCandidateMismatch").error};auto read=PoliticalTakeoverTerritoryAccess(*takeover_).jurisdiction_control(id);if(!read.ok)return {false,{},read.error};if(read.value)result.control={id,read.value->controlling_organization_id,read.value->occupation_fact_id,read.value->revision};}
    if(current->controlling_organization_id!=result.control.organization_id||current->revision!=result.control.revision||current->political_organization_id!=initial->political_organization_id)return {false,{},common_failure("TreatyPassageControlSourceMismatch",id).error};
    return {true,std::move(result),{}};
}
Outcome<TreatyPassageControlSource> TreatyPassageTerritoryAccess::treaty_control_before_signature(const StableId& id,const StableId& boundary,Integer tick)const{
    auto current=treaty_control_now(id);if(!current.ok)return current;const auto* head=common_.state().facts.find(boundary);const auto sequence=organization_instance_sequence(boundary,"treaty_passage_signature");
    if(!head||head->id!=boundary||head->domain!=treaty_passage_domain||head->kind!=treaty_passage_signature_kind||!sequence||*sequence>=common_.state().next_instance_id||tick<0||tick>common_.tick())return {false,{},common_failure("InvalidTreatyPassageControlBoundary",boundary).error};
    const auto* initial=catalog_.jurisdictions.find(id);TreatyPassageControlSource result{common_.state().identity,{id,initial->controlling_organization_id,{},initial->revision}};if(!takeover_||!takeover_->catalog.enabled)return {true,std::move(result),{}};
    const auto& order=common_.state().facts.order();const auto at=std::find(order.begin(),order.end(),boundary);if(at==order.end())return {false,{},common_failure("InvalidTreatyPassageControlBoundary",boundary).error};
    Integer latest=0;
    for(const auto& row:takeover_->state.registrations.rows()){
        const auto* rule=takeover_->catalog.rules.find(row.second.rule_id);if(!rule||rule->jurisdiction_id!=id)continue;const auto& registration=row.second;
        const auto number=organization_instance_sequence(row.first,"political_takeover_pending");const auto occupation=organization_instance_sequence(registration.occupation_fact_id,"military");const auto registration_at=std::find(order.begin(),order.end(),row.first),occupation_at=std::find(order.begin(),order.end(),registration.occupation_fact_id);
        if(!number||!occupation||registration_at==order.end()||occupation_at==order.end()||((*number<*sequence)!=(registration_at<at))||((*occupation<*sequence)!=(occupation_at<at))||((*occupation<*sequence)!=(*number<*sequence))||(*number<*sequence?registration.tick>tick:registration.tick<tick))return {false,{},common_failure("TreatyPassageControlCausalityMismatch",boundary).error};
        if(*number<*sequence){if(result.control.revision==INT64_MAX)return {false,{},common_failure("TreatyPassageControlRevisionOverflow").error};++result.control.revision;if(*number>latest){latest=*number;result.control.organization_id=registration.successor_organization_id;result.control.occupation_fact_id=registration.occupation_fact_id;}}
    }
    return {true,std::move(result),{}};
}
Outcome<bool> TreatyPassageAccess::treaty_personal_passage(const StableId& actor,const StableId& place,const StableId& terms_id)const{
    const auto& c=context_;const auto& d=c.diplomacy;if(!c.catalog.enabled)return {true,false,{}};const auto* terms=d.catalog.passage_terms.find(terms_id);if(!terms||!c.catalog.provisions.find(terms_id)||std::find(terms->place_ids.begin(),terms->place_ids.end(),place)==terms->place_ids.end())return {true,false,{}};
    const auto* organization=d.organizations.organizations.find(terms->beneficiary_organization_id);if(!organization||organization->status!=OrganizationStatus::active)return {true,false,{}};
    const auto affiliated=[&](const StableId& person){if(const auto* member=active_membership_episode(d.organizations,person,terms->beneficiary_organization_id))return member->response!=MemberResponse::departed;bool departed=false;for(const auto& row:d.organizations.members.rows())if(row.second.actor_id==person&&row.second.organization_id==terms->beneficiary_organization_id){if(row.second.response==MemberResponse::departed)departed=true;else return true;}if(departed)return false;for(const auto& row:d.organizations.offices.rows())if(row.second.organization_id==terms->beneficiary_organization_id&&row.second.status==OfficeStatus::active&&row.second.holder_actor_id==std::optional<StableId>{person})return true;return false;};
    const auto* members=d.common.members(actor);if(!members||members->empty())return {true,false,{}};
    for(const auto& person:*members){const auto* p=d.common.actor(person);const auto* army=military_.active_army_members(person);const auto* leader=military_.army_leader(person);if(!p||!p->alive||!p->available||!affiliated(person)||(army&&!army->empty())||(leader&&!leader->empty())||std::holds_alternative<InArmyLocation>(p->location))return {true,false,{}};}
    auto audited=treaty_passage_detail::audit(c);if(!audited.ok)return {false,false,audited.error};
    bool effective=false;for(const auto& pair:audited.value.signatures)if(pair.second.size()==2&&pair.second.back()->terms_id==terms_id){const auto tick=pair.second.back()->tick;if(d.common.tick()>=tick&&d.common.tick()<tick+terms->duration_ticks){effective=true;break;}}
    if(!effective)return {true,false,{}};
    auto controls=treaty_passage_detail::current_controls(c,*terms);if(!controls.ok){if(controls.error.original.code=="TreatyPassageGrantorControlLost")return {true,false,{}};return {false,false,controls.error};}
    return {true,true,{}};
}
}
