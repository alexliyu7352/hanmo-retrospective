#include "resident/domains/military/military_model.hpp"
#include "resident/domains/organization/office_authority.hpp"
// 裁定写出真实批准和逐责任事实；外层完整候选负责一并回滚军事、资产与本账。
#include "politics_takeover_detail.hpp"
#include "politics_takeover_time2.hpp"
#include "politics_takeover_production.hpp"
#include "resident/domains/politics/supply/politics_supply_adoptions.hpp"
namespace hanmo::resident {
Outcome<StableId> preview_political_responsibility_decisions(const PoliticalTakeoverContext& x,const StableId& actor,const PoliticalTakeoverDecisionCommand& command){
    using namespace takeover_detail;
    if(!x.catalog.enabled||(x.catalog.version!=1&&x.catalog.version!=2)||x.state.version!=x.catalog.version)return {false,{},common_failure("PoliticalTakeoverNotEnabled").error};
    if(!same_identity(command.expected_identity,x.common.state().identity))return {false,{},common_failure("PoliticalTakeoverRevisionMismatch").error};
    const auto state_valid=validate_political_takeover_state(x);if(!state_valid.ok)return {false,{},state_valid.error};
    const auto* r=x.state.registrations.find(command.registration_fact_id);const auto* rule=r?x.catalog.rules.find(r->rule_id):nullptr;
    if(!r||!rule||!known_to_actor(x,actor,*r))return {false,{},common_failure("PoliticalTakeoverNotKnown").error};
    auto original=military_source(x,*r);if(!original.ok)return {false,{},original.error};
    auto control=military_garrison_owner(x.common,x.military,x.military_catalog,x.military_access,rule->garrison_id);
    if(!control.ok)return {false,{},control.error};if(control.value.occupation_id!=r->occupation_fact_id||control.value.organization_id!=r->successor_organization_id)return {false,{},common_failure("PoliticalTakeoverControlChanged").error};
    const auto* last=latest(x.state,r->id);const auto previous=last?std::optional<StableId>{last->id}:std::nullopt;
    if(previous!=command.expected_previous_approval_fact_id)return {false,{},common_failure("PoliticalTakeoverDecisionChanged").error};
    if(actor!=command.authority.actor_id||command.authority.organization_id!=r->successor_organization_id||command.authority.office_id!=rule->successor_office_id)return {false,{},common_failure("PoliticalTakeoverAuthorityDenied").error};
    const auto* person=x.common.actor(actor);auto location=x.common.resolved_location(actor);const auto* place=location.ok?std::get_if<SettlementLocation>(&location.value):nullptr;
    if(!person||!person->alive||!person->available||!person->action_id.empty()||!place||place->place_id!=rule->site_id)return {false,{},common_failure("PoliticalTakeoverActorUnavailable").error};
    if(command.decisions.size()!=r->responsibilities.size())return {false,{},common_failure("PoliticalResponsibilityCoverageMismatch").error};
    std::set<decltype(key(PoliticalResponsibilityRef{}))> seen;
    for(const auto& request:command.decisions){if(!seen.insert(key(request.responsibility)).second)return {false,{},common_failure("DuplicatePoliticalResponsibility").error};const auto found=std::find_if(r->responsibilities.begin(),r->responsibilities.end(),[&](const auto& o){return key(o.responsibility)==key(request.responsibility);});if(found==r->responsibilities.end())return {false,{},common_failure("UnknownPoliticalResponsibility").error};auto derived=derive_political_supply_decision(x,*r,*found,request);if(!derived.ok)return {false,{},derived.error};}
    if(x.catalog.version==2){auto authority=read_current_political_takeover_authority(x,command.authority,*r,*rule);if(!authority.ok)return {false,{},authority.error};return {true,authority.value.effective_grant.id,{}};}
    const auto* office=x.organizations.offices.find(rule->successor_office_id);if(!office)return {false,{},common_failure("UnknownOffice",rule->successor_office_id).error};
    for(const auto& grant_id:office->grant_ids){const auto* grant=x.organization_catalog.grants.find(grant_id);if(!grant||!grant_covers(*grant,*rule,*r,x.common.tick()))continue;

      auto authority = check_authority_with_single_grant(
          x.common, x.organizations, x.organization_catalog, command.authority,
          political_takeover_command_kind,
          {ScopeRef::Kind::site, rule->site_id}, 0, *grant, nullptr);
      if (authority.ok)
        return {true, grant_id, {}};
    }
    return {false,{},common_failure("PoliticalTakeoverAuthorityDenied").error};
}
Outcome<StableId> decide_political_responsibilities_in_transaction(CommonStore& c,PoliticalTakeoverState& s,const PoliticalTakeoverContext& x,const StableId& actor,const PoliticalTakeoverDecisionCommand& command,PoliticalSupplyAdoptionState* supply_adoptions,OrganizationRuntime* organization_runtime){
    if(supply_adoptions!=x.supply_adoptions)return {false,{},common_failure("PoliticalSupplyCandidateMismatch").error};
    if(&c!=&x.common||&s!=&x.state)return {false,{},common_failure("PoliticalTakeoverCandidateMismatch").error};
    auto quote=preview_political_responsibility_decisions(x,actor,command);if(!quote.ok)return quote;
    const auto& registration=*s.registrations.find(command.registration_fact_id);
    std::optional<SingleGrantAuthorityRead> captured;std::optional<WorldMoment> approved_at;
    if(s.version==2){if(!organization_runtime||organization_runtime!=x.organization_runtime)return {false,{},common_failure("PoliticalTakeoverAuthorityCandidateMismatch").error};const auto& rule=*x.catalog.rules.find(registration.rule_id);auto authority=read_current_political_takeover_authority(x,command.authority,registration,rule);if(!authority.ok)return {false,{},authority.error};if(authority.value.effective_grant.id!=quote.value)return {false,{},common_failure("PoliticalTakeoverAuthorityChanged").error};captured=std::move(authority.value);auto now=read_organization_authority_now(c,x.actual_clock);if(!now.ok)return {false,{},now.error};approved_at=std::move(now.value);}
    std::optional<PreparedProductionResponsibilityChange> production_before;
    if(s.version==2){auto prepared=prepare_political_production_change(c,registration.responsibilities,*approved_at,x.production);if(!prepared.ok)return {false,{},prepared.error};production_before=std::move(prepared.value);}
    auto id=c.next_id("political_takeover");if(!id.ok)return id;
    PoliticalTakeoverApprovalRecord approval;approval.id=id.value;approval.registration_fact_id=registration.id;approval.previous_approval_fact_id=command.expected_previous_approval_fact_id;approval.authority=command.authority;approval.grant_id=quote.value;approval.tick=approved_at?approved_at->half_day_tick:c.tick();approval.approved_at=std::move(approved_at);
    auto fact=c.append_fact({id.value,political_takeover_fact_kind,"politics"});if(!fact.ok)return {false,{},fact.error};
    for(const auto& original:registration.responsibilities){const auto request=std::find_if(command.decisions.begin(),command.decisions.end(),[&](const auto& r){return takeover_detail::key(r.responsibility)==takeover_detail::key(original.responsibility);});auto decision=derive_political_supply_decision(x,registration,original,*request);if(!decision.ok)return {false,{},decision.error};auto key=c.next_id("political_responsibility");if(!key.ok)return key;
        decision.value.decision_fact_id=key.value;decision.value.takeover_fact_id=id.value;fact=c.append_fact({key.value,political_takeover_decision_fact_kind,"politics"});if(!fact.ok)return {false,{},fact.error};
        if(!s.decisions.insert(key.value,{key.value,id.value,std::move(decision.value)}))return {false,{},common_failure("DuplicatePoliticalDecision",key.value).error};approval.decision_fact_ids.push_back(key.value);
        if(request->supply_policy_fact_id){if(!supply_adoptions||!supply_adoptions->bindings.insert(key.value,{key.value,*request->supply_policy_fact_id}))return {false,{},common_failure("DuplicatePoliticalSupplyAdoption",key.value).error};}
    }
    if(!s.approvals.insert(id.value,std::move(approval)))return {false,{},common_failure("DuplicatePoliticalTakeover",id.value).error};
    if(captured){NativePoliticalTakeoverAuthorityRead sources(x);const auto& rule=*x.catalog.rules.find(registration.rule_id);auto saved=register_authority_basis_for_fact(c,organization_runtime->authority_bases,x.organizations,x.organization_catalog,x.organization_catalog.authority_catalog_identity,political_takeover_authority_request(command.authority,registration,rule),*captured,{"politics",political_takeover_fact_kind,id.value},sources);if(!saved.ok)return {false,{},saved.error};}
    if(s.version==2){auto adopted=apply_political_production_change_in_candidate(c,s,std::move(production_before),{{"politics",political_takeover_fact_kind,id.value},2},x.production);if(!adopted.ok)return {false,{},adopted.error};}
    return id;
}
}
