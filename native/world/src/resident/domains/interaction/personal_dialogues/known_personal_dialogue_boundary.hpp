// 独立段；静态两端依原Life，出生端点另需Birth/Lineage。旧Interaction字节不变。
#pragma once
#include "known_personal_dialogue.hpp"
#include "known_personal_dialogue_receipts.hpp"
namespace hanmo::resident {
Outcome<OpenKnownPersonalDialogue> decode_known_personal_dialogue_command(const StableId&,const BoundaryValue&);
BoundaryValue encode_known_personal_dialogue_command(const OpenKnownPersonalDialogue&);
BoundaryValue encode_known_personal_dialogue_quote(const KnownPersonalDialogueQuote&);
bool known_personal_dialogue_needs_birth(const KnownPersonalDialogueCatalog&);
Outcome<bool> known_personal_dialogue_selected(const std::vector<std::pair<StableId,StableId>>&);
Outcome<bool> decode_known_personal_dialogue_profile(const BoundaryValue& scenario,const std::vector<StableId>& capabilities);
Status validate_known_personal_dialogue_layout(const CommonStore&,const std::vector<std::pair<StableId,StableId>>&,const KnownPersonalDialogueCatalog*,const KnownPersonalDialogueState*);
Outcome<BoundaryValue> known_personal_dialogue_fact_payloads(const CommonStore&,const KnownPersonalDialogueState*);
Status validate_known_personal_dialogue_json_payloads(const CommonStore&,const KnownPersonalDialogueState*,const BoundaryValue&);
}
