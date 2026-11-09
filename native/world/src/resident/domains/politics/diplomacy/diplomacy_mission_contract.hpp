// 正式使者委任的政治批准只读合同；任务不得用普通亲递提议或自报期限代替批准。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
inline constexpr const char* diplomatic_fact_domain="politics";
inline constexpr const char* diplomatic_proposal_fact_kind="diplomatic_offer_proposed";
inline constexpr const char* diplomatic_mission_authority_fact_kind="diplomatic_mission_authorized";
inline constexpr const char* diplomatic_presentation_fact_kind="diplomatic_offer_presented";
// 所有字段均来自已签发的具名权源及不可变作者政策；不在任务端猜工期、报酬或继任者。
struct DiplomaticMissionApproval {
    WorldIdentity source_identity;
    StableId mission_authority_fact_id,proposal_id,proposal_fact_id,policy_id,rule_name_key;
    Integer terms_revision=0;
    StableId issuer_organization_id,target_organization_id,envoy_actor_id;
    StableId issuer_office_id,issuer_actor_id,issuer_grant_id;
    Integer issuer_authority_epoch=0;
    StableId target_office_id,delivery_site_id,report_office_id,report_site_id,continuity_policy_id;
    Integer issued_tick=0,acceptance_expires_tick=0,delivery_expires_tick=0;
    // 两项只有明确作者配置才可为正；未设不默认补一半日，也不派生额外工资。
    Integer delivery_work_ticks=0,report_work_ticks=0;
    bool return_report_required=false;
};
// 当事人死亡/责任主体解散和临时不在场分开；当前授权变化不能篡改历史受领来源。
enum class DiplomaticMissionContinuityKind { continue_work,pause_envoy,needs_reassignment,terminate_envoy_dead,terminate_principal_dissolved };
struct DiplomaticMissionContinuity {DiplomaticMissionContinuityKind kind=DiplomaticMissionContinuityKind::needs_reassignment;StableId code;std::optional<StableId> cause_fact_id;};
// 完工业务变化是具名结果：接收代表变动需重新会面，责任变化保留既有连续性分类。
enum class DiplomaticTaskPresentationKind { presented,renew_contact,mission_blocked,expired };
struct DiplomaticTaskPresentationAttempt {
    DiplomaticTaskPresentationKind kind=DiplomaticTaskPresentationKind::mission_blocked;
    StableId fact_id,code;
    std::optional<DiplomaticMissionContinuity> continuity;
};
// 只有实际受领和交接Action完成才可建立此绑定；本人零时长亲递保持empty。
struct DiplomaticPresentationTaskBinding {StableId mission_authority_fact_id,task_id,contract_id,action_id,acceptance_fact_id;};
// 任务只读取政治账中实际递交的来源，不复制代表/Grant/接触快照或推断条约接受。
struct DiplomaticPresentationSource {
    StableId fact_id,proposal_id,proposal_fact_id,envoy_actor_id;
    Integer terms_revision=0,completed_tick=0;
    std::optional<DiplomaticPresentationTaskBinding> task_binding;
};
class DiplomacyPresentationRead {
public:
    virtual ~DiplomacyPresentationRead()=default;
    virtual Outcome<DiplomaticPresentationSource> diplomatic_presentation_source(const StableId& fact_id)const=0;
};
// 仅任务/政治组合叶消费；不是开放任意ID的玩家查询。公开受领入口必须首先核本人合法知情。
class DiplomaticMissionRead {
public:
    virtual ~DiplomaticMissionRead()=default;
    // 未知、他人、尚未送达的委任统一DiplomaticMissionNotKnown，之后才核目前有效窗/权限。
    virtual Outcome<DiplomaticMissionApproval> diplomatic_mission_for_acceptance(const StableId& authority_fact_id,const StableId& envoy_actor_id)const=0;
    // 冷历史读取保持原冻结批准，不把今日失权/期限届满当历史不存在。
    virtual Outcome<DiplomaticMissionApproval> diplomatic_mission_source(const StableId& authority_fact_id)const=0;
    virtual Outcome<DiplomaticMissionContinuity> diplomatic_mission_continuity(const StableId& authority_fact_id,const StableId& envoy_actor_id)const=0;
};
}
