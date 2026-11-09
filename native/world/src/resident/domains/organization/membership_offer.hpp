// 具名邀约只发出冻结条款；本人答复调用唯一成员与工资owner，不直接改组织归属。
#pragma once
#include "organization_model.hpp"
#include "named_membership_model.hpp"
#include "resident/domains/interaction/interaction_model.hpp"
namespace hanmo::resident {
struct SocialState;
class OrganizationAuthorityClock;
// authority必须与中央命令主体相同；context只标明实际当面接洽，不携带授权证明。
struct OrganizationOfferMembershipCommand {
    ActingAuthority authority;
    StableId target_actor_id,terms_id;
    InteractionContext context;
};
// 玩家和NPC共用本人命令；expected版本和已知邀请事实禁止旧按钮偷偷换约。
struct OrganizationAnswerMembershipCommand {
    StableId actor_id,offer_id;
    Integer expected_offer_revision=0;
    MembershipDecisionKind answer=MembershipDecisionKind::defer;
    StableId source_fact_id;
};
// 只有本组织当前仍有完整招揽权限的签发者可撤回未决邀请。
struct OrganizationWithdrawMembershipCommand {
    ActingAuthority authority;
    StableId offer_id;
    Integer expected_offer_revision=0;
};
// 报价保留实际选中的单一grant，公开DTO只投影条款与期限，不泄露公库余额。
struct MembershipOfferQuote {
    Integer duration=0,offer_revision=0,expires_tick=0,next_review_tick=0,first_period_due=0;
    StableId grant_id,terms_id,offer_id,reason;
};
struct MembershipOfferEffects {
    StableId offer_id,decision_id,member_episode_id;
    std::vector<StableId> fact_ids,receipt_ids;
};
// 邀请来源必须由原Social报告证明本人已知；无报告或未来知识明确拒绝。
bool membership_fact_known(const CommonStore&,const SocialState&,const StableId& actor_id,const StableId& fact_id);
// 同一grant同时覆盖职责、场所、钱包、首期额度；required_grant固定原邀约授权来源。
Outcome<MembershipOfferQuote> check_membership_offer_authority(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const LocalEconomyCatalog&,const ActingAuthority&,const MembershipTerms&,const StableId& required_grant={},const OrganizationAuthorityClock* =nullptr);
Outcome<MembershipOfferQuote> preview_organization_offer_membership(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const LocalEconomyCatalog&,const SocialState&,const InteractionState&,const ActivityRead&,const StableId& command_actor,const OrganizationOfferMembershipCommand&,const OrganizationAuthorityClock* =nullptr);
Outcome<MembershipOfferQuote> preview_organization_answer_membership(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const LocalEconomyState&,const LocalEconomyCatalog&,const SocialState&,const InteractionState&,const ActivityRead&,const MembershipReleaseRead&,const StableId& command_actor,const OrganizationAnswerMembershipCommand&,const OrganizationAuthorityClock* =nullptr);
Outcome<MembershipOfferQuote> preview_organization_withdraw_membership(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const LocalEconomyCatalog&,const StableId& command_actor,const OrganizationWithdrawMembershipCommand&,const OrganizationAuthorityClock* =nullptr);
// 中央候选包含Common/Organization/LocalEconomy/Social/Director；任何晚失败整体回滚。
Outcome<MembershipOfferEffects> apply_organization_offer_membership(CommonStore&,OrganizationState&,const OrganizationCatalog&,const LocalEconomyCatalog&,const SocialState&,const InteractionState&,const ActivityRead&,const StableId& command_actor,const OrganizationOfferMembershipCommand&,const OrganizationAuthorityClock* =nullptr);
Outcome<MembershipOfferEffects> apply_organization_answer_membership(CommonStore&,OrganizationState&,const OrganizationCatalog&,LocalEconomyState&,const LocalEconomyCatalog&,const SocialState&,const InteractionState&,const ActivityRead&,const MembershipReleaseRead&,const StableId& command_actor,const OrganizationAnswerMembershipCommand&,const OrganizationAuthorityClock* =nullptr);
Outcome<MembershipOfferEffects> apply_organization_withdraw_membership(CommonStore&,OrganizationState&,const OrganizationCatalog&,const LocalEconomyCatalog&,const StableId& command_actor,const OrganizationWithdrawMembershipCommand&,const OrganizationAuthorityClock* =nullptr);
// 到期只形成唯一终态事实；不是代替本人作出拒绝或接受。
Outcome<MembershipOfferEffects> expire_membership_offers(CommonStore&,OrganizationState&,const OrganizationCatalog&,Integer tick,const OrganizationAuthorityClock* =nullptr);
// 参与者报告只投影邀请/决定的真实来源，不公开工资数值或私有余额。
std::optional<DynamicFactView> membership_offer_dynamic_fact(const OrganizationState&,const StableId& fact_id);
}
