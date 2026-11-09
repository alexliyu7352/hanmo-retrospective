// 当面接触只是当前原会话的只读证明；不代表意愿、同意、传闻传播或已经写入的送达事实。
#pragma once
#include "interaction_model.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
namespace hanmo::resident {
// actor可为当前会话的任一实际参与方；session_actor明确原会话归属，不能据此冒充NPC意愿。
struct FaceToFaceContactProof {
    WorldIdentity source_identity;
    StableId actor_id,target_actor_id,session_actor_id,session_id;
    InteractionContext context;
};
// 热证明与历史快照共用纯形状约束；不重查过去的在场，也不证明送达、意愿或同意。
bool face_to_face_contact_context_valid(const InteractionContext&);
// 仅取真实say/choice场所会面，再走原validate_interaction；没有原会话或远程场景明确拒绝。
Outcome<FaceToFaceContactProof> preview_face_to_face_contact(const CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const StableId& actor_id,const StableId& target_actor_id);
// 新job调用者在返回proof真正销毁前持该owner；业务核验与旧口共用原实现。
Outcome<FaceToFaceContactProof> preview_face_to_face_contact(const CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const StableId& actor_id,const StableId& target_actor_id,CheckpointMemoryCharge&);
// 成功领域命令写入送达快照前再核完整世界来源、双方和原会话；只读，不自建消息钟或事实账。
Status revalidate_face_to_face_contact(const CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const FaceToFaceContactProof&);
}
