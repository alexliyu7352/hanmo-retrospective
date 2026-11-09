#include "resident/kernel/catalog_section_observation.hpp"
#include "provision_management.hpp"
#include "../local_economy_internal.hpp"
#include <map>
#include <set>
namespace hanmo::resident {
namespace {
using namespace local_economy_detail;
const HouseholdProvisionAuthorization* definition(const LocalEconomyCatalog& economy,const HouseholdProvisionManagementCatalog& catalog,const StableId& id){
    if(!economy.household_provisioning||!has(catalog.authorization_ids,id))return nullptr;
    return economy.household_provisioning->authorizations.find(id);
}
Status proof_matches(const CommonStore& common,const HouseholdProvisionControlRead& controls,const HouseholdProvisionControlProof& proof,const HouseholdProvisionAuthorization& source,const StableId& actor,Integer tick,Integer revision){
    if(proof.actor_id!=actor||proof.household_group_id!=source.household_group_id||proof.operation!=household_provision_operation||proof.source_revision!=revision||revision<0||revision>common.state().identity.revision)return failure("HouseholdProvisionControlMismatch",source.id);
    return controls.validate_accepted(proof,tick);
}
Outcome<StableId> append_configuration_fact(CommonStore& common,const StableId& kind){
    auto id=common.next_id("household_provision_configuration");if(!id.ok)return id;
    auto appended=common.append_fact({id.value,kind,"local_economy"});if(!appended.ok)return {false,{},appended.error};
    return id;
}
Status resolve_pending(CommonStore& common,HouseholdProvisionManagementState& state,const HouseholdProvisionConfigurationQuote& quote,const StableId& kind,HouseholdProvisionManagementEffects& effects){
    if(quote.pending_configuration_id.empty())return common_success();
    const auto* pending=state.configurations.find(quote.pending_configuration_id);if(!pending)return failure("InvalidHouseholdProvisionConfiguration");
    auto id=append_configuration_fact(common,"household_provision_configuration_"+kind);if(!id.ok)return {false,{},id.error};
    state.resolutions.insert(id.value,{id.value,pending->id,kind,quote.command.actor_id,quote.authority,quote.requested_tick,quote.requested_revision});
    if(pending->previous_configuration_id.empty())state.current_by_authorization.erase(pending->authorization_id);
    else state.current_by_authorization.set(pending->authorization_id,pending->previous_configuration_id);
    effects.fact_ids.push_back(id.value);return common_success();
}
}
Status validate_household_provision_management_catalog(const CommonStore& common,const LocalEconomyCatalog& economy,const HouseholdProvisionManagementCatalog& catalog){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 71, "HouseholdProvisionManagementCatalog", "validate_household_provision_management_catalog", &catalog);

