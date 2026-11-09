// 现有庄园界面的有限读取：先核当前玩家在场，再按所选tab投影，不返回整庄园或历史事实。
#include "estate_model.hpp"
#include "../../views/common_presentation.hpp"
#include "../../views/character_presentation.hpp"
namespace hanmo::resident {
namespace {
using V=BoundaryValue;using D=V::Dict;using A=V::Array;
// 仅给已经被本页选中的真人解析姓名；无效画像不会阻断姓名和经营摘要。
Outcome<V> person(const CommonPresentation& names,const StableId& id){auto name=names.display_name(id);if(!name.ok)return {false,{},name.error};return {true,boundary::obj({{"actor_id",V{id}},{"display_name",V{std::move(name.value)}}}),{}};}
// 地点解析失败沿Godot空place处理，不把军团或同行成员的位置猜成本人原始位置。
bool at_place(const CommonStore& common,const StableId& actor,const StableId& place){auto resolved=common.resolved_location(actor);auto* location=resolved.ok?std::get_if<SettlementLocation>(&resolved.value):nullptr;return location&&location->place_id==place;}
// 作者现有粮食与建材保原顺序去重，不枚举仓库内其他物品或未授权公共资产。
std::vector<StableId> storage_items(const EstateDefinition& definition){std::vector<StableId> items{definition.food_item_id};if(definition.construction.material_item_id!=definition.food_item_id)items.push_back(definition.construction.material_item_id);return items;}
// 只挑本庄最近完工；同tick保留共同事实登记中先遇者，失败和前任办理的建设照常显示。
V latest_construction(const CommonStore& common,const EstateState& state,const StableId& estate){
    const EstateConstructionFact* latest=nullptr;
    for(auto& header:common.state().facts.rows()){if(header.second.kind!="estate_construction")continue;auto* payload=state.facts.find(header.first);auto* fact=payload?std::get_if<EstateConstructionFact>(payload):nullptr;if(!fact||fact->phase!="completed"||fact->terms.estate_id!=estate)continue;if(!latest||fact->completed_tick>latest->completed_tick)latest=fact;}
    if(!latest)return V{D{}};return boundary::obj({{"facility",V{latest->terms.facility}},{"to_level",V{latest->terms.to_level}},{"success",V{latest->success}}});
}
}
// 可看只要求庄园存在且当前玩家同place；公职、所有权、伤病或闲忙均不新增可见性限制。
bool estate_available(const CommonStore& common,const EstateState& state,const EstateCatalog& catalog,const StableId& id){
    if(!state.estates.find(id))return false;auto* definition=catalog.estates.find(id);return definition&&at_place(common,common.state().metadata.player_actor_id,definition->place_id);
}
// 页面结构逐键对应estate_page.gd；命令权限仍由统一preview决定，读取不会改变钱货或时间。
Outcome<BoundaryValue> query_estate_page(const CommonStore& common,const EstateState& state,const EstateCatalog& catalog,const CommonCatalog& common_catalog,const ActivityRead& activity,const StableId& id,const StableId& tab){
    using namespace boundary;if(!estate_available(common,state,catalog,id))return {true,obj({{"available",V{false}}}),{}};
    const auto& estate=*state.estates.find(id);const auto& definition=*catalog.estates.find(id);auto observed=estate_view(common,state,catalog,activity,id);if(!observed.ok)return {false,{},observed.error};
    CommonPresentation names(common.state(),common_catalog);auto owner=person(names,estate.owner_actor_id);if(!owner.ok)return owner;V manager{D{}};
    if(!estate.manager_actor_id.empty()){
        auto selected=person(names,estate.manager_actor_id);if(!selected.ok)return selected;manager=std::move(selected.value);CommonCharacterPresentation portraits(common.state(),common_catalog);auto character=portraits.character(estate.manager_actor_id);V portrait;
        if(character.ok){auto encoded=encode_character_presentation(character.value);if(!encoded.ok)return encoded;portrait=std::move(encoded.value);}else portrait=obj({{"ok",V{false}},{"code",V{character.error.original.code}}});
        std::get<D>(manager.data).emplace_back("portrait_input",std::move(portrait));
    }
    V production{D{}};if(observed.value.last_production){auto& last=*observed.value.last_production;production=obj({{"produced_quantity",V{last.produced.quantity}},{"consumed_quantity",V{last.consumed.quantity}},{"lost_output",V{last.lost_output}},{"unfed_count",V{Integer(last.unfed.size())}}});}
    V current{D{}},completion{D{}};auto* actor=common.actor(common.state().metadata.player_actor_id);if(!actor)return {false,{},common_failure("InvalidEstateViewActor").error};
    if(!actor->action_id.empty()){
        auto* action=common.action(actor->action_id);if(!action)return {false,{},common_failure("InvalidEstateViewAction",actor->action_id).error};current=obj({{"kind",V{action->kind}},{"remaining_ticks",V{action->remaining_ticks}}});
        if(action->kind=="estate_build"){auto* params=state.actions.find(action->id);if(!params)return {false,{},common_failure("InvalidEstateBuildTerms",action->id).error};std::get<D>(current.data).emplace_back("facility",V{params->terms.facility});}
    }else completion=latest_construction(common,state,id);
    D fields{{"available",V{true}},{"estate_id",V{id}},{"owner",std::move(owner.value)},{"manager",std::move(manager)},{"workforce",obj({{"workers_count",V{Integer(observed.value.available_workers.size())}},{"guards_count",V{Integer(observed.value.available_guards.size())}},{"defense",V{observed.value.defense}}})},{"last_production",std::move(production)},{"activity",std::move(current)},{"completion",std::move(completion)}};
    if(tab=="build")fields.emplace_back("facilities",V{A{obj({{"facility_id",V{StableId{"farm"}}},{"level",V{estate.facilities.farm}}}),obj({{"facility_id",V{StableId{"granary"}}},{"level",V{estate.facilities.granary}}}),obj({{"facility_id",V{StableId{"palisade"}}},{"level",V{estate.facilities.palisade}}})}});
    else if(tab=="storage"){
        auto* inventory=common.inventory(definition.inventory_id);auto* wallet=common.wallet(definition.wallet_id);if(!inventory||!wallet)return {false,{},common_failure("EstateAssetsMissing",id).error};auto weight=common.inventory_weight(inventory->id);if(!weight.ok)return {false,{},weight.error};Owner private_owner{Owner::Kind::actor,estate.owner_actor_id,{}};A items;
        for(auto& item:storage_items(definition)){auto quantity=common.count(inventory->id,item,{private_owner});if(!quantity.ok)return {false,{},quantity.error};items.push_back(obj({{"item_id",V{item}},{"available_quantity",V{quantity.value}}}));}
        fields.emplace_back("storage",obj({{"wallet_balance",V{wallet->balance}},{"weight",V{weight.value}},{"capacity",V{inventory->capacity}},{"items",V{std::move(items)}}}));
    }else{
        A residents;for(auto& id:estate.resident_actor_ids){auto selected=person(names,id);if(!selected.ok)return selected;auto* duty=estate.duties.find(id);if(!duty)return {false,{},common_failure("InvalidEstateDuties",id).error};auto* source=estate.detached.find(id);auto& row=std::get<D>(selected.value.data);row.emplace_back("absent",V{!at_place(common,id,definition.place_id)});row.emplace_back("duty",V{*duty});row.emplace_back("detached",V{source!=nullptr});row.emplace_back("source_id",V{source?*source:StableId{}});residents.push_back(std::move(selected.value));}fields.emplace_back("residents",V{std::move(residents)});
    }
    return {true,V{std::move(fields)},{}};
}
// Main只取得当前可见庄园的命令参数候选；不提供余额、事实、所有人权限或可写人物记录。
Outcome<BoundaryValue> query_estate_inputs(const CommonStore& common,const EstateState& state,const EstateCatalog& catalog,const StableId& id){
    using namespace boundary;if(!estate_available(common,state,catalog,id))return {true,obj({{"available",V{false}}}),{}};auto& estate=*state.estates.find(id);auto& definition=*catalog.estates.find(id);
    return {true,obj({{"available",V{true}},{"estate_id",V{id}},{"resident_actor_ids",string_array(estate.resident_actor_ids)},{"facility_ids",string_array({"farm","granary","palisade"})},{"item_ids",string_array(storage_items(definition))}}),{}};
}
} // 只有指定页和指定庄园会被读取；返回值深拥有，后续候选更新不会改写旧页面。
