// 接手者本人在原对白提出有限私粮照护，在世原父母本人接受；不授监护、收养或原粮处分权。
#pragma once
#include "child_care_origin.hpp"
#include "resident/domains/interaction/dialogue_node_source.hpp"
namespace hanmo::resident {
// 所有粮和工时仅来自提供者；有限援助不会解除出生双方的原责任。
struct ChildCareReliefTerms {
    StableId place_id,source_inventory_id,food_item_id;
    Integer period_ticks=0,food_per_period=0,prepaid_periods=0;
    FieldOrder field_order;
};
struct ChildCareReliefDefinition {
    StableId id,provider_actor_id,dialogue_id,node_id;
    Integer terms_version=1;
    ChildCareReliefTerms terms;
};
struct ChildCareReliefCatalog { bool enabled=false;OrderedTable<ChildCareReliefDefinition> offers; };
// 接手的原取消、死亡或有限期结束必须具名，不能只从原照护Action不存在推导。
struct ChildCareReliefSource {
    StableId offer_id,provider_actor_id,parent_actor_id,child_actor_id,birth_fact_id,original_accepted_id;
    StableId original_stop_fact_id,original_stop_kind;
    StableId dialogue_id,node_id,node_kind,session_id,scenario_id;
    WorldId world_id{};InteractionContext context;ChildCareReliefTerms terms;
    Integer terms_version=1,tick=0,revision=0,provider_birth_tick=0,parent_birth_tick=0;
    FieldOrder field_order;
};
struct ChildCareReliefQuote { AuthoredNodeSourceProof current;ChildCareReliefSource source; };
// 仅为源合同；后续资产预约、逐次Action与完成账尚须自己的原子事务。
Status validate_child_care_relief_catalog(const ChildCareReliefCatalog&,const CommonCatalog&,const InteractionCatalog&);
Outcome<ChildCareReliefQuote> preview_child_care_relief_source(const CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const ChildCareReliefCatalog&,const FamilyBirthState&,const FamilyDeathState&,const StableId& parent_actor_id,const StableId& child_actor_id,const StableId& offer_id);
Status revalidate_child_care_relief_source(const CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const ChildCareReliefCatalog&,const FamilyBirthState&,const FamilyDeathState&,const ChildCareReliefQuote&);
// actual_acceptance须是本域已落入候选的真实接受头，调用方核payload.tick相等；后来的死亡不抹旧同意。
Status validate_child_care_relief_source_before_acceptance(const CommonStore&,const InteractionCatalog&,const ChildCareReliefCatalog&,const FamilyBirthState&,const FamilyDeathState&,const ChildCareReliefSource&,const StableId& actual_acceptance);
Outcome<ChildCareReliefCatalog> decode_child_care_relief_catalog(const BoundaryValue&);
Outcome<ChildCareReliefSource> decode_child_care_relief_source(const BoundaryValue&);
BoundaryValue encode_child_care_relief_source(const ChildCareReliefSource&);
Status visit_child_care_relief_source(CodecVisitor&,ChildCareReliefSource&);
bool child_care_relief_terms_equal(const ChildCareReliefTerms&,const ChildCareReliefTerms&);
bool child_care_relief_source_equal(const ChildCareReliefSource&,const ChildCareReliefSource&);
}
