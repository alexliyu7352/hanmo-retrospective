// 谢绝与撤回各由原本人当面办理；首片不接NPC写入，也不释放已受领Task的任何预约。
#pragma once
#include "political_governance_invitation_contract.hpp"
#include "political_governance_invitation_control.hpp"
#include "resident/domains/politics/politics_governance.hpp"
#include "resident/domains/tasks/governance_task_contract.hpp"
#include "resident/domains/family/family_death.hpp"
#include "hanmo/resident/command_contract.hpp"

namespace hanmo::resident {
inline constexpr std::uint64_t governance_invitation_reply_feature = 268435456u;
inline constexpr const char* governance_invitation_reply_module = "political_governance_invitation_replies";
inline constexpr const char* governance_invitation_reply_prefix = "governance_invitation_reply";
inline constexpr const char* governance_invitation_reply_fact_kind = "political_governance_invitation_closed";
inline constexpr const char* governance_decline_command = "governance_decline";
inline constexpr const char* governance_withdraw_command = "political_withdraw_governance_invitation";

// 精确两个命令的识别不接受宽political前缀。
inline bool is_governance_invitation_reply_command(const StableId& kind){return kind==governance_decline_command||kind==governance_withdraw_command;}

// 谢绝只选本人已知原邀请；撤回还须原签发者当前持有原署的明确专项命令许可。
struct GovernanceDeclineInvitation { StableId authority_fact_id; };
struct PoliticalWithdrawGovernanceInvitation { StableId authority_fact_id; ActingAuthority authority; };
using GovernanceInvitationReplyCommand = std::variant<GovernanceDeclineInvitation, PoliticalWithdrawGovernanceInvitation>;

// 永久真实提交来源只在Common分配实际result并写入cache/history之后绑定。
struct GovernanceInvitationReplyPlayerReceipt {
    StableId result_id, actor_id;
    Integer request_sequence = 0, result_revision = 0;
};

// 原批准保留在旧政治账；本关闭事实见证writer已核当场原双方/site。
// 本版不重放所有旧行踪/占用；冷读独立核真实生命、原权源及当时本人控制。
struct GovernanceInvitationReplyRecord {
    StableId id, authority_fact_id, actor_id, counterpart_actor_id, site_id, grant_id;
    GovernanceInvitationReplyKind kind = GovernanceInvitationReplyKind::declined;
    Integer tick = 0;
    std::optional<GovernanceInvitationReplyPlayerReceipt> player_receipt;
};
struct GovernanceInvitationReplyState {
    Integer version = 1;
    OrderedTable<GovernanceInvitationReplyRecord> replies;
};

// originals必须为未包装的原PoliticsGovernanceAccess；Task只读和生命读同步借同一候选。
struct GovernanceInvitationReplyContext {
    const CommonStore& common;
    const CommonCatalog& common_catalog;
    const OrganizationState& organizations;
    const OrganizationCatalog& organization_catalog;
    const PoliticsGovernanceState& governance;
    const GovernanceAuthorityRead& originals;
    const TaskGovernanceRead& tasks;
    const ActivityRead& activity;
    const GovernanceInvitationReplyState& state;
    FamilyDeathOriginRead death_origin;
    GovernanceInvitationPlayerHistoryRead player_before_fact;
};
struct GovernanceInvitationReplyQuote {
    WorldIdentity identity;
    GovernanceInvitationReplyCommand command;
    GovernanceInvitationReplyRecord source;
};

// 显式新段统一新局、目录、标题与完整存档的依赖和状态形状。
Status validate_governance_invitation_reply_modules(const std::vector<std::pair<StableId,StableId>>&,bool enabled);
Status validate_governance_invitation_reply_layout(const CommonStore&,const std::vector<std::pair<StableId,StableId>>&,bool enabled,const GovernanceInvitationReplyState*);
Outcome<bool> decode_governance_invitation_reply_profile(const BoundaryValue& scenario,const std::vector<StableId>& capabilities);
Status validate_governance_invitation_reply_fact_header(const CommonStore&,const GovernanceInvitationReplyState&,const FactRecord&);
BoundaryValue encode_governance_invitation_reply_quote(const GovernanceInvitationReplyQuote&);
Outcome<BoundaryValue> query_governance_invitation_replies(const GovernanceInvitationReplyContext&,const StableId& place,Integer cursor=0);
Status validate_governance_invitation_replies(const GovernanceInvitationReplyContext&);
// 此口要求调用方已核本人知情；已接受后即使Task取消，也不返回可关闭的原邀请。
Outcome<GovernanceAuthorityApproval> read_unaccepted_governance_invitation(const GovernanceInvitationReplyContext&, const StableId&);
Outcome<GovernanceInvitationReplyQuote> preview_governance_invitation_reply(
    const GovernanceInvitationReplyContext&, const StableId& actor, const GovernanceInvitationReplyCommand&);
Outcome<StableId> apply_governance_invitation_reply_in_candidate(
    CommonStore&, GovernanceInvitationReplyState&, const GovernanceInvitationReplyContext&,
    const StableId& actor, const GovernanceInvitationReplyCommand&, const GovernanceInvitationReplyQuote&);

// 本账先完整核原真实批准与未受领边界，再提供唯一关闭源；不会反向调用接受包装器。
class GovernanceInvitationReplyAccess final : public GovernanceInvitationReplyRead {
public:
    explicit GovernanceInvitationReplyAccess(GovernanceInvitationReplyContext context) : context_(std::move(context)) {}
    Outcome<std::optional<GovernanceInvitationReplySource>> governance_invitation_reply(const StableId&) const override;
private:
    GovernanceInvitationReplyContext context_;
};
// 明确新profile才使用此包装器；原批准、工作、赈济连续性原样委托。
class GovernanceInvitationAcceptanceAccess final : public GovernanceAuthorityRead {
public:
    GovernanceInvitationAcceptanceAccess(const CommonStore& common,const GovernanceAuthorityRead& original,
        const GovernanceInvitationReplyRead& replies):common_(common),original_(original),replies_(replies){}
    Outcome<GovernanceAuthorityApproval> governance_authority_approval(const StableId&)const override;
    Outcome<GovernanceAuthorityApproval> governance_authority_for_acceptance(const StableId&,const StableId&,Integer)const override;
    Outcome<GovernanceContinuityResult> governance_continuity(const StableId&,const StableId&)const override;
    Outcome<GovernanceContinuityResult> governance_relief_continuity(const StableId&,const StableId&,const StableId&,const PoliticalTakeoverRead&)const override;
private:
    const CommonStore& common_;const GovernanceAuthorityRead& original_;const GovernanceInvitationReplyRead& replies_;
};
// 只挑本次可信writer的唯一新行；真实Common提交结果分配后才可永久绑定。
Outcome<StableId> governance_invitation_new_player_source(const GovernanceInvitationReplyState& before,
    const GovernanceInvitationReplyState& after,const StableId& actor,const StableId& command_kind,Integer tick);
Status bind_governance_invitation_player_result(const CommonStore&,GovernanceInvitationReplyState&,
    const StableId& source,const StableId& actor,Integer request,const CommitResult&,
    const std::function<bool(const CommitResult&)>& is_done);
Status validate_governance_invitation_player_results(const CommonStore&,const GovernanceInvitationReplyState&,
    const std::function<bool(const CommitResult&)>& is_done);
BoundaryValue encode_governance_invitation_replies(const GovernanceInvitationReplyState&);
Outcome<GovernanceInvitationReplyState> decode_governance_invitation_replies(const BoundaryValue&);
Status visit_governance_invitation_replies_checkpoint(CodecVisitor&, GovernanceInvitationReplyState&);
BoundaryValue encode_governance_invitation_reply_command(const GovernanceInvitationReplyCommand&);
Outcome<GovernanceInvitationReplyCommand> decode_governance_invitation_reply_command(const StableId& kind, const BoundaryValue&);
StableId governance_invitation_reply_command_kind(const GovernanceInvitationReplyCommand&);
}
