// 动态委托的暂拒只记本人真实知情和当面回应；不会认领Need、占款或改变关系。
#pragma once
#include "resident/domains/interaction/face_to_face_contact.hpp"
namespace hanmo::resident {
struct TaskState;struct TaskCommand;struct SocialState;struct AffairsState;struct KnownSubjectView;struct BoundaryValue;
// 历史只保实际报告引用；local_public表示当时在发布地直接得知，不补造已送达消息。
struct RuntimeOfferResponseSource {
    StableId report_id,fact_id,source_key;
    Integer observation_tick=0,learned_tick=0;
    bool local_public=false;
};
struct RuntimeTaskOfferResponse {
    StableId actor_id,offer_id,need_id,affair_id,terms_version,session_id;
    Integer need_revision=0,response_tick=0;
    InteractionContext context;
    RuntimeOfferResponseSource source;
};
// 拟嵌入唯一task_offer_responses/2的显式子载荷；自身不是第二任务账或自由启用模块。
struct RuntimeTaskOfferResponses {std::vector<RuntimeTaskOfferResponse> responses;};
struct RuntimeTaskOfferResponseQuote {
    WorldIdentity source_identity;
    FaceToFaceContactProof contact;
    RuntimeTaskOfferResponse response;
};
const RuntimeTaskOfferResponse* runtime_task_offer_response(const RuntimeTaskOfferResponses&,const StableId& actor,const StableId& offer);
// 具名decline_runtime_offer复用TaskCommand：runtime引用＋当前Need版本，暂拒不占quantity。
Outcome<RuntimeTaskOfferResponseQuote> preview_runtime_task_offer_decline(
    const CommonStore&,const TaskState&,const AffairsState&,const SocialState&,
    const InteractionState&,const InteractionCatalog&,const InteractionAccess&,
    const KnownSubjectView&,const StableId& actor,const TaskCommand&);
Status record_runtime_task_offer_decline(
    const CommonStore&,const TaskState&,const AffairsState&,const SocialState&,
    const InteractionState&,const InteractionCatalog&,const InteractionAccess&,
    const KnownSubjectView&,const StableId& actor,const TaskCommand&,
    const RuntimeTaskOfferResponseQuote&,RuntimeTaskOfferResponses&);
// 冷验证不要求旧会话或旧provider仍在职，但原报告/当时可知范围/绑定不得被替换。
Status validate_runtime_task_offer_responses(const CommonStore&,const TaskState&,const AffairsState&,
    const SocialState&,const InteractionCatalog&,const RuntimeTaskOfferResponses&);
Status visit_runtime_task_offer_responses(CodecVisitor&,RuntimeTaskOfferResponses&);
Outcome<RuntimeTaskOfferResponses> decode_runtime_task_offer_responses(const BoundaryValue&);
BoundaryValue encode_runtime_task_offer_responses(const RuntimeTaskOfferResponses&);
}
