#include "resident/checkpoint/checkpoint_economic_index_copy.hpp"
#include "provision_management_view.hpp"
#include "../local_economy_internal.hpp"
#include <algorithm>
#include <map>
namespace hanmo::resident {
namespace {
using V=BoundaryValue;
bool same_identity(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
V identity_value(const WorldIdentity& value){
    StableId hex;const char* digits="0123456789abcdef";for(auto byte:value.world_id){hex+=digits[byte>>4];hex+=digits[byte&15];}
    return boundary::obj({{"world_id",V{hex}},{"execution_epoch",V{value.execution_epoch}},{"revision",V{value.revision}},{"tick",V{value.tick}}});
}
V cursor_value(const LocalEconomyCursor& value){return boundary::obj({{"identity",identity_value(value.identity)},{"observer_actor_id",V{value.observer_actor_id}},{"place_id",V{value.place_id}},{"authority",V{}},{"offset",V{value.offset}}});}
HouseholdProvisionPlanView plan(const HouseholdProvisionConfiguration& value){return {value.id,value.effective_tick,value.first_due_tick,value.consumption_limit};}
V plan_value(const std::optional<HouseholdProvisionPlanView>& value){if(!value)return {};return boundary::obj({{"id",V{value->id}},{"effective_tick",V{value->effective_tick}},{"first_due_tick",V{value->first_due_tick}},{"consumption_limit",value->consumption_limit?V{*value->consumption_limit}:V{}}});}
}
Outcome<HouseholdProvisionViewIndex> build_household_provision_view_index(const CommonStore& common,const LocalEconomyState& economy,const LocalEconomyCatalog& original,const HouseholdProvisionManagementCatalog& catalog,const HouseholdProvisionManagementState& state,const HouseholdProvisionControlRead& controls){
    HouseholdProvisionViewIndex index;index.identity=common.state().identity;index.observer_actor_id=index_memory::copy(index.text_memory,common.state().metadata.player_actor_id);
    auto checked=validate_household_provision_management_catalog(common,original,catalog);if(!checked.ok)return {false,{},checked.error};
    const auto* actor=common.actor(index.observer_actor_id);const auto location=common.resolved_location(index.observer_actor_id);
    const auto* here=location.ok?std::get_if<SettlementLocation>(&location.value):nullptr;
    if(!actor||!actor->alive||!here)return {true,std::move(index),{}};index.place_id=index_memory::copy(index.text_memory,here->place_id);
    index_memory::BorrowedIdMap by_obligation;
    for(const auto& id:catalog.authorization_ids){
        const auto* source=original.household_provisioning->authorizations.find(id);const auto* cohort=source?economy.cohorts.find(source->cohort_id):nullptr;
        if(!source||!cohort)return {false,{},common_failure("InvalidManagedHouseholdProvision",id).error};
        if(cohort->site_id!=here->place_id&&cohort->site_id!=here->settlement_id)continue;
        auto proof=controls.current(actor->id,source->household_group_id,household_provision_operation,common.tick());if(!proof.ok)return {false,{},proof.error};
        if(!proof.value)continue;
        if(proof.value->actor_id!=actor->id||proof.value->household_group_id!=source->household_group_id||proof.value->operation!=household_provision_operation||proof.value->source_revision!=common.state().identity.revision)return {false,{},common_failure("HouseholdProvisionControlMismatch",id).error};
        checked=controls.validate_accepted(*proof.value,common.tick());if(!checked.ok)return {false,{},checked.error};
        const auto* inventory=common.inventory(source->inventory_id);
        if(!inventory||inventory->owner.kind!=Owner::Kind::household||inventory->owner.id!=source->household_group_id)return {false,{},common_failure("HouseholdProvisionOwnerMismatch",id).error};
        index.authorization_ids.push_back(index_memory::copy(index.text_memory,id));by_obligation.emplace(&source->obligation_id,&id);
        HouseholdProvisionAvailability availability;CheckpointMemoryCharge command_memory;ConfigureHouseholdProvisionCommand command{index_memory::copy(command_memory,actor->id),index_memory::copy(command_memory,id),HouseholdProvisionConfigurationKind::configure,std::nullopt};
        auto ready=preview_configure_household_provision(common,economy,original,catalog,state,controls,command);availability.may_configure=ready.ok;if(!ready.ok)availability.configure_code=index_memory::copy(index.text_memory,ready.error.original.code);
        command.kind=HouseholdProvisionConfigurationKind::cancel_pending;ready=preview_configure_household_provision(common,economy,original,catalog,state,controls,command);availability.may_cancel=ready.ok;if(!ready.ok)availability.cancel_code=index_memory::copy(index.text_memory,ready.error.original.code);
        index.availability.emplace(index_memory::copy(index.text_memory,id),std::move(availability));
    }
    // This scan is only at the final identity rebuild, never in a page query.
    for(const auto& row:economy.rations.rows()){
        const auto found=by_obligation.find(&row.second.obligation_id);if(found==by_obligation.end())continue;
        const auto* source=original.household_provisioning->authorizations.find(*found->second);const auto& receipt=row.second;
        if(receipt.inventory_id!=source->inventory_id||receipt.item_id!=source->item_id||receipt.beneficiary.kind!=SubjectKind::cohort||receipt.beneficiary.id!=source->cohort_id||receipt.tick>common.tick())continue;
        auto& current=index_memory::entry(index.text_memory,index.latest_ration_by_authorization,*found->second);const auto* previous=economy.rations.find(current);
        if(!previous||std::tie(receipt.due_tick,receipt.tick,receipt.id)>std::tie(previous->due_tick,previous->tick,previous->id))index_memory::replace(index.text_memory,current,receipt.id);
    }
    return {true,std::move(index),{}};
}
Outcome<HouseholdProvisionManagementPage> query_household_provision_management(const CommonStore& common,const LocalEconomyState& economy,const LocalEconomyCatalog& original,const HouseholdProvisionManagementCatalog& catalog,const HouseholdProvisionManagementState& state,const HouseholdProvisionControlRead& controls,const HouseholdProvisionViewIndex& index,const LocalEconomyQuery& query){
    const auto fail=[](const StableId& code)->Outcome<HouseholdProvisionManagementPage>{return {false,{},common_failure(code).error};};
    if(query.observer_actor_id.empty()||query.observer_actor_id!=common.state().metadata.player_actor_id||query.authority)return fail("HouseholdProvisionObserverRequired");
    if(index.build_error)return {false,{},*index.build_error};
    const auto* actor=common.actor(query.observer_actor_id);const auto location=common.resolved_location(query.observer_actor_id);const auto* here=location.ok?std::get_if<SettlementLocation>(&location.value):nullptr;
    if(!actor||!actor->alive||!here||here->place_id!=query.place_id)return fail("HouseholdProvisionPlaceNotObservable");
    if(!same_identity(index.identity,common.state().identity)||index.observer_actor_id!=query.observer_actor_id||index.place_id!=query.place_id||economy.last_step_tick!=common.tick())return fail("StaleHouseholdProvisionIndex");
    Integer offset=0;if(query.cursor){const auto& cursor=*query.cursor;if(!same_identity(cursor.identity,index.identity)||cursor.observer_actor_id!=index.observer_actor_id||cursor.place_id!=index.place_id||cursor.authority)return fail("StaleHouseholdProvisionCursor");offset=cursor.offset;}
    if(offset<0||offset>static_cast<Integer>(index.authorization_ids.size()))return fail("InvalidHouseholdProvisionCursor");
    HouseholdProvisionManagementPage page;page.identity=index.identity;page.observer_actor_id=index.observer_actor_id;page.place_id=index.place_id;
    while(offset<static_cast<Integer>(index.authorization_ids.size())&&page.rows.size()<32){
        const auto& id=index.authorization_ids[static_cast<std::size_t>(offset++)];
        if(!original.household_provisioning||!local_economy_detail::has(catalog.authorization_ids,id))return fail("StaleHouseholdProvisionIndex");
        const auto* source=original.household_provisioning->authorizations.find(id);const auto* cohort=source?economy.cohorts.find(source->cohort_id):nullptr;
        const auto* obligation=source?economy.obligations.find(source->obligation_id):nullptr;const auto* binding=source?economy.provision_bindings.find(source->provision_binding_id):nullptr;
        if(!source||!cohort||!obligation||!binding||(cohort->site_id!=here->place_id&&cohort->site_id!=here->settlement_id))return fail("StaleHouseholdProvisionIndex");
        auto authority=controls.current(actor->id,source->household_group_id,household_provision_operation,common.tick());if(!authority.ok)return {false,{},authority.error};if(!authority.value)return fail("HouseholdProvisionControlDenied");
        auto checked=controls.validate_accepted(*authority.value,common.tick());if(!checked.ok)return {false,{},checked.error};
        if(authority.value->actor_id!=actor->id||authority.value->household_group_id!=source->household_group_id||authority.value->operation!=household_provision_operation||authority.value->source_revision!=common.state().identity.revision)return fail("HouseholdProvisionControlMismatch");
        const auto* inventory=common.inventory(source->inventory_id);if(!inventory||inventory->owner.kind!=Owner::Kind::household||inventory->owner.id!=source->household_group_id)return fail("HouseholdProvisionOwnerMismatch");
        auto food=common.count(source->inventory_id,source->item_id,{inventory->owner});if(!food.ok)return {false,{},food.error};
        HouseholdProvisionManagementRow row;row.authorization_id=id;row.household_group_id=source->household_group_id;row.cohort_id=source->cohort_id;row.item_id=source->item_id;row.headcount=cohort->headcount;row.available_food=food.value;row.next_due_tick=obligation->next_due_tick;row.through_tick=binding->through_tick;
        if(const auto* head=state.current_by_authorization.find(id)){
            const auto* record=state.configurations.find(*head);if(!record||record->authorization_id!=id)return fail("InvalidHouseholdProvisionConfiguration");
            if(record->effective_tick>common.tick()){
                row.pending_plan=plan(*record);if(!record->previous_configuration_id.empty()){record=state.configurations.find(record->previous_configuration_id);if(!record||record->authorization_id!=id)return fail("InvalidHouseholdProvisionConfiguration");row.current_plan=plan(*record);}
            }else row.current_plan=plan(*record);
        }
        if(const auto found=index.latest_ration_by_authorization.find(id);found!=index.latest_ration_by_authorization.end()){
            const auto* ration=economy.rations.find(found->second);if(!ration||ration->inventory_id!=source->inventory_id||ration->obligation_id!=source->obligation_id||ration->beneficiary.kind!=SubjectKind::cohort||ration->beneficiary.id!=source->cohort_id)return fail("StaleHouseholdProvisionIndex");row.latest_ration=*ration;
        }
        const auto available=index.availability.find(id);if(available==index.availability.end())return fail("StaleHouseholdProvisionIndex");
        row.may_configure=available->second.may_configure;row.may_cancel=available->second.may_cancel;row.configure_code=available->second.configure_code;row.cancel_code=available->second.cancel_code;
        page.rows.push_back(std::move(row));
    }
    if(offset<static_cast<Integer>(index.authorization_ids.size()))page.next_cursor=LocalEconomyCursor{index.identity,index.observer_actor_id,index.place_id,std::nullopt,offset};
    return {true,std::move(page),{}};
}
Outcome<LocalEconomyQuery> decode_household_provision_management_query(const BoundaryValue& value,const StableId& observer){
    try{boundary::fields(value,{"place_id"},{"cursor"},"/household_provision_management");auto query=decode_local_economy_query(value,observer);if(query.ok&&query.value.cursor&&query.value.cursor->authority)return {false,{},common_failure("HouseholdProvisionAuthorityNotAccepted").error};return query;}
    catch(const boundary::DecodeFailure& error){return {false,{},error.error};}
}
BoundaryValue encode_household_provision_management_page(const HouseholdProvisionManagementPage& page){
    V::Array rows;for(const auto& row:page.rows){V ration;
        if(row.latest_ration){const auto& r=*row.latest_ration;ration=boundary::obj({{"id",V{r.id}},{"due_tick",V{r.due_tick}},{"required",V{r.required}},{"consumed",V{r.consumed}},{"shortage",V{r.shortage}}});}
        rows.push_back(boundary::obj({{"authorization_id",V{row.authorization_id}},{"household_group_id",V{row.household_group_id}},{"cohort_id",V{row.cohort_id}},{"item_id",V{row.item_id}},{"headcount",V{row.headcount}},{"available_food",V{row.available_food}},{"next_due_tick",V{row.next_due_tick}},{"through_tick",V{row.through_tick}},{"current_plan",plan_value(row.current_plan)},{"pending_plan",plan_value(row.pending_plan)},{"latest_ration",std::move(ration)},{"may_configure",V{row.may_configure}},{"may_cancel",V{row.may_cancel}},{"configure_code",V{row.configure_code}},{"cancel_code",V{row.cancel_code}}}));
    }
    return boundary::obj({{"identity",identity_value(page.identity)},{"observer_actor_id",V{page.observer_actor_id}},{"place_id",V{page.place_id}},{"rows",V{std::move(rows)}},{"next_cursor",page.next_cursor?cursor_value(*page.next_cursor):V{}}});
}
}
