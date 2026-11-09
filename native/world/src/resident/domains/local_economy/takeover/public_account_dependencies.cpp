// 单次内部预览沿原表枚举直接依赖；不触碰资产、ID、RNG或任何责任状态。
#include "public_account_dependencies.hpp"
#include "resident/domains/local_economy/fiscal/fiscal_settlement.hpp"
#include "resident/domains/local_economy/relief/public_relief_rules.hpp"
#include <map>
#include <set>
namespace hanmo::resident {
namespace {
using Result=Outcome<EconomicPublicAccountDependencies>;
Result reject(const StableId& code,const StableId& id={}){return {false,{},common_failure(code,id).error};}
}
Result read_economic_public_account_dependencies(const CommonRead& common,const LocalEconomyState& economy,
    const LocalEconomyCatalog& catalog,const FiscalSettlementState* fiscal,const PublicReliefState* relief,
    const EconomicPublicAccountSelection& selection){
    if(selection.original_owner.kind!=Owner::Kind::organization||selection.original_owner.id.empty())return reject("InvalidEconomicPublicAccountOwner");
    std::set<StableId> wallets,inventories;
    for(const auto& id:selection.wallet_ids){
        if(!wallets.insert(id).second)return reject("DuplicateEconomicPublicAccount",id);
        const auto* wallet=common.wallet(id);if(!wallet)return reject("UnknownEconomicPublicWallet",id);
        if(!owner_equal(wallet->owner,selection.original_owner))return reject("EconomicPublicAccountOwnerMismatch",id);
    }
    for(const auto& id:selection.inventory_ids){
        if(!inventories.insert(id).second)return reject("DuplicateEconomicPublicAccount",id);
        const auto* inventory=common.inventory(id);if(!inventory)return reject("UnknownEconomicPublicInventory",id);
        if(!owner_equal(inventory->owner,selection.original_owner))return reject("EconomicPublicAccountOwnerMismatch",id);
    }
    EconomicPublicAccountDependencies out;
    // 输入库共享不等于经营权转属；调用方还须读取原unit/asset的产权事实。
    for(const auto& row:economy.production_units.rows()){
        const auto& unit=row.second;EconomicProductionDependency dependency;dependency.unit_id=row.first;
        dependency.output_selected=inventories.count(unit.output_inventory_id)!=0;
        for(const auto& id:unit.input_inventory_ids)if(inventories.count(id))dependency.selected_input_inventory_ids.push_back(id);
        if(!dependency.output_selected&&dependency.selected_input_inventory_ids.empty())continue;
        dependency.labor_allocation_ids=unit.labor_allocation_ids;
        if(catalog.production_management){const auto* plan=catalog.production_management->plans.find(unit.id);if(plan)for(const auto& use:plan->asset_uses)dependency.asset_ids.push_back(use.asset_id);}
        if(economy.production_management)if(const auto* pending=economy.production_management->pending_by_unit.find(unit.id)){
            const auto* record=economy.production_management->configurations.find(*pending);
            if(!record||record->unit_id!=unit.id)return reject("InvalidEconomicProductionDependency",unit.id);
            dependency.pending_configuration_id=*pending;
        }
        out.production.push_back(std::move(dependency));
    }
    std::map<StableId,std::size_t> obligations;
    for(const auto& row:economy.obligations.rows()){
        const auto& obligation=row.second;const auto& sources=obligation.kind==ObligationKind::ration?inventories:wallets;
        EconomicObligationDependency dependency;dependency.obligation_id=row.first;
        dependency.source_selected=sources.count(obligation.source_wallet_or_inventory)!=0;
        dependency.destination_selected=sources.count(obligation.destination)!=0;
        if(!dependency.source_selected&&!dependency.destination_selected)continue;
        obligations.emplace(row.first,out.obligations.size());out.obligations.push_back(std::move(dependency));
    }
    // 已付旧期与尚欠新期都保留，不能把当前无人或active=false解释成责任消失。
    for(const auto& row:economy.arrears.rows())if(auto found=obligations.find(row.second.obligation_id);found!=obligations.end()){
        auto& dependency=out.obligations[found->second];dependency.arrear_ids.push_back(row.first);
        if(row.second.paid_amount<row.second.original_due)dependency.unpaid_arrear_ids.push_back(row.first);
    }
    for(const auto& row:economy.payments.rows())if(auto found=obligations.find(row.second.obligation_id);found!=obligations.end())out.obligations[found->second].payment_ids.push_back(row.first);
    for(const auto& row:economy.provision_bindings.rows())if(auto found=obligations.find(row.second.obligation_id);found!=obligations.end())out.obligations[found->second].provision_binding_ids.push_back(row.first);
    std::map<StableId,std::size_t> assessments;
    if(fiscal){
        for(const auto& row:fiscal->assessments.rows()){
            const auto& assessment=row.second;const auto* period=fiscal->periods.find(assessment.period_id);
            if(!period||!period->policy)return reject("InvalidEconomicFiscalDependency",row.first);
            EconomicFiscalDependency dependency;dependency.assessment_id=row.first;dependency.period_id=period->id;
            dependency.cash_source_selected=wallets.count(assessment.wallet_id)!=0;
            dependency.grain_source_selected=inventories.count(assessment.inventory_id)!=0;
            dependency.cash_destination_selected=wallets.count(period->policy->public_wallet_id)!=0;
            dependency.grain_destination_selected=inventories.count(period->policy->public_inventory_id)!=0;
            if(!dependency.cash_source_selected&&!dependency.grain_source_selected&&!dependency.cash_destination_selected&&!dependency.grain_destination_selected)continue;
            assessments.emplace(row.first,out.fiscal.size());out.fiscal.push_back(std::move(dependency));
        }
        for(const auto& row:fiscal->payments.rows())if(auto found=assessments.find(row.second.assessment_id);found!=assessments.end())out.fiscal[found->second].payment_ids.push_back(row.first);
    }
    std::map<StableId,std::size_t> terms;
    if(relief){
        for(const auto& row:relief->commitments.rows()){
            const auto& source=row.second.terms;EconomicReliefDependency dependency;
            dependency.terms_id=source.id;dependency.task_id=source.task_id;dependency.contract_id=source.contract_id;
            dependency.cash_source_selected=wallets.count(source.public_wallet_id)!=0;
            dependency.grain_source_selected=inventories.count(source.public_inventory_id)!=0;
            if(!dependency.cash_source_selected&&!dependency.grain_source_selected)continue;
            terms.emplace(source.id,out.relief.size());out.relief.push_back(std::move(dependency));
        }
        for(const auto& row:relief->receipts.rows())if(auto found=terms.find(row.second.terms_id);found!=terms.end())out.relief[found->second].receipt_ids.push_back(row.first);
        for(const auto& row:relief->releases.rows())if(auto found=terms.find(row.second.terms_id);found!=terms.end())out.relief[found->second].release_ids.push_back(row.first);
    }
    for(const auto& row:catalog.supply_sources.rows())if(inventories.count(row.second.inventory_id))out.supply_source_keys.push_back(row.first);
    return {true,std::move(out),{}};
}
}
