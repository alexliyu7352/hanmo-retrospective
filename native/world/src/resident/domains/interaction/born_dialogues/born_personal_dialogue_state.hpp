// 原出生者的私人模板只绑定真实打开的会话，不复制人物、岗位、亲缘或控制权。
#pragma once
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident {
inline constexpr const char* born_personal_dialogue_module="born_personal_dialogues";
inline constexpr const char* born_personal_dialogue_opened_kind="born_personal_dialogue_opened";
struct BornPersonalDialoguePlayerReceipt { StableId result_id,request_actor_id;Integer request_sequence=0,result_revision=0; };
struct BornPersonalDialogueOpening {
    StableId fact_id,template_id,session_id,listener_actor_id,provider_actor_id,dialogue_id,place_id;
    StableId birth_fact_id,creation_fact_id,prototype_id;
    Integer tick=0;
    std::optional<BornPersonalDialoguePlayerReceipt> player_receipt;
};
struct BornPersonalDialogueState { Integer version=1;OrderedTable<BornPersonalDialogueOpening> openings; };
Status visit_born_personal_dialogues_checkpoint(CodecVisitor&,BornPersonalDialogueState&);
BoundaryValue encode_born_personal_dialogues(const BornPersonalDialogueState&);
Outcome<BornPersonalDialogueState> decode_born_personal_dialogues(const BoundaryValue&);
Outcome<BoundaryValue> encode_born_personal_dialogue_fact(const BornPersonalDialogueState&,const StableId&);
}
