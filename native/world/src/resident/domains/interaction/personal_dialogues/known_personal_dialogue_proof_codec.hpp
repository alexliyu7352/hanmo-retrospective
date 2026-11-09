// 领域保存原Known凭据时复用同一字段visitor，不手抄第二份对白/人物序列化。
#pragma once
#include "resident/checkpoint/projection_record_archive.hpp"
#include "known_personal_dialogue_contract.hpp"
namespace hanmo::resident {
namespace dynamic_codec {class JsonArchive;}
Status emit_known_personal_dialogue_proof_fields(ProjectionWriter&,const KnownPersonalDialogueProof&);
Status visit_known_personal_dialogue_proof(DynamicProjectionArchive&,KnownPersonalDialogueProof&);
Status visit_known_personal_dialogue_proof(dynamic_codec::JsonArchive&,KnownPersonalDialogueProof&);
}
