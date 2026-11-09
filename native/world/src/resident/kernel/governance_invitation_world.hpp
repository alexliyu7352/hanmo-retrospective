// 本次调用持有未包装政治源与真实Task读；关闭账只同步借同候选，不跨保存点留子对象。
#pragma once
#include "resident/domains/politics/governance_replies/political_governance_invitation_replies.hpp"
#include "resident/domains/tasks/governance_task_rules.hpp"
#include <memory>
namespace hanmo::resident {
struct WorldBundle;
class GovernanceInvitationWorldAccess final {
public:
 static Outcome<std::unique_ptr<GovernanceInvitationWorldAccess>> create(const WorldBundle&);
 GovernanceInvitationWorldAccess(const GovernanceInvitationWorldAccess&)=delete;
 GovernanceInvitationWorldAccess& operator=(const GovernanceInvitationWorldAccess&)=delete;
 const GovernanceInvitationReplyContext& context()const{return context_;}
 const GovernanceInvitationReplyRead& replies()const{return replies_;}
private:
 explicit GovernanceInvitationWorldAccess(const WorldBundle&);
 PoliticsGovernanceAccess original_;GovernanceTaskRead tasks_;
 GovernanceInvitationReplyContext context_;GovernanceInvitationReplyAccess replies_;
};
Status validate_governance_invitation_world(const WorldBundle&);
Outcome<GovernanceInvitationReplyQuote> prepare_governance_invitation_world(const WorldBundle&,const StableId& actor,const GovernanceInvitationReplyCommand&);
Outcome<StableId> commit_governance_invitation_world(WorldBundle&,const StableId& actor,const GovernanceInvitationReplyCommand&,const GovernanceInvitationReplyQuote&);
Outcome<BoundaryValue> query_governance_invitation_world(const WorldBundle&,const StableId& place,Integer cursor=0);
}
