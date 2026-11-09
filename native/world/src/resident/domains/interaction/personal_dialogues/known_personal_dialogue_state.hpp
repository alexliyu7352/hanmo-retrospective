// 仅保存原真实session的打开来源和实际提交结果；不复制对白进度、人物或关系账。
#pragma once
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident {
inline constexpr const char* known_personal_dialogue_module="known_personal_dialogues";
inline constexpr const char* known_personal_dialogue_opened_kind="known_personal_dialogue_opened";
// 永久投影不保存今日revision或今日父母关系；Born来源只保同一个真实出生/创建身份。
struct KnownPersonalActorOrigin {
    StableId kind,actor_id,birth_fact_id,creation_fact_id,prototype_id;
};

struct KnownPersonalDialoguePlayerReceipt { StableId result_id,request_actor_id;Integer request_sequence=0,result_revision=0; };
struct KnownPersonalDialogueOpening {
    StableId fact_id,template_id,session_id,dialogue_id,place_id;
    KnownPersonalActorOrigin listener_origin,provider_origin;
    Integer tick=0;
    std::optional<KnownPersonalDialoguePlayerReceipt> player_receipt;
};
struct KnownPersonalDialogueState { Integer version=1;OrderedTable<KnownPersonalDialogueOpening> openings; };
Status visit_known_personal_dialogues_checkpoint(CodecVisitor&,KnownPersonalDialogueState&);
BoundaryValue encode_known_personal_dialogues(const KnownPersonalDialogueState&);
Outcome<KnownPersonalDialogueState> decode_known_personal_dialogues(const BoundaryValue&);
Outcome<BoundaryValue> encode_known_personal_dialogue_fact(const KnownPersonalDialogueState&,const StableId&);
}
