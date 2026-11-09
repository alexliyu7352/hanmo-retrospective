// 外交提议保存具名签发权源和唯一条款版本；尚未完成双方认可时没有通行效力。
#pragma once
#include "diplomacy_catalog.hpp"
#include "diplomacy_mission_contract.hpp"
#include "diplomacy_v2_state.hpp"
namespace hanmo::resident {
class TaskEnvoyRead;
enum class DiplomaticOfferStatus { awaiting_delivery,presented,accepted_pending_ratification,refused,withdrawn,expired,invalidated };
// 首片本人亲递；正式委派他人的受领、旅费和唯一Task由后继S3接缝承担。
struct DiplomaticOffer {
    StableId id,policy_id,terms_id,issuer_grant_id,envoy_actor_id;
    ActingAuthority issuer;
    Integer issued_tick=0,expires_tick=0,terms_revision=1,revision=1;
    DiplomaticOfferStatus status=DiplomaticOfferStatus::awaiting_delivery;
    StableId last_fact_id,presentation_fact_id,mission_authority_fact_id;
};
// 事实记录谁实际提出/呈递/作答，不能从“接触过”推导对方同意。
struct DiplomaticOfferFact {
    StableId id,offer_id,kind,actor_id,terms_id;
    Integer tick=0,terms_revision=1,offer_revision=1;
    std::vector<StableId> cause_fact_ids,witness_actor_ids;
};
// 只冻结实际呈递瞬间的接触与代表权源；会话后来关闭不会抹去真实历史。
struct DiplomaticPresentation {
    StableId id,offer_id,proposal_fact_id,terms_id,envoy_actor_id,representative_grant_id;
    ActingAuthority representative;
    Integer tick=0,terms_revision=1,offer_revision=0;
    StableId session_actor_id,session_id,place_id,provider_actor_id,role_id;
    std::optional<StableId> context_kind;
    std::optional<DiplomaticPresentationTaskBinding> task_binding;
};
// 明确回复只说明对原已亲递条款的决定；接受之后仍需双方具名认可才产生许可。
enum class DiplomaticReplyKind { accept,refuse };
struct DiplomaticReply {
    StableId id,offer_id,presentation_fact_id,terms_id,grant_id;
    ActingAuthority authority;
    DiplomaticReplyKind decision=DiplomaticReplyKind::refuse;
    Integer tick=0,terms_revision=1,offer_revision=0;
    StableId session_actor_id,session_id,place_id,provider_actor_id,role_id;
    std::optional<StableId> context_kind;
};
struct DiplomacyState {
    Integer version=1;
    OrderedTable<DiplomaticOffer> offers;
    OrderedTable<DiplomaticOfferFact> facts;
    OrderedTable<DiplomaticPresentation> presentations;
    OrderedTable<DiplomaticReply> replies;
    OrderedTable<DiplomaticMissionApproval> missions;
    std::optional<DiplomacyV2State> v2;
};
// 历史权源读取无需当年的活动会话仍存在。
struct DiplomaticHistoryRead {
    const CommonStore& common; const OrganizationState& organizations; const OrganizationCatalog& organization_catalog;
    const DiplomacyCatalog& catalog; const DiplomacyState& state;
};
// 玩家只选作者准许条款；不能提交任意期限、目标组织、授权或已同意标记。
struct DiplomaticProposeCommand {ActingAuthority authority;StableId policy_id,terms_id;};
struct DiplomaticProposeQuote {WorldIdentity source;DiplomaticOffer proposal;};
// 首片使者就是本次真实签发本人；授权提议和本人承担递交仍不代表目标组织接受。
Outcome<DiplomaticProposeQuote> preview_diplomatic_proposal(const CommonStore&,const CommonCatalog&,const OrganizationState&,const OrganizationCatalog&,const DiplomacyCatalog&,const ActivityRead&,const StableId& actor,const DiplomaticProposeCommand&);
Outcome<StableId> propose_diplomatic_offer_in_transaction(CommonStore&,const CommonCatalog&,const OrganizationState&,const OrganizationCatalog&,const DiplomacyCatalog&,DiplomacyState&,const ActivityRead&,const StableId& actor,const DiplomaticProposeCommand&);
// 签发后、呈递/答复前重验当前专项权源；旧事实仍保留历史，不抹掉原签发。
Status revalidate_diplomatic_issuer(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const DiplomacyCatalog&,const DiplomaticOffer&);
// 严格冷边界只重建历史，不续期限、不改授权、不重新呈递。
Outcome<DiplomacyState> decode_diplomacy_state(const BoundaryValue&);
BoundaryValue encode_diplomacy_state(const DiplomacyState&);
Status visit_diplomacy_checkpoint(CodecVisitor&,DiplomacyState&);
Status validate_diplomacy_state(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const DiplomacyCatalog&,const DiplomacyState&,const TaskEnvoyRead* envoy_tasks=nullptr);
}
