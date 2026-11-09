#include "resident/domains/military/military_model.hpp"
// 同候选在原军事事实分配后、资产转移前捕获完整责任，禁止夹入新实例。
#include "politics_takeover_capture.hpp"
#include "resident/domains/local_economy/fiscal/occupation_fiscal_responsibility.hpp"
#include "politics_takeover_source_conversion.hpp"
#include "politics_takeover_detail.hpp"
namespace hanmo::resident {
Outcome<PoliticalTakeoverCapture> capture_political_takeover_responsibilities(const CommonStore& common,const LocalEconomyState& economy,const LocalEconomyCatalog& catalog,const FiscalSettlementState* fiscal,const FiscalSettlementCatalog* fiscal_catalog,const PublicReliefState* relief,const TaskState& tasks,const TaskCatalog& task_catalog,const GovernanceTaskState* governance,const EnvoyTaskState* envoy,const MilitaryState& military,const StableId& occupation_id){return capture_political_takeover_responsibilities(common,economy,catalog,fiscal,fiscal_catalog,relief,tasks,task_catalog,governance,envoy,military,occupation_id,nullptr,nullptr);}
Outcome<PoliticalTakeoverCapture> capture_political_takeover_responsibilities(const CommonStore& common,const LocalEconomyState& economy,const LocalEconomyCatalog& catalog,const FiscalSettlementState* fiscal,const FiscalSettlementCatalog* fiscal_catalog,const PublicReliefState* relief,const TaskState& tasks,const TaskCatalog& task_catalog,const GovernanceTaskState* governance,const EnvoyTaskState* envoy,const MilitaryState& military,const StableId& occupation_id,const OccupationFiscalSettlementState* occupation_fiscal,const DynamicStepInput* phase){
    using namespace takeover_detail;const auto fail=[](const StableId& code){return Outcome<PoliticalTakeoverCapture>{false,{},common_failure(code).error};};
    const auto* occupation=takeover_detail::occupation(military,occupation_id);const auto* head=common.state().facts.find(occupation_id);const auto sequence=organization_instance_sequence(occupation_id,"military");
    if(!occupation||!head||head->kind!="military_occupation"||head->domain!="military"||!sequence||*sequence==INT64_MAX||common.state().next_instance_id!=*sequence+1||occupation->assets_applied.value_or(false)||occupation->asset_manifest||occupation->tick<common.tick()||occupation->tick-common.tick()>1)return fail("InvalidPoliticalTakeoverCaptureBoundary");
    PoliticalTakeoverCapture out;auto& r=out.value_;r.occupation_fact_id=occupation_id;r.source_identity=common.state().identity;r.tick=occupation->tick;r.captured_next_instance_id=common.state().next_instance_id;r.former_organization_id=occupation->from_organization_id;r.successor_organization_id=occupation->to_organization_id;r.public_wallet_ids=occupation->public_wallet_ids;r.public_inventory_ids=occupation->public_inventory_ids;
    const Owner old{Owner::Kind::organization,r.former_organization_id},next{Owner::Kind::organization,r.successor_organization_id};std::set<StableId> wallet_ids,inventory_ids;
    for(const auto& id:r.public_wallet_ids){const auto* w=common.wallet(id);if(!w||!owner_equal(w->owner,old)||!wallet_ids.insert(id).second)return fail("PoliticalTakeoverPublicWalletMismatch");out.assets_.wallets.push_back({id,{old,next,w->balance,{}}});out.wallet_reservations_.emplace_back(id,w->reserved);}
    for(const auto& id:r.public_inventory_ids){const auto* inventory=common.inventory(id);if(!inventory||!owner_equal(inventory->owner,old)||!inventory_ids.insert(id).second)return fail("PoliticalTakeoverPublicInventoryMismatch");OccupationInventoryManifest m;m.from_owner=old;m.to_owner=next;for(const auto& lot:inventory->lots)if(owner_equal(lot.owner,old)){m.public_lots.push_back({lot.id,lot.item_id,lot.quantity,{}});out.public_lot_reservations_.push_back({id,lot.id,lot.reserved_by});}else m.protected_lots.push_back(lot);out.assets_.inventories.push_back({id,std::move(m)});}
    // 禁用经济域没有经济责任；空公库同样无需调用要求非空清单的经济读口。
    if(catalog.enabled&&(!r.public_wallet_ids.empty()||!r.public_inventory_ids.empty())){
        EconomicPublicAccountSelection selected{old,r.public_wallet_ids,r.public_inventory_ids};auto dependencies=read_economic_public_account_dependencies(common,economy,catalog,fiscal,relief,selected);if(!dependencies.ok)return {false,{},dependencies.error};std::vector<EconomicResponsibilityRef> refs;
        for(const auto& p:dependencies.value.production){refs.push_back({EconomicResponsibilityKind::production_unit,p.unit_id,{}});for(const auto& id:p.asset_ids)refs.push_back({EconomicResponsibilityKind::production_asset_use,id,p.unit_id});if(p.pending_configuration_id)refs.push_back({EconomicResponsibilityKind::pending_configuration,*p.pending_configuration_id,{}});}
        for(const auto& p:dependencies.value.obligations){refs.push_back({EconomicResponsibilityKind::obligation,p.obligation_id,{}});for(const auto& id:p.arrear_ids)refs.push_back({EconomicResponsibilityKind::arrear,id,{}});}
        for(const auto& p:dependencies.value.fiscal)refs.push_back({EconomicResponsibilityKind::fiscal_assessment,p.assessment_id,{}});
        for(const auto& p:dependencies.value.relief)refs.push_back({EconomicResponsibilityKind::relief_contract,p.contract_id,{}});
        for(const auto& id:dependencies.value.supply_source_keys)refs.push_back({EconomicResponsibilityKind::supply_source,id,{}});
        for(const auto& ref:refs){auto original=read_economic_responsibility_source(common,economy,catalog,fiscal,fiscal_catalog,relief,selected,ref);if(!original.ok)return {false,{},original.error};auto converted=political_original_economic_source(original.value,out.assets_);if(!converted.ok)return {false,{},converted.error};r.responsibilities.push_back(std::move(converted.value));}
    }
    if(occupation_fiscal&&(!r.public_wallet_ids.empty()||!r.public_inventory_ids.empty())){
        if(!fiscal_catalog||!catalog.enabled)return fail("OccupationFiscalDependenciesRequired");EconomicPublicAccountSelection selected{old,r.public_wallet_ids,r.public_inventory_ids};
        auto dependencies=read_occupation_fiscal_public_account_dependencies(common,economy,*fiscal_catalog,*occupation_fiscal,selected,phase);if(!dependencies.ok)return {false,{},dependencies.error};
        for(const auto& item:dependencies.value){auto source=read_occupation_fiscal_responsibility_source(common,economy,*fiscal_catalog,*occupation_fiscal,selected,{EconomicResponsibilityKind::fiscal_assessment,item.assessment_id,{}},phase);if(!source.ok)return {false,{},source.error};auto converted=political_original_economic_source(source.value,out.assets_);if(!converted.ok)return {false,{},converted.error};r.responsibilities.push_back(std::move(converted.value));}
    }
    if(!r.public_wallet_ids.empty()||!r.public_inventory_ids.empty()){
        auto dependencies=read_task_public_account_dependencies(common,tasks,task_catalog,governance,envoy,{old,r.public_wallet_ids,r.public_inventory_ids});if(!dependencies.ok)return {false,{},dependencies.error};
        for(const auto& source:dependencies.value.responsibilities){auto converted=political_original_task_source(source,out.assets_);if(!converted.ok)return {false,{},converted.error};r.responsibilities.push_back(std::move(converted.value));}
    }
    std::sort(r.responsibilities.begin(),r.responsibilities.end(),[](const auto& a,const auto& b){return key(a.responsibility)<key(b.responsibility);});
    for(std::size_t i=0;i<r.responsibilities.size();++i)if(!valid_ref(r.responsibilities[i].responsibility)||(i&&key(r.responsibilities[i-1].responsibility)==key(r.responsibilities[i].responsibility)))return fail("DuplicatePoliticalResponsibility");
    return {true,std::move(out),{}};
}
}
