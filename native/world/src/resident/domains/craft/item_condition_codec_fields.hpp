// One field-order definition for embedded original sources and result projection.
#pragma once
#include "item_condition_model.hpp"
namespace hanmo::resident::item_condition_fields {
template<class A>void fields(A&a,BattleWear&v){a("battle_or_session_ref",v.battle_or_session_ref);a("result_ref",v.result_ref);a("usage_ordinal",v.usage_ordinal);}
template<class A>void fields(A&a,GatherToolWear&v){a("gather_receipt_ref",v.gather_receipt_ref);a("tool_usage_ordinal",v.tool_usage_ordinal);}
template<class A>void fields(A&a,InfusionFailureWear&v){a("infusion_result_ref",v.infusion_result_ref);a("item_slot_ordinal",v.item_slot_ordinal);}
template<class A>void fields(A&a,FacilityAttackToolWear&v){a("facility_attack_result_id",v.facility_attack_result_id);a("episode_id",v.episode_id);a("period_ordinal",v.period_ordinal);a("tool_usage_ordinal",v.tool_usage_ordinal);}
// A supplies reading(), operator()(name,value), reject(code,path). The writer
// visits the original active branch by reference and never changes published data.
template<class A>void fields(A&a,ItemUseSource&v){
 StableId kind;if(!a.reading())switch(v.index()){case 0:kind="battle_wear";break;case 1:kind="gather_tool_wear";break;case 2:kind="infusion_failure_wear";break;case 3:kind="facility_attack_tool_wear";break;default:a.reject("InvalidItemUseSource","kind");return;}
 a("kind",kind);if(a.reading()){if(kind=="battle_wear"){if(!a.template prepare_default_values<BattleWear>().ok)return;v=BattleWear{};}else if(kind=="gather_tool_wear"){if(!a.template prepare_default_values<GatherToolWear>().ok)return;v=GatherToolWear{};}else if(kind=="infusion_failure_wear"){if(!a.template prepare_default_values<InfusionFailureWear>().ok)return;v=InfusionFailureWear{};}else if(kind=="facility_attack_tool_wear"){if(!a.template prepare_default_values<FacilityAttackToolWear>().ok)return;v=FacilityAttackToolWear{};}else{a.reject("InvalidItemUseSource","kind");return;}}
 std::visit([&](auto&row){fields(a,row);},v);
}
template<class A>void fields(A&a,ItemConditionResultView&v){
 a("result_id",v.result_id);a("instance_id",v.instance_id);a("source_kind",v.source_kind);a("use_source",v.use_source);a("source_fact_ref",v.source_fact_ref);a("source_activity_ref",v.source_activity_ref);a("usage_ordinal",v.usage_ordinal);a("effective_tick",v.effective_tick);a("instance_version_before",v.instance_version_before);a("instance_version_after",v.instance_version_after);a("durability_before",v.durability_before);a("durability_after",v.durability_after);a("max_durability_before",v.max_durability_before);a("max_durability_after",v.max_durability_after);a("resource_effect_refs",v.resource_effect_refs);a("disposition",v.disposition);a("reason",v.reason);
}
}
