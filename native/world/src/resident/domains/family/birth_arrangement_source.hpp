// 成年双方的生育及有限初期照护意愿；本来源本身不建立孕期、不扣粮、不生成人物。
#pragma once
#include "resident/domains/interaction/dialogue_node_source.hpp"
#include "life_age_basis.hpp"
#include "resident/domains/actor_foundation/actor_foundation_refs.hpp"
namespace hanmo::resident {
struct BoundaryValue;
class CodecVisitor;
class FamilyContinuationPeopleRead4;
// 只允许双方之一明确照护，当前另一方以本人私库存承担有限粮食；没有第三人的默认承诺。
struct BirthCareTerms {
    StableId gestating_actor_id,caregiver_actor_id,payer_actor_id,place_id,source_inventory_id;
    StableId food_item_id,child_prototype_id;
    Integer birth_duration_ticks=0,care_period_ticks=0,food_per_period=0,prepaid_care_periods=0;
    FieldOrder field_order;
};
struct BirthArrangementDefinition {
    StableId id,speaker_actor_id,listener_actor_id,dialogue_id,node_id;
    Integer terms_version=1;
    BirthCareTerms terms;
};
struct BirthArrangementCatalog { bool enabled=false;OrderedTable<BirthArrangementDefinition> arrangements;Integer selected_version=1;std::optional<ActorFoundationRuleRef> due_policy_ref;std::optional<StableId> dependent_provision_profile_ref; };
// 冷保存本人真正听到的原条款；后来的死亡/迁居不删除历史意愿，也不证明已提供照护。
struct BirthArrangementSource {
    StableId arrangement_id,speaker_actor_id,listener_actor_id,dialogue_id,node_id,node_kind,session_id,scenario_id;
    WorldId world_id{};InteractionContext context;BirthCareTerms terms;
    Integer terms_version=1,tick=0,revision=0,speaker_birth_tick=0,listener_birth_tick=0;
    FieldOrder field_order;
    std::optional<FamilyDecisionAgeSources4> age_sources;
};
struct BirthArrangementQuote { AuthoredNodeSourceProof current;BirthArrangementSource source; };
struct FamilyCatalog;
// 目录只有两位原成年人角色和原型引用；新生儿原型必须没有已学经验，不能继承亲代技能。
Status validate_birth_arrangement_catalog(const BirthArrangementCatalog&,const CommonCatalog&,const InteractionCatalog&,const FamilyCatalog&);
Status validate_birth_arrangement_source(const CommonStore&,const InteractionCatalog&,const FamilyCatalog&,const BirthArrangementCatalog&,const BirthArrangementSource&,const FamilyContinuationPeopleRead4* people=nullptr,const StableId& actual_before_fact={});
Outcome<BirthArrangementQuote> preview_birth_arrangement_source(const CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const FamilyCatalog&,const BirthArrangementCatalog&,const StableId& listener,const StableId& arrangement_id,const FamilyContinuationPeopleRead4* people=nullptr);
Status revalidate_birth_arrangement_source(const CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const FamilyCatalog&,const BirthArrangementCatalog&,const BirthArrangementQuote&,const FamilyContinuationPeopleRead4* people=nullptr);
bool birth_care_terms_equal(const BirthCareTerms&,const BirthCareTerms&);
bool birth_arrangement_source_equal(const BirthArrangementSource&,const BirthArrangementSource&);
Outcome<BirthArrangementCatalog> decode_birth_arrangement_catalog(const BoundaryValue&);
Outcome<BirthArrangementCatalog> decode_birth_arrangement_catalog_complete(const BoundaryValue&,Integer selected_version,const CatalogIdentity&);
Outcome<BirthArrangementSource> decode_birth_arrangement_source(const BoundaryValue&,Integer selected_version=1);
BoundaryValue encode_birth_arrangement_source(const BirthArrangementSource&,Integer selected_version=1);
Status visit_birth_arrangement_source(CodecVisitor&,BirthArrangementSource&,Integer selected_version=1);
}
