// 作者绑定原出生prototype与无岗位对白；每次打开仍读取实际出生者，不把prototype当本人。
#pragma once
#include "resident/domains/interaction/interaction_model.hpp"
#include "born_personal_dialogue_state.hpp"
namespace hanmo::resident {
struct FamilyCatalog;struct BirthArrangementCatalog;
struct BornPersonalDialogueTemplate { StableId id,prototype_id,dialogue_id; };
struct BornPersonalDialogueCatalog { Integer version=1;std::vector<BornPersonalDialogueTemplate> templates; };
Status validate_born_personal_dialogue_catalog(const BornPersonalDialogueCatalog&,const InteractionCatalog&);
Status validate_born_personal_dialogue_birth_catalog(const BornPersonalDialogueCatalog&,const FamilyCatalog&,const BirthArrangementCatalog&);
Outcome<BornPersonalDialogueCatalog> decode_born_personal_dialogue_catalog(const BoundaryValue&,const InteractionCatalog&,const FamilyCatalog&,const BirthArrangementCatalog&);
BoundaryValue encode_born_personal_dialogue_catalog(const BornPersonalDialogueCatalog&);
}
