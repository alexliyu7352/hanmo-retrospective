#include "resident/domains/military/military_model.hpp"
#include "resident/domains/organization/office_authority.hpp"
// 待批准政策必须由真实接任Office持同一专项Grant签发，旧源与原合同不改派。
#include "politics_supply_authority_detail.hpp"
#include <map>
namespace hanmo::resident::supply_authority_detail {
Outcome<Owner> asset_owner_before(const PoliticalTakeoverContext& x,const StableId& id,bool inventory,Integer tick,Integer sequence,const StableId& boundary){
    const auto fail=[&](){return Outcome<Owner>{false,{},common_failure("PoliticalSupplyAssetHistoryMismatch",id).error};};
    const auto* authored=inventory?x.common_catalog.required_inventories.find(id):x.common_catalog.required_wallets.find(id);
    const auto* wallet=inventory?nullptr:x.common.wallet(id);const auto* store=inventory?x.common.inventory(id):nullptr;
    if(!authored||(!wallet&&!store)||authored->owner.kind!=Owner::Kind::organization)return fail();
    Owner result=authored->owner,current=result;Integer previous=0,previous_tick=-1;StableId previous_fact;
    std::map<Integer,const MilitaryOccupationFact*> ordered;
    auto transfer=[&](const MilitaryOccupationFact& f)->std::optional<std::pair<Owner,Owner>>{if(!f.asset_manifest)return {};if(inventory){for(const auto& row:f.asset_manifest->inventories)if(row.first==id)return std::make_pair(row.second.from_owner,row.second.to_owner);}else for(const auto& row:f.asset_manifest->wallets)if(row.first==id)return std::make_pair(row.second.from_owner,row.second.to_owner);return {};};
    for(const auto& f:x.military.occupation_facts)if(transfer(f)){auto number=organization_instance_sequence(f.id,"military");if(!number||*number>=x.common.state().next_instance_id||!ordered.emplace(*number,&f).second)return fail();}
    for(const auto& row:ordered){const auto& f=*row.second;const auto moved=*transfer(f);const auto* header=x.common.state().facts.find(f.id);
        if(!f.assets_applied.value_or(false)||!header||header->id!=f.id||header->kind!="military_occupation"||header->domain!="military"||row.first<=previous||f.tick<previous_tick||(!previous_fact.empty()&&!before(x.common,previous_fact,f.id))||(!boundary.empty()&&((row.first<sequence)!=before(x.common,f.id,boundary)))||!owner_equal(moved.first,current))return fail();
        if((row.first<sequence&&f.tick>tick)||(row.first>=sequence&&f.tick<tick))return fail();
        current=moved.second;previous=row.first;previous_tick=f.tick;previous_fact=f.id;if(row.first<sequence)result=current;
    }
    if(!owner_equal(current,inventory?store->owner:wallet->owner))return fail();return {true,result,{}};
}
Status control_before(const PoliticalTakeoverContext& x,const PoliticalTakeoverRegistration& r,const PoliticalTakeoverRule& rule,Integer tick,Integer sequence,const StableId& boundary){
    const MilitaryOccupationFact* latest=nullptr;Integer selected=0;
    for(const auto& f:x.military.occupation_facts)if(f.garrison_id==rule.garrison_id&&f.assets_applied.value_or(false)){
        auto number=organization_instance_sequence(f.id,"military");if(!number||*number>=x.common.state().next_instance_id)return common_failure("PoliticalSupplyControlHistoryMismatch",f.id);
        if(!boundary.empty()&&((*number<sequence)!=before(x.common,f.id,boundary)))return common_failure("PoliticalSupplyControlHistoryMismatch",f.id);
        if((*number<sequence&&f.tick>tick)||(*number>=sequence&&f.tick<tick))return common_failure("PoliticalSupplyControlHistoryMismatch",f.id);
        if(*number<sequence&&*number>selected){latest=&f;selected=*number;}
    }
    if(!latest||latest->id!=r.occupation_fact_id||latest->to_organization_id!=r.successor_organization_id)return common_failure("PoliticalSupplyControlChanged",r.id);
    return common_success();
}
}
namespace hanmo::resident {
namespace {
Outcome<PoliticalSupplyPolicyAuthoritySource> read(const PoliticalTakeoverContext& x,bool enabled,const ActingAuthority& acting,const PoliticalSupplyPolicyMandate& m,const std::optional<StableId>& grant_id,const std::optional<StableId>& boundary,Integer tick){
    using namespace takeover_detail;using namespace supply_authority_detail;
    const auto fail=[](const StableId& code,const StableId& id=StableId{})->Outcome<PoliticalSupplyPolicyAuthoritySource>{return {false,{},common_failure(code,id).error};};
    if(!enabled||!x.catalog.enabled)return fail("PoliticalSupplyPoliciesNotEnabled");
    if((x.phase&&!boundary)||tick<0||tick>x.common.tick()||(!boundary&&tick!=x.common.tick()))return fail("PoliticalSupplyIssueRequiresCommittedClock");
    auto valid=validate_political_takeover_state(x);if(!valid.ok)return {false,{},valid.error};
    const auto* registration=x.state.registrations.find(m.registration_fact_id);const auto* rule=registration?x.catalog.rules.find(registration->rule_id):nullptr;
    if(!registration||!rule||m.original_source_key.empty()||m.organization_id!=registration->successor_organization_id||m.site_id!=rule->site_id||m.inventory_id.empty()||m.funding_wallet_id.empty()||m.reward_wallet_id.empty()||m.maximum_contract_commitment<0||acting.organization_id!=m.organization_id||acting.office_id!=rule->successor_office_id)return fail("PoliticalSupplyMandateMismatch");
    const auto source=std::find_if(registration->responsibilities.begin(),registration->responsibilities.end(),[&](const auto& v){return v.responsibility.kind==PoliticalResponsibilityKind::supply_source&&v.responsibility.responsibility_id==m.original_source_key;});
    if(source==registration->responsibilities.end()||!has(registration->public_inventory_ids,m.inventory_id)||std::none_of(source->containers.begin(),source->containers.end(),[&](const auto& c){return c.inventory&&c.selected&&c.id==m.inventory_id;}))return fail("PoliticalSupplySourceInventoryMismatch",m.original_source_key);
    Integer sequence=x.common.state().next_instance_id;
    if(boundary){const auto* fact=x.common.state().facts.find(*boundary);auto number=organization_instance_sequence(*boundary,"economic_supply_policy");if(!fact||fact->id!=*boundary||fact->kind!="economic_supply_policy_issued"||fact->domain!="economic_supply_policies"||!number||*number>=sequence)return fail("PoliticalSupplyIssueBoundaryMismatch",*boundary);sequence=*number;}
    const auto registered=organization_instance_sequence(registration->id,"political_takeover_pending");if(!registered||*registered>=sequence||registration->tick>tick||(boundary&&!before(x.common,registration->id,*boundary)))return fail("PoliticalSupplyRegistrationTimeMismatch",m.registration_fact_id);
    auto ownership=x.phase?validate_military_ownership(x.common,x.military,x.military_catalog,x.military_access,*x.phase):validate_military_ownership(x.common,x.military,x.military_catalog,x.military_access);if(!ownership.ok)return {false,{},ownership.error};
    auto control=control_before(x,*registration,*rule,tick,sequence,boundary.value_or(StableId{}));if(!control.ok)return {false,{},control.error};
    const auto* office=x.organization_catalog.offices.find(acting.office_id);if(!office)return fail("PoliticalSupplyAuthorityDenied",acting.office_id);
    const Grant* selected=nullptr;
    if(boundary){
        if(!grant_id)return fail("PoliticalSupplyGrantRequired");const auto* grant=x.organization_catalog.grants.find(*grant_id);
        if(!grant||!has(office->grant_ids,*grant_id)||!covers(*grant,*rule,m,tick))return fail("PoliticalSupplyGrantMismatch",*grant_id);
        auto signer=validate_historical_office_signer(x.common,x.organizations,x.organization_catalog,acting,tick,sequence);if(!signer.ok)return {false,{},signer.error};selected=grant;
        for(const auto& id:grant->wallet_ids){auto owner=asset_owner_before(x,id,false,tick,sequence,*boundary);if(!owner.ok)return {false,{},owner.error};if(!owner_is(owner.value,m.organization_id))return fail("PoliticalSupplyHistoricalAssetDenied",id);}
        for(const auto& id:grant->inventory_ids){auto owner=asset_owner_before(x,id,true,tick,sequence,*boundary);if(!owner.ok)return {false,{},owner.error};if(!owner_is(owner.value,m.organization_id))return fail("PoliticalSupplyHistoricalAssetDenied",id);}
    }else{
        const auto* actor=x.common.actor(acting.actor_id);auto resolved=x.common.resolved_location(acting.actor_id);const auto* location=resolved.ok?std::get_if<SettlementLocation>(&resolved.value):nullptr;
        if(!actor||!actor->alive||!actor->available||!actor->action_id.empty()||!location||location->place_id!=m.site_id)return fail("PoliticalSupplyRequiresLocalAuthority",m.site_id);
        const auto* current=x.organizations.offices.find(acting.office_id);if(!current)return fail("PoliticalSupplyAuthorityDenied",acting.office_id);
        for (const auto &id : current->grant_ids) {
          const auto *grant = x.organization_catalog.grants.find(id);
          if (!grant || !has(office->grant_ids, id) ||
              !covers(*grant, *rule, m, tick))
            continue;
          auto authority = check_authority_with_single_grant(
              x.common, x.organizations, x.organization_catalog, acting,
              political_supply_policy_grant, {ScopeRef::Kind::site, m.site_id},
              m.maximum_contract_commitment, *grant, nullptr);
          if (authority.ok) {
            selected = grant;
            break;
          }
        }
        if(!selected)return fail("PoliticalSupplyAuthorityDenied",acting.office_id);
    }
    PoliticalSupplyPolicyAuthoritySource out;out.source_identity=x.common.state().identity;out.mandate=m;out.authority=acting;out.grant_id=selected->id;out.registration=registration_source(x.common,*registration,*rule);out.issued_tick=tick;return {true,std::move(out),{}};
}
}
Outcome<PoliticalSupplyPolicyAuthoritySource> PoliticalSupplyPolicyAccess::political_supply_policy_authority(const ActingAuthority& acting,const PoliticalSupplyPolicyMandate& m)const{return read(context_,enabled_,acting,m,{},{},context_.common.tick());}
Outcome<PoliticalSupplyPolicyAuthoritySource> PoliticalSupplyPolicyAccess::political_supply_policy_authority_before_issue(const ActingAuthority& acting,const PoliticalSupplyPolicyMandate& m,const StableId& grant,const StableId& fact,Integer tick)const{return read(context_,enabled_,acting,m,grant,fact,tick);}
}