    if(catalog.version!=1||!economy.enabled||!economy.household_provisioning||catalog.authorization_ids.empty()||catalog.authorization_ids.size()>65536)return common_failure("InvalidHouseholdProvisionManagementCatalog");
    auto original=validate_household_provision_catalog(common,economy,*economy.household_provisioning);if(!original.ok)return original;
    std::set<StableId> ids;
    for(const auto& id:catalog.authorization_ids)if(!ids.insert(id).second||!economy.household_provisioning->authorizations.find(id))return common_failure("UnknownManagedHouseholdProvision",id);
    return common_success();
}
Outcome<HouseholdProvisionConfigurationQuote> preview_configure_household_provision(const CommonStore& common,const LocalEconomyState& economy,const LocalEconomyCatalog& original,const HouseholdProvisionManagementCatalog& catalog,const HouseholdProvisionManagementState& state,const HouseholdProvisionControlRead& controls,const ConfigureHouseholdProvisionCommand& command){
    const auto fail=[](const StableId& code)->Outcome<HouseholdProvisionConfigurationQuote>{return {false,{},common_failure(code).error};};
    auto checked=validate_household_provision_management_catalog(common,original,catalog);if(!checked.ok)return {false,{},checked.error};
    if(state.version!=1||!economy.household_provisioning)return fail("HouseholdProvisionManagementUnavailable");
    const auto* source=definition(original,catalog,command.authorization_id);
    const auto* actor=common.actor(command.actor_id);const auto* cohort=source?economy.cohorts.find(source->cohort_id):nullptr;
    const auto* obligation=source?economy.obligations.find(source->obligation_id):nullptr;
    const auto* binding=source?economy.provision_bindings.find(source->provision_binding_id):nullptr;
    if(!source||!cohort||!obligation||!binding)return fail("UnknownManagedHouseholdProvision");
    if(!actor||!actor->alive||!actor->available)return fail("HouseholdProvisionManagerUnavailable");
    if(!actor->action_id.empty())return fail("HouseholdProvisionManagerBusy");
    const auto location=common.resolved_location(actor->id);const auto* here=location.ok?std::get_if<SettlementLocation>(&location.value):nullptr;
    if(!here||(here->place_id!=cohort->site_id&&here->settlement_id!=cohort->site_id))return fail("HouseholdProvisionManagerNotPresent");
    auto authority=controls.current(actor->id,source->household_group_id,household_provision_operation,common.tick());
    if(!authority.ok)return {false,{},authority.error};if(!authority.value)return fail("HouseholdProvisionControlDenied");
    checked=proof_matches(common,controls,*authority.value,*source,actor->id,common.tick(),common.state().identity.revision);if(!checked.ok)return {false,{},checked.error};
    checked=check_household_ration_authority(common,economy,original,*original.household_provisioning,*economy.household_provisioning,*obligation,obligation->next_due_tick);if(!checked.ok)return {false,{},checked.error};
    HouseholdProvisionConfigurationQuote quote;quote.command=command;quote.authority=*authority.value;quote.requested_tick=common.tick();quote.requested_revision=common.state().identity.revision;
    quote.effective_tick=obligation->next_due_tick;
    if(!obligation->active||quote.effective_tick<=common.tick()||!add(quote.effective_tick,obligation->period_ticks,quote.first_due_tick)||quote.first_due_tick>binding->through_tick)return fail("NoFutureHouseholdProvisionPeriod");
    if(const auto* head=state.current_by_authorization.find(source->id)){
        const auto* previous=state.configurations.find(*head);if(!previous||previous->authorization_id!=source->id)return fail("InvalidHouseholdProvisionConfiguration");
        if(previous->effective_tick>common.tick()){quote.pending_configuration_id=previous->id;quote.previous_configuration_id=previous->previous_configuration_id;}
        else quote.previous_configuration_id=previous->id;
    }
    if(command.kind==HouseholdProvisionConfigurationKind::cancel_pending){
        if(quote.pending_configuration_id.empty()||command.consumption_limit)return fail("NoPendingHouseholdProvisionConfiguration");
    }else if(command.kind!=HouseholdProvisionConfigurationKind::configure||(command.consumption_limit&&*command.consumption_limit<0))return fail("InvalidHouseholdProvisionConsumptionLimit");
    return {true,std::move(quote),{}};
}
Outcome<HouseholdProvisionManagementEffects> apply_configure_household_provision_in_candidate(CommonStore& common,const LocalEconomyState& economy,const LocalEconomyCatalog& original,const HouseholdProvisionManagementCatalog& catalog,HouseholdProvisionManagementState& state,const HouseholdProvisionControlRead& controls,const ConfigureHouseholdProvisionCommand& command){
    auto quote=preview_configure_household_provision(common,economy,original,catalog,state,controls,command);if(!quote.ok)return {false,{},quote.error};
    auto candidate=state;HouseholdProvisionManagementEffects effects;
    auto status=resolve_pending(common,candidate,quote.value,command.kind==HouseholdProvisionConfigurationKind::cancel_pending?"cancelled":"superseded",effects);if(!status.ok)return {false,{},status.error};
    if(command.kind==HouseholdProvisionConfigurationKind::configure){
        auto id=append_configuration_fact(common,"household_provision_configured");if(!id.ok)return {false,{},id.error};
        const auto& q=quote.value;HouseholdProvisionConfiguration record;
        record.id=id.value;record.authorization_id=command.authorization_id;record.actor_id=command.actor_id;record.previous_configuration_id=q.previous_configuration_id;record.authority=q.authority;
        record.requested_tick=q.requested_tick;record.requested_revision=q.requested_revision;record.effective_tick=q.effective_tick;record.first_due_tick=q.first_due_tick;record.consumption_limit=command.consumption_limit;
        candidate.configurations.insert(record.id,record);candidate.current_by_authorization.set(record.authorization_id,record.id);effects.fact_ids.push_back(record.id);
    }
    status=validate_household_provision_management(common,economy,original,catalog,candidate,controls);if(!status.ok)return {false,{},status.error};
    state=std::move(candidate);return {true,std::move(effects),{}};
}
Outcome<HouseholdProvisionManagementEffects> apply_configure_household_provision(CommonStore& common,const LocalEconomyState& economy,const LocalEconomyCatalog& original,const HouseholdProvisionManagementCatalog& catalog,HouseholdProvisionManagementState& state,const HouseholdProvisionControlRead& controls,const ConfigureHouseholdProvisionCommand& command){
    auto preview=preview_configure_household_provision(common,economy,original,catalog,state,controls,command);if(!preview.ok)return {false,{},preview.error};
    PhaseSavepoint savepoint(common);if(!savepoint.token().ok)return {false,{},savepoint.token().error};auto candidate=state;
    auto result=apply_configure_household_provision_in_candidate(common,economy,original,catalog,candidate,controls,command);if(!result.ok)return result;
    auto committed=savepoint.commit();if(!committed.ok)return {false,{},committed.error};state=std::move(candidate);return result;
}
Outcome<Integer> household_provision_consumption_limit(const LocalEconomyState& economy,const LocalEconomyCatalog& original,const HouseholdProvisionManagementContext& context,const RecurringObligation& obligation,Integer due,Integer requirement){
    if(requirement<0||context.catalog.version!=1||context.state.version!=1)return {false,0,common_failure("InvalidHouseholdProvisionManagementState").error};
    const auto* source=definition(original,context.catalog,obligation.authority_ref);
    if(!source)return {true,requirement,{}};
    const auto* binding=economy.provision_bindings.find(source->provision_binding_id);
    if(source->obligation_id!=obligation.id||!binding||due<binding->from_tick||due>binding->through_tick)return {false,0,common_failure("HouseholdProvisionPeriodMismatch",obligation.id).error};
    const auto* head=context.state.current_by_authorization.find(source->id);StableId id=head?*head:StableId{};
    std::size_t remaining=context.state.configurations.size();
    while(!id.empty()){
        if(!remaining--)return {false,0,common_failure("HouseholdProvisionConfigurationCycle").error};
        const auto* record=context.state.configurations.find(id);if(!record||record->authorization_id!=source->id)return {false,0,common_failure("InvalidHouseholdProvisionConfiguration",id).error};
        if(record->first_due_tick<=due)return {true,record->consumption_limit?std::min(requirement,*record->consumption_limit):requirement,{}};
        id=record->previous_configuration_id;
    }
    return {true,requirement,{}};
}
Status validate_household_provision_management(const CommonStore& common,const LocalEconomyState& economy,const LocalEconomyCatalog& original,const HouseholdProvisionManagementCatalog& catalog,const HouseholdProvisionManagementState& state,const HouseholdProvisionControlRead& controls){
    auto checked=validate_household_provision_management_catalog(common,original,catalog);if(!checked.ok)return checked;
    if(state.version!=1)return failure("InvalidHouseholdProvisionManagementState");
    const Integer now=std::max(common.tick(),economy.last_step_tick);
    std::map<StableId,StableId> current;
    std::map<StableId,const HouseholdProvisionConfigurationResolution*> replacements;
    std::set<StableId> earlier;std::size_t configurations=0,resolutions=0;
    for(const auto& fact:common.state().facts.rows()){
        if(fact.second.kind=="household_provision_configured"){
            const auto* record=state.configurations.find(fact.first);const auto* source=record?definition(original,catalog,record->authorization_id):nullptr;
            const auto* obligation=source?original.obligations.find(source->obligation_id):nullptr;const auto* binding=source?original.provision_bindings.find(source->provision_binding_id):nullptr;
            if(!record||record->id!=fact.first||fact.second.domain!="local_economy"||!source||!obligation||!binding||record->actor_id.empty()||record->requested_tick<0||record->requested_tick>now||record->effective_tick<=record->requested_tick||record->first_due_tick>binding->through_tick||(record->consumption_limit&&*record->consumption_limit<0)||!earlier.count(record->authority.fact_id))return failure("InvalidHouseholdProvisionConfiguration",fact.first);
            Integer cycles=record->requested_tick<obligation->next_due_tick?0:(record->requested_tick-obligation->next_due_tick)/obligation->period_ticks+1;
            Integer elapsed=0,effective=0,first_due=0;
            if(!multiply(cycles,obligation->period_ticks,elapsed)||!add(obligation->next_due_tick,elapsed,effective)||!add(effective,obligation->period_ticks,first_due)||record->effective_tick!=effective||record->first_due_tick!=first_due)return failure("HouseholdProvisionConfigurationPeriod",record->id);
            checked=proof_matches(common,controls,record->authority,*source,record->actor_id,record->requested_tick,record->requested_revision);if(!checked.ok)return checked;
            const auto found=current.find(source->id);const StableId previous=found==current.end()?StableId{}:found->second;
            if(record->previous_configuration_id!=previous)return failure("HouseholdProvisionConfigurationHistory",record->id);
            if(!previous.empty()){const auto* previous_record=state.configurations.find(previous);if(!previous_record||previous_record->effective_tick>record->requested_tick)return failure("UnresolvedHouseholdProvisionConfiguration",record->id);}
            if(const auto pending=replacements.find(source->id);pending!=replacements.end()){
                const auto& resolution=*pending->second;if(resolution.tick!=record->requested_tick||resolution.source_revision!=record->requested_revision||resolution.actor_id!=record->actor_id||resolution.authority.fact_id!=record->authority.fact_id)return failure("HouseholdProvisionReplacementMismatch",record->id);
                replacements.erase(pending);
            }
            current[source->id]=record->id;++configurations;
        }else if(fact.second.kind=="household_provision_configuration_cancelled"||fact.second.kind=="household_provision_configuration_superseded"){
            const auto* resolution=state.resolutions.find(fact.first);const auto* record=resolution?state.configurations.find(resolution->configuration_id):nullptr;const auto* source=record?definition(original,catalog,record->authorization_id):nullptr;
            if(!resolution||resolution->id!=fact.first||fact.second.domain!="local_economy"||!record||!source||!earlier.count(record->id)||!earlier.count(resolution->authority.fact_id)||resolution->kind!=(fact.second.kind=="household_provision_configuration_cancelled"?"cancelled":"superseded")||resolution->tick<record->requested_tick||resolution->tick>=record->effective_tick||resolution->tick>now||current[source->id]!=record->id)return failure("InvalidHouseholdProvisionResolution",fact.first);
            checked=proof_matches(common,controls,resolution->authority,*source,resolution->actor_id,resolution->tick,resolution->source_revision);if(!checked.ok)return checked;
            if(record->previous_configuration_id.empty())current.erase(source->id);else current[source->id]=record->previous_configuration_id;
            if(resolution->kind=="superseded"&&!replacements.emplace(source->id,resolution).second)return failure("UnresolvedHouseholdProvisionReplacement",record->id);
            ++resolutions;
        }
        earlier.insert(fact.first);
    }
    if(!replacements.empty()||configurations!=state.configurations.size()||resolutions!=state.resolutions.size()||current.size()!=state.current_by_authorization.size())return failure("HouseholdProvisionConfigurationRegistryMismatch");
    for(const auto& row:state.current_by_authorization.rows()){const auto found=current.find(row.first);if(found==current.end()||found->second!=row.second)return failure("HouseholdProvisionConfigurationIndexMismatch",row.first);}
    const HouseholdProvisionManagementContext context{catalog,state};
    for(const auto& row:economy.rations.rows()){
        const auto* obligation=economy.obligations.find(row.second.obligation_id);if(!obligation)return failure("InvalidHouseholdProvisionRation",row.first);
        if(!definition(original,catalog,obligation->authority_ref))continue;
        auto limit=household_provision_consumption_limit(economy,original,context,*obligation,row.second.due_tick,row.second.required);if(!limit.ok)return {false,{},limit.error};
        if(row.second.consumed>limit.value)return failure("HouseholdProvisionLimitExceeded",row.first);
    }
    return common_success();
}
}
