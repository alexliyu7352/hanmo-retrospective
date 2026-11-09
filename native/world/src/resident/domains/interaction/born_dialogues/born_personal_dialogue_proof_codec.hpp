#pragma once
#include "born_personal_dialogue_contract.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
namespace hanmo::resident {
namespace dynamic_codec {class JsonArchive;}
Status emit_born_personal_dialogue_proof_fields(ProjectionWriter&,const BornPersonalDialogueProof&);
Status visit_born_personal_dialogue_proof(DynamicProjectionArchive&,BornPersonalDialogueProof&);
Status visit_born_personal_dialogue_proof(dynamic_codec::JsonArchive&,BornPersonalDialogueProof&);
}
