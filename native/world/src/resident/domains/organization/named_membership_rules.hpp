// 具名成员生命周期入口；由正式邀请答复及本人响应命令调用。
#pragma once
#include "organization_model.hpp"
#include "named_membership_model.hpp"
#include "resident/domains/local_economy/membership_wages.hpp"
namespace hanmo::resident {
struct MembershipOfficeTransitionWriteContext;
class OrganizationAuthorityClock;
struct NamedMembershipJoinEffects { StableId member_episode_id,duty_id,wage_binding_id,obligation_id; std::vector<StableId> fact_ids,receipt_ids; };
// 只选唯一非departed episode，历史离职记录不会抢先遮住新任职。
const OrgMemberState* active_membership_episode(const OrganizationState&,const StableId& actor_id,const StableId& organization_id);
// 答复前复验单一有效episode、真实责任及首期资金；邀请权限/知情由offer owner先验。
Status preview_named_membership_join(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const LocalEconomyCatalog&,const MembershipReleaseRead&,const StableId& offer_id,Integer tick,const OrganizationAuthorityClock* =nullptr);
// 调用者已在同一候选写入一次accept决定；此入口建立唯一成员/职责/工资并消费原邀请。
Outcome<NamedMembershipJoinEffects> join_named_membership(CommonStore&,OrganizationState&,const OrganizationCatalog&,LocalEconomyState&,const LocalEconomyCatalog&,const MembershipReleaseRead&,const StableId& offer_id,const StableId& decision_id,Integer tick,const OrganizationAuthorityClock* =nullptr);
// 新profile预览允许无欠薪的合法辞任；不把待收款视为人身占用。
Status preview_named_membership_member_command(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const LocalEconomyState&,const MembershipReleaseRead&,const OrganizationMemberCommand&,Integer tick,const MembershipOfficeTransitionWriteContext* =nullptr);
// 新profile本人欠薪响应由Director或玩家唯一提出；释放必须读取真实责任owner。
Outcome<OrganizationEffects> apply_named_membership_member_command(CommonStore&,OrganizationState&,const OrganizationCatalog&,LocalEconomyState&,const LocalEconomyCatalog&,const ActivityRead&,const MembershipReleaseRead&,const OrganizationMemberCommand&,Integer tick,const MembershipOfficeTransitionWriteContext* =nullptr);
// 仅处理已经提出的awaiting_release；不自行读全局欠额发起个人决定。
Outcome<OrganizationEffects> reconcile_named_membership(CommonStore&,OrganizationState&,const OrganizationCatalog&,LocalEconomyState&,const LocalEconomyCatalog&,const ActivityRead&,const MembershipReleaseRead&,Integer tick,const MembershipOfficeTransitionWriteContext* =nullptr);
} // 所有共同事实、成员及经济写入均在候选保存点内提交。
