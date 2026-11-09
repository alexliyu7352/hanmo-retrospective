#pragma once
#include "face_to_face_contact.hpp"
#include "resident/kernel/source_ref.hpp"
namespace hanmo::resident {
inline constexpr const char* face_to_face_contact_source_kind="face_to_face_contact";
// Only a successful original interaction writer calls this after its real node pump.
// preview/read never create a receipt, advance a session, or change knowledge.
Outcome<std::optional<SourceRef>> publish_face_to_face_contact_in_candidate(CommonStore&,InteractionState&,const InteractionCatalog&,const InteractionAccess&,const StableId&session_actor,const WorldMoment&actual);
Outcome<const FaceToFaceContactResult*> read_face_to_face_contact_source(const CommonStore&,const InteractionState&,const InteractionCatalog&,const SourceRef&,Integer schema=1);
Outcome<SourceRef> read_current_face_to_face_contact_source(const CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const StableId&actor,const StableId&target,const WorldMoment&actual);
// 返回SourceRef的普通字符串预留由调用者持有到该值销毁；内部proof另持临时费用。
Outcome<SourceRef> read_current_face_to_face_contact_source(const CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const StableId&actor,const StableId&target,const WorldMoment&actual,CheckpointMemoryCharge&);
Status validate_face_to_face_contact_sources(const CommonStore&,const InteractionState&,const InteractionCatalog&,const WorldMoment&actual);
Status visit_face_to_face_contact_sources_checkpoint(CodecVisitor&,FaceToFaceContactSources&);
Outcome<FaceToFaceContactSources> decode_face_to_face_contact_sources(const BoundaryValue&);
Outcome<BoundaryValue> encode_face_to_face_contact_sources(const FaceToFaceContactSources&);
class ProjectionWriter;
Status emit_face_to_face_contact_sources_projection(ProjectionWriter&,std::string_view,const FaceToFaceContactSources&);
}
