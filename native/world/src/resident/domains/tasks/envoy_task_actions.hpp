// 使者交接使用唯一Common Action；报价/开工都重验真实当面会话与政治当前代表资格。
#pragma once
#include "envoy_task_rules.hpp"
#include "resident/domains/interaction/face_to_face_contact.hpp"
namespace hanmo::resident {
// 组合层绑定S6具名mission_contact服务，回调只能证明本次明确责任/接触的政治资格。
using EnvoyContactAuthorization=std::function<Status(const EnvoyTaskAcceptance&,const FaceToFaceContactProof&)>;
struct EnvoyDeliveryQuote {WorldIdentity source_identity;StableId task_id;Integer duration_ticks=0;FaceToFaceContactProof contact;};
Outcome<EnvoyDeliveryQuote> preview_envoy_delivery(const CommonStore&,const TaskState&,const EnvoyTaskState&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const DiplomaticMissionRead&,const DiplomacyPresentationRead&,const StableId& actor_id,const StableId& task_id,const StableId& provider_actor_id,Integer expected_world_revision,const EnvoyContactAuthorization&);
// 此入口要求调用者持整个World候选；政治批准、Task和Common任何失败都整体撤回。
Outcome<StableId> begin_envoy_delivery_in_candidate(CommonStore&,const TaskState&,EnvoyTaskState&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const DiplomaticMissionRead&,const DiplomacyPresentationRead&,const StableId& actor_id,const StableId& task_id,const StableId& provider_actor_id,Integer expected_world_revision,const EnvoyContactAuthorization&);
}
