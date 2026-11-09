#include "resident/checkpoint/checkpoint_economic_index_copy.hpp"
#include "production_history.hpp"
#include "../takeover/execution_materials.hpp"
#include "../local_economy_internal.hpp"
#include <algorithm>
#include <limits>
#include <set>
namespace hanmo::resident {
namespace {
using V=BoundaryValue;
bool same_identity(const WorldIdentity& a,const WorldIdentity& b) {
    return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;
}
bool same_authority(const std::optional<ActingAuthority>& a,const std::optional<ActingAuthority>& b) {
    return a.has_value()==b.has_value()&&(!a||(a->organization_id==b->organization_id&&a->office_id==b->office_id&&a->actor_id==b->actor_id&&a->authority_epoch==b->authority_epoch));
}
// There is no historical ownership snapshot in legacy production receipts. Only the
// original authored owner and unchanged production containers can support this view.
// A takeover must provide its own proven historical access before this guard expands.
bool continuous_unit(const CommonStore& common,const LocalEconomyCatalog& catalog,const ProductionUnit& unit) {
    const auto* initial=catalog.production_units.find(unit.id);
    if(!initial||!owner_equal(initial->owner,unit.owner)||initial->site_id!=unit.site_id||
       initial->recipe_id!=unit.recipe_id||initial->output_inventory_id!=unit.output_inventory_id||
       initial->input_inventory_ids!=unit.input_inventory_ids)return false;
    const auto same_owner=[&](const StableId& id){const auto* inventory=common.inventory(id);return inventory&&owner_equal(inventory->owner,initial->owner);};
    if(!same_owner(unit.output_inventory_id))return false;
    for(const auto& id:unit.input_inventory_ids)if(!same_owner(id))return false;
    return true;
}
bool receipt_visible(const CommonStore& common,const ProductionUnit& unit,const ProductionReceipt& receipt,const Owner& owner) {
    const auto* fact=common.state().facts.find(receipt.id);
    if(!fact||fact->domain!="local_economy"||fact->kind!="local_production"||receipt.unit_id!=unit.id||receipt.recipe_id!=unit.recipe_id||receipt.tick>common.tick())return false;
    for(const auto& quantity:receipt.inputs) {
        if(std::find(unit.input_inventory_ids.begin(),unit.input_inventory_ids.end(),quantity.inventory_id)==unit.input_inventory_ids.end())return false;
        const auto* inventory=common.inventory(quantity.inventory_id);
        if(!inventory||!owner_equal(inventory->owner,owner))return false;
    }
    for(const auto* quantities:{&receipt.outputs,&receipt.unrealized_output})for(const auto& quantity:*quantities)
        if(quantity.inventory_id!=unit.output_inventory_id)return false;
    return true;
}
// 当前继任者只能读自己明确获准经营时形成的真实收据，不取得前任或未裁定期间的私账。
bool taken_receipt_visible(const EconomicTakeoverReadContext& read,const ProductionUnit& unit,const ProductionReceipt& receipt,const Owner& owner){
    if(owner.kind!=Owner::Kind::organization||!receipt_visible(read.common,unit,receipt,owner))return false;
    for(const auto& ref:economic_production_responsibilities(read.catalog,unit.id)){auto mapped=economic_responsibility_political_ref(ref);if(!mapped.ok)return false;auto source=read.politics.political_responsibility_before_fact(mapped.value,receipt.id,receipt.tick,read.common.state().identity);if(!source.ok||!source.value||source.value->status!=PoliticalResponsibilityExecutionStatus::approved||!source.value->decision||!source.value->decision->execution_principal||source.value->decision->execution_principal->kind!=SubjectKind::organization||source.value->decision->execution_principal->id!=owner.id)return false;}return true;
}
V identity_value(const WorldIdentity& identity) {
    StableId hex;const char* digits="0123456789abcdef";
    for(auto byte:identity.world_id){hex.push_back(digits[byte>>4]);hex.push_back(digits[byte&15]);}
    return boundary::obj({{"world_id",V{hex}},{"execution_epoch",V{identity.execution_epoch}},{"revision",V{identity.revision}},{"tick",V{identity.tick}}});
}
V authority_value(const std::optional<ActingAuthority>& authority) {
    if(!authority)return {};
    return boundary::obj({{"organization_id",V{authority->organization_id}},{"office_id",V{authority->office_id}},{"authority_epoch",V{authority->authority_epoch}},{"actor_id",V{authority->actor_id}}});
}
V cursor_value(const ProductionHistoryCursor& cursor) {
    const auto& source=cursor.source;
    return boundary::obj({{"unit_id",V{cursor.unit_id}},{"source",boundary::obj({{"identity",identity_value(source.identity)},{"observer_actor_id",V{source.observer_actor_id}},{"place_id",V{source.place_id}},{"authority",authority_value(source.authority)},{"offset",V{source.offset}}})}});
}
bool quantities(const std::vector<ProductionQuantity>& source,std::vector<ProductionHistoryQuantity>& target) {
    std::map<StableId,Integer> totals;
    for(const auto& quantity:source)
        if(quantity.quantity<=0||!local_economy_detail::add(totals[quantity.item_id],quantity.quantity,totals[quantity.item_id]))return false;
    for(const auto& row:totals)target.push_back({row.first,row.second});
    return true;
}
V quantities_value(const std::vector<ProductionHistoryQuantity>& quantities) {
    V::Array values;for(const auto& quantity:quantities)values.push_back(boundary::obj({{"item_id",V{quantity.item_id}},{"quantity",V{quantity.quantity}}}));
    return V{std::move(values)};
}
}
void index_production_history(const CommonStore& common,const LocalEconomyState& state,const LocalEconomyCatalog& catalog,LocalEconomyIndex& index,const EconomicTakeoverReadContext* takeover) {
    index_memory::clear(index.text_memory,index.production_receipts_by_unit);
    index_memory::BorrowedIds units;
    const auto collect=[&](const CheckpointVector<LocalEconomySource>& sources){for(const auto& source:sources)if(source.kind==LocalEconomySource::Kind::production)units.insert(&source.id);};
    collect(index.personal_sources);for(const auto& authority:index.authority_sources)collect(authority.second);
    for(const auto* borrowed:units) {const auto& id=*borrowed;
        const auto* unit=state.production_units.find(id);
        if(unit&&((takeover&&index.production_execution_owners.count(id))||continuous_unit(common,catalog,*unit)))index_memory::entry(index.text_memory,index.production_receipts_by_unit,id);
    }
    for(const auto& row:state.production_receipts.rows()) {
        const auto found=index.production_receipts_by_unit.find(row.second.unit_id);
        if(found==index.production_receipts_by_unit.end())continue;
        const auto* unit=state.production_units.find(row.second.unit_id);
        if(!unit)continue;const auto taken=index.production_execution_owners.find(unit->id);const bool visible=taken==index.production_execution_owners.end()?receipt_visible(common,*unit,row.second,unit->owner):takeover&&taken_receipt_visible(*takeover,*unit,row.second,taken->second);if(visible)found->second.push_back(index_memory::copy(index.text_memory,row.first));
    }
    for(auto& entry:index.production_receipts_by_unit)std::sort(entry.second.begin(),entry.second.end(),[&](const StableId& a,const StableId& b){
        const auto& x=*state.production_receipts.find(a);const auto& y=*state.production_receipts.find(b);
        return std::tie(x.due_tick,x.tick,x.id)>std::tie(y.due_tick,y.tick,y.id);
    });
}
void index_production_history(const CommonStore& c,const LocalEconomyState& s,const LocalEconomyCatalog& catalog,LocalEconomyIndex& index){index_production_history(c,s,catalog,index,nullptr);}
Outcome<ProductionHistoryPage> query_production_history(const CommonStore& common,const LocalEconomyState& state,const LocalEconomyCatalog& catalog,const OrganizationRead& organizations,const LocalEconomyIndex& index,const ProductionHistoryQuery& query) {
    const auto fail=[](const StableId& code)->Outcome<ProductionHistoryPage>{return {false,{},common_failure(code).error};};
    if(!catalog.enabled)return fail("LocalEconomyUnavailable");
    const auto& identity=common.state().identity;
    if(query.observer_actor_id!=common.state().metadata.player_actor_id||query.observer_actor_id.empty())return fail("LocalEconomyObserverRequired");
    const auto* actor=common.actor(query.observer_actor_id);const auto location=common.resolved_location(query.observer_actor_id);
    const auto* here=location.ok?std::get_if<SettlementLocation>(&location.value):nullptr;
    if(!actor||!actor->alive||!here||here->place_id!=query.place_id)return fail("LocalEconomyPlaceNotObservable");
    if(state.last_step_tick!=common.tick())return fail("LocalEconomyClockMismatch");
    if(!same_identity(index.identity,identity)||index.observer_actor_id!=query.observer_actor_id||index.place_id!=query.place_id)return fail("StaleLocalEconomyIndex");
    if(query.authority) {
        const auto& authority=*query.authority;
        if(authority.actor_id!=query.observer_actor_id)return fail("LocalEconomyAuthorityActorMismatch");
        auto proof=organizations.authorize(authority,"inspect_local_economy",{ScopeRef::Kind::site,query.place_id},0);
        if(!proof.ok)return {false,{},proof.error};
    }
    const auto* unit=state.production_units.find(query.unit_id);
    const auto found=index.production_receipts_by_unit.find(query.unit_id);
    const auto taken=index.production_execution_owners.find(query.unit_id);
    if(!unit||found==index.production_receipts_by_unit.end()||(taken==index.production_execution_owners.end()&&!continuous_unit(common,catalog,*unit))||
       (unit->site_id!=here->place_id&&unit->site_id!=here->settlement_id))return fail("ProductionHistoryNotObservable");
    const Owner& operating=taken==index.production_execution_owners.end()?unit->owner:taken->second;
    const bool personal=operating.kind==Owner::Kind::actor&&operating.id==query.observer_actor_id;
    if(!personal) {
        if(!query.authority||operating.kind!=Owner::Kind::organization||operating.id!=query.authority->organization_id)return fail("ProductionHistoryNotObservable");
        auto proof=organizations.authorize(*query.authority,"inspect_local_economy",{ScopeRef::Kind::site,unit->site_id},0);
        if(!proof.ok)return {false,{},proof.error};
    }
    Integer offset=0;
    if(query.cursor) {
        const auto& cursor=*query.cursor;const auto& source=cursor.source;
        if(cursor.unit_id!=query.unit_id||!same_identity(source.identity,identity)||source.observer_actor_id!=query.observer_actor_id||source.place_id!=query.place_id||!same_authority(source.authority,query.authority))return fail("StaleProductionHistoryCursor");
        offset=source.offset;
    }
    const auto& ids=found->second;
    if(ids.size()>static_cast<std::size_t>(std::numeric_limits<Integer>::max()))return fail("LocalEconomyViewLimit");
    if(offset<0||offset>static_cast<Integer>(ids.size()))return fail("InvalidProductionHistoryCursor");
    ProductionHistoryPage page;page.identity=identity;page.observer_actor_id=query.observer_actor_id;page.place_id=query.place_id;page.unit_id=query.unit_id;
    while(offset<static_cast<Integer>(ids.size())&&page.rows.size()<static_cast<std::size_t>(local_economy_page_limit)) {
        const auto* receipt=state.production_receipts.find(ids[static_cast<std::size_t>(offset++)]);
        if(!receipt||!receipt_visible(common,*unit,*receipt,operating))return fail("StaleLocalEconomyIndex");
        ProductionHistoryReceipt row;row.receipt_id=receipt->id;row.tick=receipt->tick;row.due_tick=receipt->due_tick;
        row.allocated_workers=receipt->allocated_workers;row.effective_workers=receipt->effective_workers;row.batches=receipt->batches;row.context=receipt->production_context;
        if(!quantities(receipt->inputs,row.inputs)||!quantities(receipt->outputs,row.outputs)||!quantities(receipt->unrealized_output,row.unrealized_output))return fail("InvalidProductionHistoryQuantity");
        page.rows.push_back(std::move(row));
    }
    if(offset<static_cast<Integer>(ids.size()))page.next_cursor=ProductionHistoryCursor{{identity,query.observer_actor_id,query.place_id,query.authority,offset},query.unit_id};
    return {true,std::move(page),{}};
}
Outcome<ProductionHistoryQuery> decode_production_history_query(const BoundaryValue& value,const StableId& observer) {
    try {
        boundary::fields(value,{"place_id","unit_id"},{"cursor","authority"},"/local_production_history");
        V::Dict scope={{"place_id",boundary::field(value,"place_id","")}};
        if(const auto* authority=value.find("authority"))scope.emplace_back("authority",*authority);
        auto decoded=decode_local_economy_query(V{std::move(scope)},observer);if(!decoded.ok)return {false,{},decoded.error};
        ProductionHistoryQuery query;query.observer_actor_id=observer;query.place_id=decoded.value.place_id;query.authority=decoded.value.authority;
        query.unit_id=boundary::get_string(value,"unit_id","");if(query.unit_id.empty())boundary::fail("ProductionHistoryUnitRequired","/unit_id");
        if(const auto* cursor=value.find("cursor"))if(!std::holds_alternative<std::monostate>(cursor->data)) {
            boundary::fields(*cursor,{"source","unit_id"},{},"/cursor");
            auto source=decode_local_economy_cursor(boundary::field(*cursor,"source","/cursor"));if(!source.ok)return {false,{},source.error};
            query.cursor=ProductionHistoryCursor{source.value,boundary::get_string(*cursor,"unit_id","/cursor")};
        }
        return {true,std::move(query),{}};
    }catch(const boundary::DecodeFailure& error){return {false,{},error.error};}
}
BoundaryValue encode_production_history_page(const ProductionHistoryPage& page) {
    V::Array rows;
    for(const auto& receipt:page.rows) {
        V context;
        if(receipt.context)context=boundary::obj({{"capacity",V{receipt.context->capacity}},{"region_factor_bp",V{receipt.context->region_factor_bp}},{"season_factor_bp",V{receipt.context->season_factor_bp}},{"season_phase",V{receipt.context->season_phase}}});
        rows.push_back(boundary::obj({{"receipt_id",V{receipt.receipt_id}},{"tick",V{receipt.tick}},{"due_tick",V{receipt.due_tick}},{"allocated_workers",V{receipt.allocated_workers}},{"effective_workers",V{receipt.effective_workers}},{"batches",V{receipt.batches}},{"inputs",quantities_value(receipt.inputs)},{"outputs",quantities_value(receipt.outputs)},{"unrealized_output",quantities_value(receipt.unrealized_output)},{"production_context",std::move(context)}}));
    }
    return boundary::obj({{"identity",identity_value(page.identity)},{"observer_actor_id",V{page.observer_actor_id}},{"place_id",V{page.place_id}},{"unit_id",V{page.unit_id}},{"rows",V{std::move(rows)}},{"next_cursor",page.next_cursor?cursor_value(*page.next_cursor):V{}}});
}
}
