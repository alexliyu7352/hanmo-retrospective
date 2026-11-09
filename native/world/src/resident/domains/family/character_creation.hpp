// 自建草案的有界纯规则；不分配人物/财物，不消费随机，不修改已有剧本人物。
#pragma once
#include "resident/storage/common_codec.hpp"

namespace hanmo::resident {
enum class CharacterCreationMode { standard,free };
struct CharacterCreationDraft {
    CharacterCreationMode mode=CharacterCreationMode::standard;
    StableId display_name;
    Integer age_years=18;
    ActorAttributes attributes;
    ActorSkills skills;
    FieldOrder field_order,attributes_order,skills_order;
};
struct CharacterCreationBudget {
    Integer attribute_total=0,skill_level_total=0;
    bool attribute_budget_complete=false,skill_budget_complete=false;
};
// extended 必须来自所选情景目录，客户端不能靠提交多六项私自升级规则。
Outcome<CharacterCreationBudget> inspect_character_creation_budget(const CharacterCreationDraft&,bool extended);
Status validate_character_creation_draft(const CharacterCreationDraft&,bool extended);
Outcome<CharacterCreationDraft> decode_character_creation_draft(const BoundaryValue&,bool extended);
BoundaryValue encode_character_creation_draft(const CharacterCreationDraft&,bool extended);
StableId character_creation_mode_name(CharacterCreationMode);
}
