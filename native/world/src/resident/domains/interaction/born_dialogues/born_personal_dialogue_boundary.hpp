// 独立版本与原Birth/Lineage/Life同启用，旧Interaction段及空目录均不增加字段。
#pragma once
#include "born_personal_dialogue.hpp"
#include "born_personal_dialogue_receipts.hpp"
namespace hanmo::resident {
Outcome<OpenBornPersonalDialogue> decode_born_personal_dialogue_command(const StableId&,const BoundaryValue&);
BoundaryValue encode_born_personal_dialogue_command(const OpenBornPersonalDialogue&);
BoundaryValue encode_born_personal_dialogue_quote(const BornPersonalDialogueQuote&);
Outcome<bool> decode_born_personal_dialogue_profile(const BoundaryValue& scenario,const std::vector<StableId>& capabilities);
Status validate_born_personal_dialogue_layout(const CommonStore&,const std::vector<std::pair<StableId,StableId>>&,const BornPersonalDialogueCatalog*,const BornPersonalDialogueState*);
Outcome<BoundaryValue> born_personal_dialogue_fact_payloads(const CommonStore&,const BornPersonalDialogueState*);
Status validate_born_personal_dialogue_json_payloads(const CommonStore&,const BornPersonalDialogueState*,const BoundaryValue&);
}
