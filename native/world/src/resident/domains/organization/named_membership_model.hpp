// 具名邀请与真实成员episode的来源合同；不保存第二份成员或可花钱包。
#pragma once
#include "resident/storage/common_codec.hpp"
#include "resident/model/dynamic_contract.hpp"
namespace hanmo::resident {
enum class MembershipOfferStatus { pending, accepted, declined, withdrawn, expired };
enum class MembershipDecisionKind { accept, decline, defer };
// 作者冻结任职条款；工资率引用不可变经济目录，终态补偿规则必须明确选择。
struct MembershipTerms {
    StableId id,organization_id,duty_kind="contract_executor",site_id,member_policy_id;
    StableId payer_wallet_id,wage_rate_profile_id;
    Integer wage_period_ticks=1,offer_duration_ticks=2;
    StableId continuity_policy_id="continue_under_organization";
    StableId terminal_fraction_policy_id="terminal_fraction_compensation_v1";
};
// 本人政策只约束可接受职责与精确工资底线，不预写是否同意。
struct NamedMembershipPolicy {
    StableId id; std::vector<StableId> allowed_duty_kinds;
    Integer minimum_wage_numerator=0,minimum_wage_denominator=1,minimum_wage_period_ticks=1,review_interval_ticks=1;
    bool decline_known_arrears=true;
};
// 未决邀请冻结实际签发者和同一grant；终态引用真实决定及唯一成员episode。
struct MembershipOffer {
    StableId id,organization_id,issuer_actor_id,office_id,grant_id,target_actor_id,terms_id;
    Integer authority_epoch=0,created_tick=0,expires_tick=0,revision=1;
    MembershipOfferStatus status=MembershipOfferStatus::pending;
    StableId source_fact_id,decision_fact_id,member_episode_id;
};
// id即唯一organization决定事实ID；defer保留邀请且只推进有界复评。
struct MembershipDecision {
    StableId id,offer_id,target_actor_id; Integer offer_revision=0;
    MembershipDecisionKind kind=MembershipDecisionKind::defer;
    StableId reason; std::vector<StableId> source_fact_ids;
    Integer tick=0,decision_seq=0,next_review_tick=0;
};
// 只嵌入OrgMemberState；无此字段的成员必须由旧目录逐项证明。
struct MemberEpisodeSource {
    StableId offer_id,decision_id,terms_id,joined_fact_id,wage_binding_id;
    Integer joined_tick=0; std::optional<Integer> departed_tick;
    StableId departure_fact_id;
};
enum class MembershipReleaseBlockerKind { personal_action, interaction, battle, task_execution, entrusted_cargo, military_service, teaching, household };
// 阻塞来自实际责任owner，待收的钱不成为人身阻塞。
struct MembershipReleaseBlocker { MembershipReleaseBlockerKind kind=MembershipReleaseBlockerKind::personal_action; StableId owner_id,source_fact_id; };
struct MembershipReleaseView { StableId actor_id; Integer tick=0,revision=0; std::vector<MembershipReleaseBlocker> blockers; };
// 中央只读适配器按当前候选读取责任，不接受UI提供的busy布尔值。
class MembershipReleaseRead {
public:
    virtual ~MembershipReleaseRead()=default;
    virtual Outcome<MembershipReleaseView> membership_release_view(const StableId& actor_id) const=0;
};
} // 邀请和决定不占行动、不预留未来资金，也不授予office或grant。
