#include "resident/domains/military/military_model.hpp"
// 继任者重新签发下一半日起的新政策；这里没有征收、余数转让或旧欠债改派。
#include "politics_occupation_fiscal_detail.hpp"
#include "resident/domains/politics/politics_internal.hpp"
namespace hanmo::resident {
Status require_legacy_fiscal_before_occupation(const PoliticalTakeoverContext& x,const StableId& jurisdiction){
    auto valid=validate_political_takeover_state(x);if(!valid.ok)return valid;
    if(!x.politics_catalog.jurisdictions.find(jurisdiction))return common_failure("UnknownPoliticalJurisdiction",jurisdiction);
    for(const auto& row:x.state.registrations.rows()){const auto* rule=x.catalog.rules.find(row.second.rule_id);if(rule&&rule->jurisdiction_id==jurisdiction)return common_failure("OccupationFiscalPolicyRequired",jurisdiction);}
    return common_success();
}
Status validate_occupation_fiscal(const OccupationFiscalContext& x){
    using namespace occupation_fiscal_detail;const auto& t=x.takeover;
    if(x.catalog.version!=1||x.state.version!=1)return common_failure("UnsupportedOccupationFiscalVersion");
    if(!x.catalog.enabled&&!x.state.policies.empty())return common_failure("UnexpectedOccupationFiscalState");
    if(x.catalog.enabled&&!t.catalog.enabled)return common_failure("OccupationFiscalTakeoverRequired");
    auto valid=validate_political_takeover_state(t);if(!valid.ok)return valid;
    // 新显式财政启用后，任何发生过接管的辖区不能从旧header另开第二条新税政。
    if(x.catalog.enabled)for(const auto& row:x.legacy.policies.rows()){
        const auto& policy=row.second;auto sequence=organization_instance_sequence(policy.id,"politics_fiscal");if(!sequence)return common_failure("InvalidFiscalPolicySource",row.first);
        for(const auto& taken:t.state.registrations.rows()){const auto* rule=t.catalog.rules.find(taken.second.rule_id);if(!rule||rule->jurisdiction_id!=policy.snapshot.jurisdiction_id)continue;auto occupation_sequence=organization_instance_sequence(taken.second.occupation_fact_id,"military");if(occupation_sequence&&*sequence>=*occupation_sequence)return common_failure("LegacyFiscalPolicyAfterOccupation",policy.id);}
    }
    std::map<StableId,Integer> revisions,sequences,ticks;
    for(const auto& row:x.state.policies.rows()){
        const auto& r=row.second;const auto& p=r.policy;const auto* registration=t.state.registrations.find(r.registration_fact_id);const auto* rule=registration?t.catalog.rules.find(registration->rule_id):nullptr;const auto* jurisdiction=t.politics_catalog.jurisdictions.find(p.jurisdiction_id);const auto* fact=t.common.state().facts.find(r.id);auto sequence=organization_instance_sequence(r.id,political_occupation_fiscal_id_prefix);
        const auto fail=[&](){return common_failure("InvalidOccupationFiscalPolicy",row.first);};
        if(row.first!=r.id||!sequence||*sequence>=t.common.state().next_instance_id||!registration||!rule||!jurisdiction||!fact||fact->id!=r.id||fact->kind!=political_occupation_fiscal_fact_kind||fact->domain!="politics"||p.source_fact_id!=r.id||p.issued_tick<registration->tick||p.issued_tick>t.common.tick()||p.issued_tick==INT64_MAX||p.effective_from_tick!=p.issued_tick+1||!valid_mode(p.mode))return fail();
        if(p.jurisdiction_id!=rule->jurisdiction_id||p.site_id!=rule->site_id||p.site_id!=jurisdiction->seat_place_id||p.organization_id!=registration->successor_organization_id||r.authority.organization_id!=p.organization_id||r.authority.office_id!=rule->successor_office_id||p.public_wallet_id!=jurisdiction->public_wallet_id||!takeover_detail::has(jurisdiction->public_inventory_ids,p.public_inventory_id)||!takeover_detail::has(registration->public_wallet_ids,p.public_wallet_id)||!takeover_detail::has(registration->public_inventory_ids,p.public_inventory_id))return fail();
        auto registration_sequence=organization_instance_sequence(registration->id,"political_takeover_pending");if(!registration_sequence||*registration_sequence>=*sequence||last_before(t,p.jurisdiction_id,*sequence)!=registration)return common_failure("OccupationFiscalIssuedWithoutControl",r.id);
        for(const auto& later:t.state.registrations.rows()){const auto* later_rule=t.catalog.rules.find(later.second.rule_id);if(!later_rule||later_rule->jurisdiction_id!=p.jurisdiction_id)continue;auto later_sequence=organization_instance_sequence(later.second.occupation_fact_id,"military");if(later_sequence&&*later_sequence>*sequence&&later.second.tick<p.issued_tick)return common_failure("OccupationFiscalControlTimeMismatch",r.id);}
        const auto* office=t.organization_catalog.offices.find(r.authority.office_id);const auto* grant=t.organization_catalog.grants.find(r.grant_id);if(!office||!grant||!takeover_detail::has(office->grant_ids,r.grant_id)||!grant_covers(*grant,*rule,*registration,p,p.issued_tick))return common_failure("OccupationFiscalGrantMismatch",r.id);
        auto signer=validate_historical_office_signer(t.common,t.organizations,t.organization_catalog,r.authority,p.issued_tick,*sequence);if(!signer.ok)return signer;
        if(p.policy_revision!=revisions[p.jurisdiction_id]+1||*sequence<=sequences[p.jurisdiction_id]||p.issued_tick<ticks[p.jurisdiction_id])return common_failure("OccupationFiscalHistoryMismatch",r.id);
        revisions[p.jurisdiction_id]=p.policy_revision;sequences[p.jurisdiction_id]=*sequence;ticks[p.jurisdiction_id]=p.issued_tick;
    }
    for(const auto& row:t.common.state().facts.rows())if(row.second.domain=="politics"&&row.second.kind==political_occupation_fiscal_fact_kind&&!x.state.policies.find(row.first))return common_failure("MissingOccupationFiscalPolicyPayload",row.first);
    return common_success();
}
Status preview_political_set_occupation_tax(const OccupationFiscalContext& x,const ActivityRead& activity,const StableId& actor,const PoliticalSetOccupationTaxCommand& command){
    using namespace occupation_fiscal_detail;const auto& t=x.takeover;
    if(!x.catalog.enabled)return common_failure("OccupationFiscalNotEnabled",{},ErrorKind::unsupported_capability);
    if(t.phase)return common_failure("OccupationFiscalCommandRequiresCommittedClock");
    auto valid=validate_occupation_fiscal(x);if(!valid.ok)return valid;
    if(actor!=command.authority.actor_id)return common_failure("PoliticalCommandActorMismatch");
    if(!valid_mode(command.mode))return common_failure("InvalidFiscalTaxMode");
    const auto* jurisdiction=x.politics.jurisdictions.find(command.jurisdiction_id);const auto* registration=t.state.registrations.find(command.registration_fact_id);const auto* rule=registration?t.catalog.rules.find(registration->rule_id):nullptr;
    if(!jurisdiction||!registration||!rule||rule->jurisdiction_id!=jurisdiction->id)return common_failure("UnknownOccupationFiscalJurisdiction",command.jurisdiction_id);
    if(jurisdiction->revision!=command.expected_jurisdiction_revision)return common_failure("StalePoliticalJurisdiction",jurisdiction->id);
    if(command.expected_policy_revision!=revision(x.state,jurisdiction->id))return common_failure("StaleOccupationFiscalPolicy",jurisdiction->id);
    if(command.expected_policy_revision==INT64_MAX||t.common.tick()==INT64_MAX)return common_failure("FiscalPolicyCounterOverflow",jurisdiction->id);
    if(command.authority.organization_id!=registration->successor_organization_id||command.authority.office_id!=rule->successor_office_id||jurisdiction->controlling_organization_id!=registration->successor_organization_id)return common_failure("PoliticalControlDenied",jurisdiction->id);
    auto control=military_garrison_owner(t.common,t.military,t.military_catalog,t.military_access,rule->garrison_id);if(!control.ok)return {false,{},control.error};if(control.value.occupation_id!=registration->occupation_fact_id||control.value.organization_id!=registration->successor_organization_id)return common_failure("PoliticalTakeoverControlChanged",registration->id);
    auto available=politics_detail::available(t.common,activity,actor);if(!available.ok)return available;auto age=growth_independent_office_age(t.common,actor);if(!age.ok)return age;
    const auto location=politics_detail::place(t.common,actor);if(!location||location->place_id!=rule->site_id)return common_failure("FiscalPolicyRequiresLocalAuthority",rule->site_id);
    if(!takeover_detail::has(jurisdiction->public_inventory_ids,command.public_inventory_id)||!takeover_detail::has(registration->public_wallet_ids,jurisdiction->public_wallet_id)||!takeover_detail::has(registration->public_inventory_ids,command.public_inventory_id))return common_failure("FiscalPublicInventoryMismatch",command.public_inventory_id);
    const auto* wallet=t.common.wallet(jurisdiction->public_wallet_id);const auto* inventory=t.common.inventory(command.public_inventory_id);if(!wallet||!inventory||!takeover_detail::owner_is(wallet->owner,registration->successor_organization_id)||!takeover_detail::owner_is(inventory->owner,registration->successor_organization_id))return common_failure("FiscalPublicAssetMismatch",jurisdiction->id);
    auto authorized=check_authority(t.common,t.organizations,t.organization_catalog,command.authority,political_occupation_fiscal_command_kind,{ScopeRef::Kind::site,rule->site_id},0);if(!authorized.ok)return {false,{},authorized.error};
    if(!current_grant(x,command,*registration,*rule,*jurisdiction))return common_failure("OccupationFiscalGrantIncomplete",jurisdiction->id);
    return common_success();
}
Outcome<PoliticalFiscalEffects> apply_political_set_occupation_tax_in_transaction(CommonStore& common,OccupationFiscalState& state,const OccupationFiscalContext& x,const ActivityRead& activity,const StableId& actor,const PoliticalSetOccupationTaxCommand& command){
    if(&common!=&x.takeover.common||&state!=&x.state)return {false,{},common_failure("OccupationFiscalCandidateMismatch").error};
    auto valid=preview_political_set_occupation_tax(x,activity,actor,command);if(!valid.ok)return {false,{},valid.error};
    const auto& registration=*x.takeover.state.registrations.find(command.registration_fact_id);const auto& rule=*x.takeover.catalog.rules.find(registration.rule_id);const auto& jurisdiction=*x.politics.jurisdictions.find(command.jurisdiction_id);const auto* grant=occupation_fiscal_detail::current_grant(x,command,registration,rule,jurisdiction);
    auto id=common.next_id(political_occupation_fiscal_id_prefix);if(!id.ok)return {false,{},id.error};
    OccupationFiscalPolicyRecord record;record.id=id.value;record.registration_fact_id=registration.id;record.grant_id=grant->id;record.authority=command.authority;record.policy={jurisdiction.id,rule.site_id,registration.successor_organization_id,jurisdiction.public_wallet_id,command.public_inventory_id,id.value,command.expected_policy_revision+1,common.tick(),common.tick()+1,command.mode};
    auto appended=common.append_fact({id.value,political_occupation_fiscal_fact_kind,"politics"});if(!appended.ok)return {false,{},appended.error};
    if(!state.policies.insert(id.value,std::move(record)))return {false,{},common_failure("DuplicateOccupationFiscalPolicy",id.value).error};
    return {true,{id.value,command.expected_policy_revision+1,common.tick()+1},{}};
}
}
