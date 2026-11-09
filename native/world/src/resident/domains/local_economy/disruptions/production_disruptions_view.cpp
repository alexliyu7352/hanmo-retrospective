#include "resident/checkpoint/checkpoint_economic_index_copy.hpp"
#include "production_disruptions_view.hpp"
#include <set>
namespace hanmo::resident {
namespace {
using V=BoundaryValue;
bool same_identity(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
bool same_source(const CommonStore& common,const LocalEconomyIndex& index,const WorldIdentity& identity,const StableId& actor,const StableId& place){return same_identity(common.state().identity,identity)&&same_identity(index.identity,identity)&&index.observer_actor_id==actor&&actor==common.state().metadata.player_actor_id&&index.place_id==place;}
template<class Ids>Outcome<V> causes_value(const ProductionDisruptionCatalog& catalog,const ProductionDisruptionState& state,const Ids& ids,const StableId& unit,Integer due){
    V::Array causes;Integer factor=10000;if(ids.size()>32)return {false,{},common_failure("ProductionDisruptionViewLimit").error};
    for(const auto& id:ids){const auto* registration=state.registrations.find(id);const auto* cause=registration?catalog.causes.find(registration->cause_id):nullptr;
        if(!cause||due<cause->from_tick||due>cause->through_tick||std::find(cause->unit_ids.begin(),cause->unit_ids.end(),unit)==cause->unit_ids.end())return {false,{},common_failure("StaleProductionDisruptionIndex").error};
        factor=std::min(factor,cause->remaining_yield_bp);causes.push_back(boundary::obj({{"reason_key",V{cause->reason_key}},{"from_tick",V{cause->from_tick}},{"through_tick",V{cause->through_tick}},{"remaining_yield_bp",V{cause->remaining_yield_bp}}}));
    }
    return {true,boundary::obj({{"remaining_yield_bp",V{factor}},{"combination_policy",V{catalog.combination_policy}},{"causes",V{std::move(causes)}}}),{}};
}
Outcome<V> settlement_value(const ProductionDisruptionCatalog& catalog,const ProductionDisruptionState& state,const LocalEconomyIndex& index,const StableId& unit,const StableId& receipt){
    const auto found=index.production_disruption_by_receipt.find(receipt);if(found==index.production_disruption_by_receipt.end())return {true,V{},{} };
    const auto* value=state.settlements.find(found->second);if(!value||value->unit_id!=unit||value->production_receipt_id!=receipt)return {false,{},common_failure("StaleProductionDisruptionIndex").error};
    auto encoded=causes_value(catalog,state,value->registration_fact_ids,unit,value->due_tick);if(!encoded.ok)return encoded;
    V::Array outputs;for(const auto& output:value->outputs)outputs.push_back(boundary::obj({{"item_id",V{output.item_id}},{"original_potential",V{output.original_potential}},{"unproduced",V{output.unproduced}},{"adjusted_potential",V{output.adjusted_potential}},{"stored",V{output.stored}},{"capacity_shortfall",V{output.capacity_shortfall}}}));
    encoded.value.as<V::Dict>()->emplace_back("outputs",V{std::move(outputs)});return encoded;
}
}
void index_production_disruptions(const CommonStore& common,const ProductionDisruptionCatalog& catalog,const ProductionDisruptionState& state,LocalEconomyIndex& index){
    index_memory::clear(index.text_memory,index.production_disruption_causes_by_unit);index_memory::clear(index.text_memory,index.production_disruption_by_receipt);
    index_memory::BorrowedIds units,receipts;
    const auto collect=[&](const CheckpointVector<LocalEconomySource>& rows){for(const auto& row:rows)if(row.kind==LocalEconomySource::Kind::production)units.insert(&row.id);};
    collect(index.personal_sources);for(const auto& row:index.authority_sources)collect(row.second);
    for(const auto& row:state.registrations.rows()){
        const auto* cause=catalog.causes.find(row.second.cause_id);if(!cause||common.tick()<cause->from_tick||common.tick()>cause->through_tick)continue;
        for(const auto& unit:cause->unit_ids)if(units.count(&unit))index_memory::entry(index.text_memory,index.production_disruption_causes_by_unit,unit).push_back(index_memory::copy(index.text_memory,row.first));
    }
    // 历史注记仅附于原合法收据，今日产权不能授予前任的私账知情权。
    for(const auto& row:index.production_receipts_by_unit)for(const auto& receipt:row.second)receipts.insert(&receipt);
    for(const auto& row:state.settlements.rows())if(receipts.count(&row.second.production_receipt_id))index_memory::insert(index.text_memory,index.production_disruption_by_receipt,row.second.production_receipt_id,row.first);
}
Outcome<BoundaryValue> encode_economy_with_production_disruptions(const CommonStore& common,const ProductionDisruptionCatalog& catalog,const ProductionDisruptionState& state,const LocalEconomyIndex& index,const LocalEconomyPage& page){
    if(!same_source(common,index,page.identity,page.observer_actor_id,page.place_id))return {false,{},common_failure("StaleProductionDisruptionIndex").error};
    auto encoded=encode_local_economy_page(page);auto& rows=*encoded.find("rows")->as<V::Array>();
    for(std::size_t i=0;i<page.rows.size();++i)if(const auto* unit=std::get_if<LocalProductionView>(&page.rows[i])){
        const auto current=index.production_disruption_causes_by_unit.find(unit->unit_id);if(current!=index.production_disruption_causes_by_unit.end()){
            auto info=causes_value(catalog,state,current->second,unit->unit_id,common.tick());if(!info.ok)return info;rows[i].as<V::Dict>()->emplace_back("production_disruption",std::move(info.value));
        }
        if(unit->last_result){auto info=settlement_value(catalog,state,index,unit->unit_id,unit->last_result->id);if(!info.ok)return info;if(!std::holds_alternative<std::monostate>(info.value.data))rows[i].find("last_result")->as<V::Dict>()->emplace_back("production_disruption",std::move(info.value));}
    }
    return {true,std::move(encoded),{}};
}
Outcome<BoundaryValue> encode_history_with_production_disruptions(const CommonStore& common,const ProductionDisruptionCatalog& catalog,const ProductionDisruptionState& state,const LocalEconomyIndex& index,const ProductionHistoryPage& page){
    if(!same_source(common,index,page.identity,page.observer_actor_id,page.place_id))return {false,{},common_failure("StaleProductionDisruptionIndex").error};
    auto encoded=encode_production_history_page(page);auto& rows=*encoded.find("rows")->as<V::Array>();
    for(std::size_t i=0;i<page.rows.size();++i){auto info=settlement_value(catalog,state,index,page.unit_id,page.rows[i].receipt_id);if(!info.ok)return info;if(!std::holds_alternative<std::monostate>(info.value.data))rows[i].as<V::Dict>()->emplace_back("production_disruption",std::move(info.value));}
    return {true,std::move(encoded),{}};
}
}
