// 接触证明从当前真实会话派生；不因两人同地就补造送达，也不决定任何人的同意。
#include "face_to_face_contact.hpp"
#include "resident/storage/common_store.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
namespace hanmo::resident {
namespace {
// 世界替换、推进或任何提交都会使旧证明失效，必须重新读取当面状态。
bool same_source(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
// 只比原上下文有业务意义的typed字段，冷字段排列不是另一份接触权源。
bool same_context(const InteractionContext& a,const InteractionContext& b){return a.place_id==b.place_id&&a.provider_actor_id==b.provider_actor_id&&a.role_id==b.role_id&&a.kind==b.kind&&a.encounter_id==b.encounter_id&&a.input_error==b.input_error;}
}
// 仅支持岗位当面或无岗位的叙事当面；两者都不能夹带道路遭遇、空来源或解码错误。
bool face_to_face_contact_context_valid(const InteractionContext& context){
    if(context.place_id.empty()||context.provider_actor_id.empty()||context.encounter_id||context.input_error)return false;
    return context.kind?*context.kind=="narrative"&&context.role_id.empty():!context.role_id.empty();
}
static Outcome<FaceToFaceContactProof> preview_contact_owned(const CommonStore& common,const InteractionState& state,const InteractionCatalog& catalog,const InteractionAccess& access,const StableId& actor,const StableId& target,CheckpointMemoryCharge* memory){
    if(actor.empty()||target.empty()||actor==target)return {false,{},common_failure("InvalidContactParticipants").error};const InteractionSession* meeting=nullptr;
    for(const auto& session:state.interactions){const bool direct=session.actor_id==actor&&session.context.provider_actor_id==target;const bool reply=session.actor_id==target&&session.context.provider_actor_id==actor;if((!direct&&!reply)||(session.phase!="say"&&session.phase!="choice"))continue;if(meeting)return {false,{},common_failure("AmbiguousInteractionContact",{},ErrorKind::internal).error};meeting=&session;}
    if(!meeting)return {false,{},common_failure("FaceToFaceContactRequired").error};
    if(meeting->id.empty()||!face_to_face_contact_context_valid(meeting->context))return {false,{},common_failure("UnsupportedFaceToFaceContactContext").error};
    // 原validator成功返回ctx的一次fresh copy与proof四ID都在真正复制前预留。
    if(memory){const auto& c=meeting->context;(*memory).grow(0);}
    auto checked=validate_interaction(common,state,catalog,access,meeting->actor_id,meeting->context);if(!checked.ok)return {false,{},checked.error};
    return {true,{common.state().identity,actor,target,meeting->actor_id,meeting->id,std::move(checked.value)},{}};
}
// 旧口保原业务；新口只增加真实返回字段的预先收费与外部拥有寿命。
Outcome<FaceToFaceContactProof> preview_face_to_face_contact(const CommonStore& c,const InteractionState& s,const InteractionCatalog& cat,const InteractionAccess& a,const StableId& actor,const StableId& target){return preview_contact_owned(c,s,cat,a,actor,target,nullptr);}
Outcome<FaceToFaceContactProof> preview_face_to_face_contact(const CommonStore& c,const InteractionState& s,const InteractionCatalog& cat,const InteractionAccess& a,const StableId& actor,const StableId& target,CheckpointMemoryCharge& memory){return preview_contact_owned(c,s,cat,a,actor,target,&memory);}
Status revalidate_face_to_face_contact(const CommonStore& common,const InteractionState& state,const InteractionCatalog& catalog,const InteractionAccess& access,const FaceToFaceContactProof& proof){
    if(!same_source(proof.source_identity,common.state().identity))return common_failure("StaleContactProof");
    CheckpointMemoryCharge fresh_memory;auto fresh=preview_face_to_face_contact(common,state,catalog,access,proof.actor_id,proof.target_actor_id,fresh_memory);if(!fresh.ok)return {false,{},fresh.error};const auto& actual=fresh.value;
    if(actual.session_actor_id!=proof.session_actor_id||actual.session_id!=proof.session_id||!same_context(actual.context,proof.context))return common_failure("StaleContactProof");return common_success();
}
}
