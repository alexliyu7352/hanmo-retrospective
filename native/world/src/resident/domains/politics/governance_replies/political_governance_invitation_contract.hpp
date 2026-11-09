// 未受领邀请的关闭来源；只限制新的本人接受，不修改已经成立的任务或预算。
#pragma once
#include "resident/model/dynamic_contract.hpp"

namespace hanmo::resident {
enum class GovernanceInvitationReplyKind { declined, withdrawn };

// 只返回真实原邀请的唯一关闭事实，不从到期、计划等待或今天的职权推导一份回复。
struct GovernanceInvitationReplySource {
    WorldIdentity source_identity;
    StableId fact_id, authority_fact_id, actor_id, counterpart_actor_id, site_id;
    GovernanceInvitationReplyKind kind = GovernanceInvitationReplyKind::declined;
    Integer tick = 0;
};

// 调用方须先证明本人知道原邀请；本读口不承担未知邀请的发现或远方状态查询。
class GovernanceInvitationReplyRead {
public:
    virtual ~GovernanceInvitationReplyRead() = default;
    virtual Outcome<std::optional<GovernanceInvitationReplySource>> governance_invitation_reply(
        const StableId& authority_fact_id) const = 0;
};
}
