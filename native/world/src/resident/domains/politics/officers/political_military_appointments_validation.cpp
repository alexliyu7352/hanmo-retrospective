#include "resident/kernel/catalog_section_observation.hpp"
// 冷验核过去真实军籍和Office，不因今日撤权抹历史，也不因今天同名指挥者忽略中间变更。
#include "political_military_appointments_detail.hpp"
#include <algorithm>
#include <limits>
#include <set>
namespace hanmo::resident::political_military_detail {
bool same_identity(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
bool same_target(const MilitaryOfficerAssignmentTarget& a,const MilitaryUnitCommandObservation& b){return a.unit_id==b.unit_id&&a.organization_id==b.organization_id&&a.garrison_id==b.garrison_id&&b.army_id.empty()&&a.previous_actor_id==b.commander_actor_id&&a.previous_source.kind==b.command_source.kind&&a.previous_source.id==b.command_source.id;}
bool grant_covers(const Grant& g,const PoliticalMilitaryAppointmentPolicy& p,Integer tick){return g.amount_limit>=1&&tick>=g.valid_from&&(!g.valid_until||tick<=*g.valid_until)&&g.wallet_ids.empty()&&g.inventory_ids.empty()&&std::find(g.allowed_command_kinds.begin(),g.allowed_command_kinds.end(),political_officer_grant_command)!=g.allowed_command_kinds.end()&&std::any_of(g.scopes.begin(),g.scopes.end(),[&](const auto& s){return s.kind==ScopeRef::Kind::site&&s.id==p.site_id;});}
Outcome<StableId> current_authority(const PoliticalMilitaryAppointmentContext& c,const ActingAuthority& acting,const PoliticalMilitaryAppointmentPolicy& p){
    if(acting.organization_id!=p.organization_id)return {false,{},common_failure("PoliticalMilitaryOrganizationMismatch").error};
    auto proof=check_authority(c.common,c.organizations,c.organization_catalog,acting,political_officer_grant_command,{ScopeRef::Kind::site,p.site_id},1);if(!proof.ok)return {false,{},proof.error};
    const auto* office=c.organizations.offices.find(acting.office_id);if(!office)return {false,{},common_failure("PoliticalMilitaryOfficeRequired").error};
    for(const auto& id:office->grant_ids){const auto* grant=c.organization_catalog.grants.find(id);if(grant&&grant->office_id==office->id&&grant_covers(*grant,p,c.common.tick()))return {true,id,{}};}
    return {false,{},common_failure("PoliticalMilitarySpecificGrantRequired").error};
}
Status historical_authority(const PoliticalMilitaryAppointmentContext& c,const ActingAuthority& acting,const StableId& grant_id,const PoliticalMilitaryAppointmentPolicy& p,Integer tick,Integer sequence){
    const auto* office=c.organization_catalog.offices.find(acting.office_id);const auto* grant=c.organization_catalog.grants.find(grant_id);
    if(!office||acting.organization_id!=p.organization_id||office->organization_id!=acting.organization_id||!grant||grant->office_id!=office->id||std::find(office->grant_ids.begin(),office->grant_ids.end(),grant_id)==office->grant_ids.end()||!grant_covers(*grant,p,tick))return common_failure("InvalidPoliticalMilitaryGrant",grant_id);
    return validate_historical_office_signer(c.common,c.organizations,c.organization_catalog,acting,tick,sequence);
}
Status current_source(const PoliticalMilitaryAppointmentContext& c,const MilitaryOfficerAssignmentTarget& target){
    const auto source_kind=static_cast<Integer>(target.previous_source.kind);if(source_kind<0||source_kind>(c.field_commands?3:2))return common_failure("PoliticalMilitaryCommandSourceVersionRequired",target.unit_id);
    if(!c.units.current)return common_failure("PoliticalMilitarySourceRequired");auto source=c.units.current(target.unit_id);if(!source.ok)return {false,{},source.error};
    return same_identity(source.value.source_identity,c.common.state().identity)&&same_target(target,source.value)?common_success():common_failure("PoliticalMilitaryCommandSourceChanged",target.unit_id);
}
Status historical_source(const PoliticalMilitaryAppointmentContext& c,const MilitaryOfficerAssignmentTarget& target,Integer tick,const StableId& fact){
    const auto source_kind=static_cast<Integer>(target.previous_source.kind);if(source_kind<0||source_kind>(c.field_commands?3:2))return common_failure("PoliticalMilitaryCommandSourceVersionRequired",target.unit_id);
    if(!c.units.before_fact)return common_failure("PoliticalMilitaryHistoryRequired");auto source=c.units.before_fact(target.unit_id,tick,fact);if(!source.ok)return {false,{},source.error};
    return same_identity(source.value.source_identity,c.common.state().identity)&&same_target(target,source.value)?common_success():common_failure("InvalidPoliticalMilitaryCommandHistory",fact);
}
Outcome<AppointmentAudit> audit(const PoliticalMilitaryAppointmentContext& c){
    const auto fail=[](const char* code,const StableId& id=StableId{})->Outcome<AppointmentAudit>{return {false,{},common_failure(code,id).error};};
    if(c.state.version!=1||c.catalog.version!=1)return fail("UnsupportedPoliticalMilitaryAppointmentVersion");
    auto directory=validate_political_military_appointment_catalog(c.common_catalog,c.organization_catalog,c.catalog,c.units);if(!directory.ok)return {false,{},directory.error};
    AppointmentAudit out;std::set<StableId> seen_authorizations,seen_answers,seen_withdrawals;Integer prior_sequence=0,prior_tick=0;
    for(const auto& id:c.common.state().facts.order()){
        const auto* head=c.common.state().facts.find(id);if(!head||head->id!=id)return fail("InvalidPoliticalMilitaryFactOrder",id);
        const bool issue=head->kind==political_officer_issue_kind,answer=head->kind==political_officer_accept_kind||head->kind==political_officer_decline_kind,withdraw=head->kind==political_officer_withdraw_kind;
        if(!issue&&!answer&&!withdraw)continue;if(!c.catalog.enabled)return fail("PoliticalMilitaryAppointmentProfileRequired",id);
        const auto seq=organization_instance_sequence(id,issue?"political_military_authorization":answer?"political_military_answer":"political_military_withdrawal");
        if(head->domain!="politics"||!seq||*seq<=prior_sequence||*seq>=c.common.state().next_instance_id)return fail("InvalidPoliticalMilitaryFactOrder",id);
        Integer tick=0;
        if(issue){
            const auto* v=c.state.authorizations.find(id);if(!v||v->id!=id||c.state.answers.find(id)||c.state.withdrawals.find(id))return fail("MissingPoliticalMilitaryAuthorization",id);
            const auto* policy=c.catalog.policies.find(v->policy_id);const auto* next=c.common.state().actors.find(v->target.next_actor_id);
            if(!policy||v->target.organization_id!=policy->organization_id||v->target.garrison_id!=policy->garrison_id||v->target.unit_id.empty()||v->target.previous_source.id.empty()||v->target.previous_actor_id==v->target.next_actor_id||!c.common.actor(v->target.previous_actor_id)||!next||!next->birth_tick||v->tick<0||*next->birth_tick>v->tick-18*720||v->tick>std::numeric_limits<Integer>::max()-policy->response_ticks||v->answer_before_tick!=v->tick+policy->response_ticks||!c.common_catalog.places.find(v->meeting_place_id))return fail("InvalidPoliticalMilitaryAuthorization",id);
            auto authority=historical_authority(c,v->authority,v->grant_id,*policy,v->tick,*seq);if(!authority.ok)return {false,{},authority.error};auto source=historical_source(c,v->target,v->tick,id);if(!source.ok)return {false,{},source.error};
            // 同时占用同部或同一受任人的两份未失效命令，热门不能产生，冷读也拒。
            for(const auto& row:out.authorizations){const auto& old=*row.second;if(old.target.unit_id!=v->target.unit_id&&old.target.next_actor_id!=v->target.next_actor_id)continue;if(out.withdrawals.count(old.id))continue;auto answer=out.answers.find(old.id);if(answer!=out.answers.end()&&!answer->second->accepted)continue;auto deadline=answer==out.answers.end()?old.answer_before_tick:answer->second->apply_before_tick;if(v->tick>=deadline)continue;auto prior_authority=historical_authority(c,old.authority,old.grant_id,*c.catalog.policies.find(old.policy_id),v->tick,*seq);auto prior_source=historical_source(c,old.target,v->tick,id);if(prior_authority.ok&&prior_source.ok)return fail("DuplicatePoliticalMilitaryPendingOrder",id);}
            tick=v->tick;out.authorizations.emplace(id,v);seen_authorizations.insert(id);
        }else if(answer){
            const auto* v=c.state.answers.find(id);if(!v||v->id!=id||c.state.authorizations.find(id)||c.state.withdrawals.find(id))return fail("MissingPoliticalMilitaryAnswer",id);
            const auto auth=out.authorizations.find(v->authorization_fact_id);if(auth==out.authorizations.end()||out.answers.count(v->authorization_fact_id)||out.withdrawals.count(v->authorization_fact_id))return fail("InvalidPoliticalMilitaryAnswerSource",id);
            const auto& issued=*auth->second;const auto* policy=c.catalog.policies.find(issued.policy_id);
            if(v->witness_actor_ids.empty()||v->witness_actor_ids.front()!=v->actor_id||v->witness_actor_ids.size()>2||(v->witness_actor_ids.size()==2&&(v->witness_actor_ids.back()!=issued.authority.actor_id||v->witness_actor_ids.back()==v->actor_id))||v->actor_id!=issued.target.next_actor_id||v->tick<issued.tick||v->tick>=issued.answer_before_tick||v->accepted!=(head->kind==political_officer_accept_kind)||v->tick>std::numeric_limits<Integer>::max()-policy->arrival_ticks||v->apply_before_tick!=(v->accepted?v->tick+policy->arrival_ticks:0))return fail("InvalidPoliticalMilitaryAnswer",id);
            if(v->accepted){auto authority=historical_authority(c,issued.authority,issued.grant_id,*policy,v->tick,*seq);if(!authority.ok)return {false,{},authority.error};auto source=historical_source(c,issued.target,v->tick,id);if(!source.ok)return {false,{},source.error};}
            tick=v->tick;out.answers.emplace(v->authorization_fact_id,v);seen_answers.insert(id);
        }else{
            const auto* v=c.state.withdrawals.find(id);if(!v||v->id!=id||c.state.authorizations.find(id)||c.state.answers.find(id))return fail("MissingPoliticalMilitaryWithdrawal",id);
            const auto auth=out.authorizations.find(v->authorization_fact_id);if(auth==out.authorizations.end()||out.withdrawals.count(v->authorization_fact_id))return fail("InvalidPoliticalMilitaryWithdrawalSource",id);
            const auto& issued=*auth->second;const auto* policy=c.catalog.policies.find(issued.policy_id);const auto answer_it=out.answers.find(issued.id);if(v->witness_actor_ids.empty()||v->witness_actor_ids.front()!=v->authority.actor_id||v->witness_actor_ids.size()>2||(v->witness_actor_ids.size()==2&&(v->witness_actor_ids.back()!=issued.target.next_actor_id||v->witness_actor_ids.back()==v->authority.actor_id))||v->tick<issued.tick||v->authority.office_id!=issued.authority.office_id||v->authority.actor_id!=issued.authority.actor_id||(answer_it!=out.answers.end()&&!answer_it->second->accepted))return fail("InvalidPoliticalMilitaryWithdrawal",id);
            auto authority=historical_authority(c,v->authority,v->grant_id,*policy,v->tick,*seq);if(!authority.ok)return {false,{},authority.error};auto source=historical_source(c,issued.target,v->tick,id);if(!source.ok)return {false,{},source.error};tick=v->tick;out.withdrawals.emplace(issued.id,v);seen_withdrawals.insert(id);
        }
        if(tick<prior_tick||tick>c.common.tick())return fail("InvalidPoliticalMilitaryFactOrder",id);prior_sequence=*seq;prior_tick=tick;
    }
    if(seen_authorizations.size()!=c.state.authorizations.size()||seen_answers.size()!=c.state.answers.size()||seen_withdrawals.size()!=c.state.withdrawals.size())return fail("OrphanPoliticalMilitaryAppointmentPayload");
    for(const auto& row:c.state.authorizations.rows())if(row.first!=row.second.id||!seen_authorizations.count(row.first))return fail("OrphanPoliticalMilitaryAppointmentPayload",row.first);
    for(const auto& row:c.state.answers.rows())if(row.first!=row.second.id||!seen_answers.count(row.first))return fail("OrphanPoliticalMilitaryAppointmentPayload",row.first);
    for(const auto& row:c.state.withdrawals.rows())if(row.first!=row.second.id||!seen_withdrawals.count(row.first))return fail("OrphanPoliticalMilitaryAppointmentPayload",row.first);
    return {true,std::move(out),{}};
}
}
namespace hanmo::resident {
Status validate_political_military_appointment_catalog(const CommonCatalog& common,const OrganizationCatalog& organizations,const PoliticalMilitaryAppointmentCatalog& catalog,const PoliticalMilitaryAppointmentUnitRead& units){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 104, "PoliticalMilitaryAppointmentCatalog", "validate_political_military_appointment_catalog", &catalog);

    if(catalog.version!=1)return common_failure("UnsupportedPoliticalMilitaryAppointmentVersion");if(!catalog.enabled)return catalog.policies.empty()?common_success():common_failure("UnexpectedPoliticalMilitaryAppointmentCatalog");
    if(!organizations.enabled||!units.garrison_site||catalog.policies.empty())return common_failure("PoliticalMilitaryAppointmentDependenciesRequired");
    for(const auto& row:catalog.policies.rows()){const auto& p=row.second;auto site=units.garrison_site(p.garrison_id);if(row.first!=p.id||!organizations.organizations.find(p.organization_id)||!common.places.find(p.site_id)||site!=std::optional<StableId>{p.site_id}||p.response_ticks<=0||p.arrival_ticks<=0)return common_failure("InvalidPoliticalMilitaryAppointmentPolicy",row.first);}
    return common_success();
}
Status validate_political_military_appointments(const PoliticalMilitaryAppointmentContext& c){auto r=political_military_detail::audit(c);return r.ok?common_success():Status{false,{},r.error};}
}
