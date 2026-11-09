// M-CONTINUITY/1 原九键拥有体候选；程序只引用唯一Office/人/资源来源。
// 尚无全部业务producer/完整源门，不得仅因本型可解码就选择或发布该模块。
#pragma once
#include "political_foundation_decisions.hpp"
#include "resident/domains/organization/authority_registry.hpp"
namespace hanmo::resident {
enum class PoliticalSuccessionRuleKind {
  higher_office_appointment,
  designated_successor,
  council_selection,
  control_recognition
};
enum class PoliticalSuccessionPhase {
  nomination,
  voting,
  awaiting_arrival,
  accessioned,
  vacant,
  disputed,
  closed
};
// 开案时冻结真人及实际资格来源。以后失资格需原源，后来入会不得补进快照。
struct PoliticalSuccessionElector {
  StableId actor_id;
  std::vector<SourceRef> eligibility_sources;
};
// P§15.4的程序行；原任期用既有OfficeAuthoritySource保作者根或真更职源，
// 不强迫初始任期伪造Common事实，不在本表复制当前holder或可用权力。
struct PoliticalContinuitySuccessionCase {
  StableId id, org_id, office_id;
  OfficeAuthoritySource original_term;
  Integer office_epoch = 0;
  StableId rule_id;
  Integer rule_version = 0;
  SourceRef trigger_ref;
  Integer opening_tick = 0;
  PoliticalSuccessionPhase phase = PoliticalSuccessionPhase::nomination;
  Integer round = 1;
  std::vector<SourceRef> nomination_refs;
  std::vector<PoliticalSuccessionElector> elector_snapshot;
  std::vector<SourceRef> ballot_refs;
  std::optional<SourceRef> interim_authority_ref;
  Integer deadline_tick = 0;
  std::optional<SourceRef> resolution_ref;
};
// 真提名、本人答复和撤回分三个不可变叶；不把未发生答复写进提名行。
struct PoliticalContinuityNomination {
  StableId id, case_id, candidate_actor_id;
  Integer terms_revision = 1;
  ActingAuthority authority;
  AuthorityBasisRef authority_basis_ref;
  WorldMoment issued_at;
};
enum class PoliticalContinuityAnswer { accept, refuse };
struct PoliticalContinuityNominationAnswer {
  StableId id, actor_id;
  SourceRef nomination_ref;
  Integer terms_revision = 1;
  PoliticalContinuityAnswer decision = PoliticalContinuityAnswer::refuse;
  MomentOwnerSource actual_content_receipt;
  WorldMoment answered_at;
};
struct PoliticalContinuityNominationWithdrawal {
  StableId id;
  SourceRef nomination_ref;
  ActingAuthority authority;
  AuthorityBasisRef authority_basis_ref;
  WorldMoment withdrawn_at;
};
using PoliticalContinuityNominationDecision =
    std::variant<PoliticalContinuityNomination,
                 PoliticalContinuityNominationAnswer,
                 PoliticalContinuityNominationWithdrawal>;
// 本人投出和计票者实际收到是两个已发生来源；不能以cast自动填received。
struct PoliticalContinuityBallotCast {
  StableId id, case_id, actor_id;
  Integer round = 1;
  SourceRef candidate_acceptance_ref;
  MomentOwnerSource actual_content_receipt;
  WorldMoment cast_at;
};
struct PoliticalContinuityBallotReceived {
  StableId id, counting_actor_id;
  SourceRef ballot_ref;
  MomentOwnerSource actual_delivery_ref;
  WorldMoment received_at;
};
using PoliticalContinuityBallot =
    std::variant<PoliticalContinuityBallotCast,
                 PoliticalContinuityBallotReceived>;
// claim仅记录实际控制/指挥与本人接受依据，不成为第二Office或战力数值。
struct PoliticalContinuityLeadershipClaim {
  StableId id, case_id, actor_id;
  SourceRef seat_control_ref, command_ref, candidate_acceptance_ref;
  WorldMoment claimed_at;
};
struct PoliticalContinuityRecognition {
  StableId id, actor_id;
  SourceRef claim_ref;
  PoliticalContinuityAnswer decision = PoliticalContinuityAnswer::refuse;
  MomentOwnerSource actual_content_receipt;
  WorldMoment decided_at;
};
struct PoliticalContinuityRecognitionReceived {
  StableId id, counting_actor_id;
  SourceRef recognition_ref;
  MomentOwnerSource actual_delivery_ref;
  WorldMoment received_at;
};
using PoliticalContinuityRecognitionRecord =
    std::variant<PoliticalContinuityLeadershipClaim,
                 PoliticalContinuityRecognition,
                 PoliticalContinuityRecognitionReceived>;
// 原ExileBasis唯一程序行；不是地图外免费组织，不把旅客/家属当执行力量。
struct PoliticalContinuityExileCase {
  StableId id, org_id;
  SourceRef acting_head_or_agency;
  std::vector<SourceRef> accepted_followers;
  std::optional<SourceRef> base_or_shelter_right;
  std::vector<SourceRef> mobile_carrier_refs, supply_obligation_refs;
  Integer next_review_tick = 0;
};
// 原review实际证据，不重复保存独立可写ScopedExileStatus。读取该唯一review
// 时再投影其原source_ref到U§7.3；技术缺源不是NoIndependent的业务结果。
struct PoliticalContinuityViableExile {
  SourceRef scope_continuity_ref, leadership_or_lawful_agency_ref;
  std::vector<SourceRef> accepted_political_or_military_execution_refs;
  std::vector<SourceRef> usable_base_or_mobile_carrier_refs;
  std::vector<SourceRef> actual_supply_basis_refs;
};
struct PoliticalContinuityNoIndependentBasis {
  std::vector<SourceRef> disposition_source_refs;
};
using PoliticalContinuityExileReviewEvidence =
    std::variant<PoliticalContinuityViableExile,
                 PoliticalContinuityNoIndependentBasis>;
struct PoliticalContinuityExileReview {
  StableId id, scope_id;
  SourceRef exile_case_ref;
  // 反读实际发生的供给/庇护/载体/领导变化，不以当前时间猜复评原因。
  std::vector<SourceRef> reviewed_change_refs;
  PoliticalContinuityExileReviewEvidence evidence;
  WorldMoment reviewed_at;
};
enum class PoliticalLegacyContinuationKind {
  political_succession,
  successor_round,
  term_handover
};
// 旧原账仍是唯一writer；本行只是旧源→新程序消费的关联，不复制旧票或Office。
struct PoliticalLegacyContinuationBinding {
  StableId id, case_id;
  PoliticalLegacyContinuationKind original_kind =
      PoliticalLegacyContinuationKind::political_succession;
  MomentOwnerSource original_source;
  SourceRef actual_consumer;
};
struct PoliticalContinuityState {
  Integer schema_version = 1;
  OrderedTable<PoliticalContinuitySuccessionCase> succession_cases;
  OrderedTable<PoliticalContinuityNominationDecision> nomination_decisions;
  OrderedTable<PoliticalContinuityBallot> ballots;
  OrderedTable<PoliticalContinuityRecognitionRecord> recognitions;
  // 此union现只有真实意向/支持两叶，后继成立/分合必须与实际producer同批加入。
  OrderedTable<PoliticalLifecycleDecisionRecord> lifecycle_decisions;
  OrderedTable<PoliticalContinuityExileCase> exile_cases;
  OrderedTable<PoliticalContinuityExileReview> exile_reviews;
  OrderedTable<PoliticalLegacyContinuationBinding> legacy_continuation_bindings;
};
} // namespace hanmo::resident
