#include "resident/kernel/catalog_section_observation.hpp"
// 有限作者减产保留原收据；数量只在本期产出进入库存之前折减。
#include "production_disruptions.hpp"
#include "../local_economy_internal.hpp"
#include <algorithm>
#include <map>
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace {
// 共同事实只证明本域来源存在，不将作者计划当成动态事件授权。
bool fact_is(const CommonStore& common,const StableId& id,const StableId& kind){
    const auto* fact=common.state().facts.find(id);
    return fact&&fact->id==id&&fact->kind==kind&&fact->domain=="local_economy";
}
Outcome<StableId> new_fact(CommonStore& common,const StableId& kind){
    auto id=common.next_id(kind);if(!id.ok)return id;
    auto appended=common.append_fact({id.value,kind,"local_economy"});
    if(!appended.ok)return {false,{},appended.error};return id;
}
// 每项作者原因恰有一次登记，额外或被删除的登记事实都不是合法冷态。
Status registrations_valid(const CommonStore& common,const ProductionDisruptionCatalog& catalog,const ProductionDisruptionState& state,Integer tick){
    if(state.version!=1||tick<0||state.registrations.size()!=catalog.causes.size())return common_failure("ProductionDisruptionRegistrationMismatch");
    std::set<StableId> causes;
    for(const auto& row:state.registrations.rows()){
        const auto& registration=row.second;const auto* cause=catalog.causes.find(registration.cause_id);
        if(row.first!=registration.id||!cause||!causes.insert(cause->id).second||registration.tick<0||registration.tick>tick||registration.tick>cause->from_tick||!fact_is(common,row.first,"production_disruption_registered"))return common_failure("InvalidProductionDisruptionRegistration",row.first);
    }
    for(const auto& row:common.state().facts.rows())if(row.second.domain=="local_economy"&&row.second.kind=="production_disruption_registered"&&!state.registrations.find(row.first))return common_failure("MissingProductionDisruptionRegistration",row.first);
    return common_success();
}
using OutputKey=std::pair<StableId,StableId>;
// 正式收据仅存正数条目；重复容器/物品或额外输出不能被汇总隐藏。
Outcome<std::map<OutputKey,Integer>> receipt_quantities(const std::vector<ProductionQuantity>& rows){
    std::map<OutputKey,Integer> result;
    for(const auto& row:rows)if(row.inventory_id.empty()||row.item_id.empty()||row.quantity<=0||!result.emplace(OutputKey{row.inventory_id,row.item_id},row.quantity).second)return {false,{},common_failure("InvalidProductionDisruptionReceiptQuantity",row.item_id).error};
    return {true,std::move(result),{}};
}
bool same_output(const ProductionDisruptionOutput& a,const ProductionDisruptionOutput& b){
    return std::tie(a.inventory_id,a.item_id,a.original_potential,a.unproduced,a.adjusted_potential,a.stored,a.capacity_shortfall)==std::tie(b.inventory_id,b.item_id,b.original_potential,b.unproduced,b.adjusted_potential,b.stored,b.capacity_shortfall);
}
// 原潜在量 = 未产出量 + 最终潜在量；最终潜在量 = 实入库 + 容量损失。
Outcome<ProductionDisruptionSettlement> settlement_value(const ProductionDisruptionQuote& quote,const ProductionReceipt& receipt,const std::vector<ProductionQuantity>& potential){
    auto fail=[&](){return Outcome<ProductionDisruptionSettlement>{false,{},common_failure("InvalidProductionDisruptionSettlementQuantity",receipt.id).error};};
    if(receipt.id.empty()||quote.unit_id.empty()||quote.unit_id!=receipt.unit_id||quote.tick!=receipt.tick||quote.due_tick!=receipt.due_tick||quote.tick<quote.due_tick||quote.due_tick<0||quote.remaining_yield_bp<0||quote.remaining_yield_bp>10000||(quote.registration_fact_ids.empty()&&quote.remaining_yield_bp!=10000)||potential.empty())return fail();
    auto stored=receipt_quantities(receipt.outputs);if(!stored.ok)return {false,{},stored.error};
    auto shortfall=receipt_quantities(receipt.unrealized_output);if(!shortfall.ok)return {false,{},shortfall.error};
    ProductionDisruptionSettlement result;result.production_receipt_id=receipt.id;result.unit_id=receipt.unit_id;result.tick=receipt.tick;result.due_tick=receipt.due_tick;result.remaining_yield_bp=quote.remaining_yield_bp;result.registration_fact_ids=quote.registration_fact_ids;
    std::set<OutputKey> keys;
    for(const auto& row:potential){
        const OutputKey key{row.inventory_id,row.item_id};
        if(row.inventory_id.empty()||row.item_id.empty()||row.quantity<0||!keys.insert(key).second)return fail();
        auto adjusted=disrupted_production_quantity(row.quantity,quote.remaining_yield_bp);if(!adjusted.ok)return {false,{},adjusted.error};
        const auto actual=stored.value.find(key),lost=shortfall.value.find(key);
        const Integer quantity=actual==stored.value.end()?0:actual->second,capacity=lost==shortfall.value.end()?0:lost->second;
        Integer total=0;if(!local_economy_detail::add(quantity,capacity,total)||total!=adjusted.value)return fail();
        result.outputs.push_back({row.inventory_id,row.item_id,row.quantity,row.quantity-adjusted.value,adjusted.value,quantity,capacity});
    }
    for(const auto& row:stored.value)if(!keys.count(row.first))return fail();
    for(const auto& row:shortfall.value)if(!keys.count(row.first))return fail();
    return {true,std::move(result),{}};
}
}
Status validate_production_disruption_catalog(const LocalEconomyCatalog& original,const ProductionDisruptionCatalog& catalog){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 64, "ProductionDisruptionCatalog", "validate_production_disruption_catalog", &catalog);

    if(!original.enabled||catalog.version!=1||catalog.profile!=production_disruption_profile||catalog.combination_policy!=production_disruption_combination||catalog.causes.empty()||catalog.causes.size()>65536)return common_failure("InvalidProductionDisruptionCatalog");
    std::set<StableId> sources;std::map<StableId,Integer> causes_by_unit;
    for(const auto& row:catalog.causes.rows()){
        const auto& cause=row.second;
        if(row.first!=cause.id||cause.id.empty()||cause.source_key.empty()||!sources.insert(cause.source_key).second||cause.site_id.empty()||cause.reason_key.empty()||cause.from_tick<0||cause.through_tick<cause.from_tick||cause.remaining_yield_bp<0||cause.remaining_yield_bp>10000||cause.unit_ids.empty()||cause.unit_ids.size()>256)return common_failure("InvalidProductionDisruptionCause",row.first);
        std::set<StableId> units;for(const auto& id:cause.unit_ids){const auto* unit=original.production_units.find(id);const auto* binding=original.production_bindings.find(id);if(!unit||unit->site_id!=cause.site_id||!binding||binding->policy_owner!=ProvisionOwner::macro||!units.insert(id).second||++causes_by_unit[id]>32)return common_failure("InvalidProductionDisruptionUnit",id);}
    }
    return common_success();
}
Outcome<ProductionDisruptionState> make_initial_production_disruptions(CommonStore& common,const LocalEconomyCatalog& original,const ProductionDisruptionCatalog& catalog){
    local_economy_detail::PhaseSavepoint guard(common);if(!guard.token().ok)return {false,{},guard.token().error};
    auto result=make_initial_production_disruptions_in_candidate(common,original,catalog);if(!result.ok)return result;
    auto committed=guard.commit();if(!committed.ok)return {false,{},committed.error};return result;
}
Outcome<ProductionDisruptionState> make_initial_production_disruptions_in_candidate(CommonStore& common,const LocalEconomyCatalog& original,const ProductionDisruptionCatalog& catalog){
    auto valid=validate_production_disruption_catalog(original,catalog);if(!valid.ok)return {false,{},valid.error};
    for(const auto& row:catalog.causes.rows())if(row.second.from_tick<common.tick())return {false,{},common_failure("ProductionDisruptionBeforeRegistration",row.first).error};
    ProductionDisruptionState result;
    for(const auto& row:catalog.causes.rows()){auto id=new_fact(common,"production_disruption_registered");if(!id.ok)return {false,{},id.error};result.registrations.insert(id.value,{id.value,row.first,common.tick()});}
    return {true,std::move(result),{}};
}
Outcome<ProductionDisruptionQuote> quote_production_disruption(const CommonStore& common,const LocalEconomyCatalog& original,const ProductionDisruptionCatalog& catalog,const ProductionDisruptionState& state,const ProductionUnit& unit,Integer tick,Integer due){
    auto valid=validate_production_disruption_catalog(original,catalog);if(!valid.ok)return {false,{},valid.error};valid=registrations_valid(common,catalog,state,std::max(common.tick(),tick));if(!valid.ok)return {false,{},valid.error};
    const auto* authored=original.production_units.find(unit.id);if(!authored||authored->site_id!=unit.site_id||due<0||tick<due)return {false,{},common_failure("InvalidProductionDisruptionPeriod",unit.id).error};
    ProductionDisruptionQuote quote;quote.unit_id=unit.id;quote.tick=tick;quote.due_tick=due;
    for(const auto& row:state.registrations.rows()){
        const auto& cause=*catalog.causes.find(row.second.cause_id);
        if(cause.site_id!=unit.site_id||due<cause.from_tick||due>cause.through_tick||!local_economy_detail::has(cause.unit_ids,unit.id))continue;
        if(row.second.tick>due)return {false,{},common_failure("ProductionDisruptionBeforeRegistration",cause.id).error};
        quote.remaining_yield_bp=std::min(quote.remaining_yield_bp,cause.remaining_yield_bp);quote.registration_fact_ids.push_back(row.first);
    }
    return {true,std::move(quote),{}};
}
Outcome<Integer> disrupted_production_quantity(Integer potential,Integer factor){
    if(potential<0||factor<0||factor>10000)return {false,0,common_failure("InvalidProductionDisruptionQuantity").error};
    // 分段计算避免最大世界数量乘以10000溢出。
    return {true,(potential/10000)*factor+((potential%10000)*factor)/10000,{}};
}
Outcome<StableId> record_production_disruption(CommonStore& common,ProductionDisruptionState& state,const ProductionDisruptionQuote& quote,const ProductionReceipt& receipt,const std::vector<ProductionQuantity>& original){
    if(quote.registration_fact_ids.empty())return {true,{},{} };if(state.version!=1||!fact_is(common,receipt.id,"local_production"))return {false,{},common_failure("InvalidProductionDisruptionReceipt",receipt.id).error};
    std::set<StableId> sources;for(const auto& id:quote.registration_fact_ids)if(!sources.insert(id).second||!state.registrations.find(id)||!fact_is(common,id,"production_disruption_registered")||std::count(receipt.cause_fact_ids.begin(),receipt.cause_fact_ids.end(),id)!=1)return {false,{},common_failure("InvalidProductionDisruptionSource",id).error};
    auto result=settlement_value(quote,receipt,original);if(!result.ok)return {false,{},result.error};
    for(const auto& row:state.settlements.rows())if(row.second.production_receipt_id==receipt.id){const auto& prior=row.second;const auto& value=result.value;if(prior.unit_id!=value.unit_id||prior.tick!=value.tick||prior.due_tick!=value.due_tick||prior.remaining_yield_bp!=value.remaining_yield_bp||prior.registration_fact_ids!=value.registration_fact_ids||prior.outputs.size()!=value.outputs.size())return {false,{},common_failure("ProductionDisruptionReplayMismatch",receipt.id).error};for(std::size_t i=0;i<value.outputs.size();++i)if(!same_output(prior.outputs[i],value.outputs[i]))return {false,{},common_failure("ProductionDisruptionReplayMismatch",receipt.id).error};return {true,prior.id,{}};}
    auto id=new_fact(common,"production_disruption_settled");if(!id.ok)return id;result.value.id=id.value;state.settlements.insert(id.value,std::move(result.value));return id;
}
Status validate_production_disruptions(const CommonStore& common,const LocalEconomyState& economy,const LocalEconomyCatalog& original,const ProductionDisruptionCatalog& catalog,const ProductionDisruptionState& state){
    auto valid=validate_production_disruption_catalog(original,catalog);if(!valid.ok)return valid;valid=registrations_valid(common,catalog,state,std::max(common.tick(),economy.last_step_tick));if(!valid.ok)return valid;
    std::map<StableId,const ProductionDisruptionSettlement*> by_receipt;
    for(const auto& row:state.settlements.rows())if(row.first!=row.second.id||!fact_is(common,row.first,"production_disruption_settled")||!by_receipt.emplace(row.second.production_receipt_id,&row.second).second)return common_failure("InvalidProductionDisruptionSettlement",row.first);
    std::map<StableId,std::vector<Integer>> remainders;std::set<StableId> earlier,seen;std::size_t settled=0;
    for(const auto& fact:common.state().facts.rows()){
        if(fact.second.kind=="local_production"){
            const auto* receipt=economy.production_receipts.find(fact.first);const auto* unit=receipt?economy.production_units.find(receipt->unit_id):nullptr;const auto* authored=unit?original.production_units.find(unit->id):nullptr;const auto* recipe=unit?original.recipes.find(unit->recipe_id):nullptr;
            if(!receipt||!unit||!authored||!recipe)return common_failure("InvalidProductionDisruptionProduction",fact.first);
            auto quote=quote_production_disruption(common,original,catalog,state,*unit,receipt->tick,receipt->due_tick);if(!quote.ok)return {false,{},quote.error};
            for(const auto& id:quote.value.registration_fact_ids)if(!earlier.count(id)||std::count(receipt->cause_fact_ids.begin(),receipt->cause_fact_ids.end(),id)!=1)return common_failure("ProductionDisruptionCauseOrder",fact.first);
            for(const auto& id:receipt->cause_fact_ids)if(state.registrations.find(id)&&!local_economy_detail::has(quote.value.registration_fact_ids,id))return common_failure("UnexpectedProductionDisruptionCause",fact.first);
            auto [position,inserted]=remainders.emplace(unit->id,authored->output_remainders);auto& remaining=position->second;if(inserted&&remaining.empty())remaining.resize(recipe->outputs.size());if(remaining.size()!=recipe->outputs.size())return common_failure("InvalidProductionDisruptionRemainder",unit->id);
            std::vector<ProductionQuantity> potential;
            for(std::size_t i=0;i<recipe->outputs.size();++i){Integer quantity=0,next=0;const auto& output=recipe->outputs[i];if(!local_economy_detail::ratio(receipt->batches,output.numerator,output.denominator,remaining[i],quantity,next))return common_failure("ProductionDisruptionQuantityOverflow",fact.first);remaining[i]=next;potential.push_back({unit->output_inventory_id,output.item_id,quantity});}
            auto expected=settlement_value(quote.value,*receipt,potential);if(!expected.ok)return {false,{},expected.error};
            const auto found=by_receipt.find(receipt->id);
            if(quote.value.registration_fact_ids.empty()){if(found!=by_receipt.end())return common_failure("UnexpectedProductionDisruptionSettlement",receipt->id);}
            else{
                if(found==by_receipt.end())return common_failure("MissingProductionDisruptionSettlement",receipt->id);
                const auto& actual=*found->second;const auto& value=expected.value;if(actual.unit_id!=value.unit_id||actual.tick!=value.tick||actual.due_tick!=value.due_tick||actual.remaining_yield_bp!=value.remaining_yield_bp||actual.registration_fact_ids!=value.registration_fact_ids||actual.outputs.size()!=value.outputs.size())return common_failure("ProductionDisruptionSettlementMismatch",actual.id);
                for(std::size_t i=0;i<value.outputs.size();++i)if(!same_output(actual.outputs[i],value.outputs[i]))return common_failure("ProductionDisruptionOutputMismatch",actual.id);
                seen.insert(actual.id);++settled;
            }
        }else if(fact.second.kind=="production_disruption_settled"){
            const auto* receipt=state.settlements.find(fact.first);if(!receipt||!seen.count(fact.first)||!earlier.count(receipt->production_receipt_id)||fact.second.domain!="local_economy")return common_failure("ProductionDisruptionSettlementOrder",fact.first);
        }
        earlier.insert(fact.first);
    }
    if(settled!=state.settlements.size())return common_failure("ProductionDisruptionSettlementRegistryMismatch");
    for(const auto& row:remainders)if(economy.production_units.find(row.first)->output_remainders!=row.second)return common_failure("ProductionDisruptionRemainderMismatch",row.first);
    return common_success();
}
// 内部事实投影不授予玩家查询任意来源的权限。
std::optional<DynamicFactView> production_disruption_dynamic_fact(const ProductionDisruptionState& state,const StableId& id){
    DynamicFactView out;out.id=id;out.rule_version="production_disruptions_v1";
    if(const auto* registration=state.registrations.find(id)){
        out.kind="production_disruption_registered";out.tick=registration->tick;
        out.subject_id=registration->cause_id;out.outcome="authored_window_registered";return out;
    }
    if(const auto* result=state.settlements.find(id)){
        out.kind="production_disruption_settled";out.tick=result->tick;out.subject_id=result->unit_id;
        out.cause_fact_ids=result->registration_fact_ids;out.cause_fact_ids.push_back(result->production_receipt_id);
        out.outcome="actual_yield_settled";return out;
    }
    return {};
}
}
