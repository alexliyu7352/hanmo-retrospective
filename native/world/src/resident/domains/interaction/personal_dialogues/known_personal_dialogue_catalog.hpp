// 模板只绑定既存人物或实际出生原型与原无岗位对白，不提供可执行脚本或临时role。
#pragma once
#include "known_personal_dialogue_contract.hpp"
#include "resident/domains/interaction/interaction_model.hpp"
namespace hanmo::resident {
struct FamilyCatalog;struct BirthArrangementCatalog;
const KnownPersonalDialogueTemplate* known_personal_dialogue_template(const KnownPersonalDialogueCatalog&,const StableId&);
Status validate_known_personal_dialogue_catalog(const KnownPersonalDialogueCatalog&,const InteractionCatalog&,const FamilyCatalog&,const BirthArrangementCatalog&);
Outcome<KnownPersonalDialogueCatalog> decode_known_personal_dialogue_catalog(const BoundaryValue&,const InteractionCatalog&,const FamilyCatalog&,const BirthArrangementCatalog&);
BoundaryValue encode_known_personal_dialogue_catalog(const KnownPersonalDialogueCatalog&);
}
