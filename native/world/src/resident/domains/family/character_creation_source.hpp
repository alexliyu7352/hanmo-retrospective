// 自建来源与原身份/家庭事实同存；初货只在尚未发布的新局候选生成一次。
#pragma once
#include "character_creation.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
struct FamilyState;struct FamilyCatalog;struct InteractionState;struct SocialState;
struct CharacterCreationV2Write;
struct CharacterCreationGoods { StableId item_id;Integer quantity=0; };
struct CharacterCreationAppearance { StableId id;AppearanceFeatures features;StableId attire_style; };
struct CharacterCreationCatalog {
    StableId prototype_actor_id,starting_place_id;
    std::vector<CharacterCreationMode> allowed_modes;
    Integer initial_money=0;
    std::vector<CharacterCreationGoods> initial_goods;
    OrderedTable<CharacterCreationAppearance> appearance_choices;
};
struct CharacterCreationFact {
    StableId kind="family_character_creation",actor_id,prototype_actor_id,scenario_id,appearance_choice_id;
    Integer created_tick=0;
    CharacterCreationDraft draft;
    FieldOrder field_order;
};
Outcome<CharacterCreationCatalog> decode_character_creation_catalog(const BoundaryValue& data,const StableId& scenario_id);
Status validate_character_creation_catalog(const CommonStore&,const CommonCatalog&,const FamilyCatalog&,const CharacterCreationCatalog&);
Status validate_character_creation_source(const CommonStore&,const FamilyState&,const FamilyCatalog&,const CommonCatalog&);
// 已完整核准目录的内部只读消费复用同一实际source规则，避免每条profile指纹clone/排序作者列表。
Status validate_character_creation_source_body(const CommonStore&,const FamilyState&,const FamilyCatalog&,const CommonCatalog&);
Outcome<StableId> initialize_character_creation(CommonStore&,FamilyState&,InteractionState&,SocialState&,const FamilyCatalog&,const CommonCatalog&,const CharacterCreationDraft&,const StableId& appearance_choice_id);
// Explicit complete Origin/2 path. The original creation algorithm is shared.
Outcome<StableId> initialize_character_creation_v2(CommonStore&,FamilyState&,InteractionState&,SocialState&,const FamilyCatalog&,const CommonCatalog&,const CharacterCreationDraft&,const StableId& appearance_choice_id,CharacterCreationV2Write&);
bool same_character_creation_draft(const CharacterCreationDraft&,const CharacterCreationDraft&,bool extended);
// 模式由初次创建事实决定，继任不把自由创建的人生改标为普通来源。
Outcome<CharacterCreationMode> family_character_creation_mode(const CommonStore&,const FamilyState&,const FamilyCatalog&,const CommonCatalog&);
Outcome<CharacterCreationMode> family_character_creation_mode_body(const CommonStore&,const FamilyState&,const FamilyCatalog&,const CommonCatalog&);
CharacterCreationFact decode_character_creation_fact(const BoundaryValue&,bool extended);
BoundaryValue encode_character_creation_fact(const CharacterCreationFact&,bool extended);
Status visit_character_creation_fact(CodecVisitor&,CharacterCreationFact&,bool extended);
}
