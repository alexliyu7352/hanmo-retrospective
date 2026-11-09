// EC53 / RESIDENCE-2 唯一住居来源。不是产权、Location、雇佣或家庭供养权限。
// 首次 private_estate_services/2 候选，未选择模块时不得安装或补造旧住民接受。
// 暂定内部布局：5 表保原约定/接受/逐 line 历史，字段排列可改；不是已注册 ABI。
#pragma once
#include "residence_service_refs.hpp"
#include "resident/domains/actor_foundation/model.hpp"
#include "resident/domains/local_economy/population_transport.hpp"
#include "resident/facility_claims/model.hpp"
#include "resident/kernel/world_timeline.hpp"
#include "resident/storage/common_codec.hpp"

namespace hanmo::resident {
inline constexpr std::string_view residence_service_module =
    "private_estate_services";
inline constexpr Integer residence_service_schema = 2;

// actor 接受者与接受来源分别保留；未成年另借原合法 care authority。
struct ResidenceActorSource {
  StableId actor_id;
  MomentOwnerSource acceptance_ref;
  std::optional<MomentOwnerSource> care_authority_ref;
};
// 原 population revision/selection 不转换成户口数量，也不把 household mouths
// 当名单。
struct ResidenceCivilianSelectionSource {
  StableId selection_id;
  CivilianPopulationRef source_population_ref;
  MomentOwnerSource acceptance_source_ref;
};
using ResidentSource =
    std::variant<ResidenceActorSource, ResidenceCivilianSelectionSource>;

enum class ResidenceAgreementPhase {
  offered,
  active,
  notice,
  handing_over,
  ended
};
enum class ResidenceIntakePhase {
  accepted_waiting,
  travelling,
  arrived,
  resident,
  withdrawn,
  interrupted,
  departing,
  ended
};

// 暂定初态授权表示：同锁明确许可独立于产权，不由 owner/Location 推导。
// 本定义只选原「允许该 host 接纳住民」用途；不是可扩张的权限字符串。
struct ResidenceHostAdmissionDefinition {
  ActorFoundationRuleRef permission_ref;
  StableId host_actor_id, original_licensor_actor_id, estate_ref, place_ref;
  FacilitySlotRef bed_slot;
  MomentSpan validity_window;
};
// 条款只引用同锁内原食宿/费用/终止规则，不在此选择租价、默认床数或免除旧债。
// max_* 是作者明确的有限窗上界；0 不是无限或可用默认。
struct ResidenceTerms {
  ActorFoundationRuleRef terms_ref, host_admission_ref;
  StableId estate_ref, place_ref;
  FacilitySlotRef bed_slot;
  ActorFoundationRuleRef provision_terms_ref, termination_policy_ref;
  std::optional<ActorFoundationRuleRef> rent_terms_ref;
  Integer max_arrival_half_days = 0, max_residence_half_days = 0;
};
struct ResidenceServiceCatalog {
  CatalogIdentity identity;
  CheckpointVector<ResidenceTerms> terms;
  CheckpointVector<ResidenceHostAdmissionDefinition> host_admission_definitions;
};

// Offer 冻结真实受邀范围，不填未来 acceptance/selection ID。
struct ResidenceNamedRequest {
  StableId actor_id;
  std::optional<MomentOwnerSource> care_authority_ref;
};
struct ResidenceCivilianRequest {
  CivilianPopulationRef source_population_ref;
  PopulationPortion portion;
};
using ResidenceResidentRequest =
    std::variant<ResidenceNamedRequest, ResidenceCivilianRequest>;
// Offer 只记实际 host 的表达和已核原准入住权，不创建床位、不创建虚构来客。
struct ResidenceOffer {
  StableId id;
  ActorFoundationRuleRef terms_ref;
  SubjectRef host;
  StableId host_actor_id;
  ActorFoundationRuleRef host_admission_authority_ref;
  CheckpointVector<ResidenceResidentRequest> requested_residents;
  WorldMoment offered_at;
  SourceRef host_command_source;
};
// Agreement 的 resident_sources 是真正接受的原范围。实际分拆后的 line 可指
// 同一原选择的真实后继；household_affiliations 只关联资产/供养，绝不决定住民。
struct ResidenceAgreement {
  StableId id, offer_id;
  Integer terms_revision = 0;
  CheckpointVector<ResidentSource> resident_sources;
  CheckpointVector<StableId> household_affiliations;
  ResidenceAgreementPhase state = ResidenceAgreementPhase::offered;
};
// 本人接受仍是独立的实际源，必须绑定原同版 offer 及其 S0 成功命令。
struct ResidenceAcceptance {
  StableId id;
  ResidenceAgreementRef agreement;
  StableId accepting_actor_id;
  ResidentSource accepted_resident;
  WorldMoment accepted_at;
  SourceRef acceptance_command_source;
};
// claim ID 与原 admission receipt 一起定位 S0 采用，不保存第二床位账。
struct ResidenceSlotClaimRef {
  StableId claim_id;
  FacilityClaimReceiptRef admission_receipt;
};
struct ResidenceIntakeLine {
  StableId line_id;
  ResidenceAgreementRef agreement;
  ResidentSource resident_source;
  ResidenceSlotClaimRef slot_claim_ref;
  MomentSpan arrival_window;
  std::optional<MomentOwnerSource> arrival_source_ref, intake_result_ref;
  ResidenceIntakePhase state = ResidenceIntakePhase::accepted_waiting;
};
// 原实际状态变更，不是自由 cause/phase setter。Receive 的原 S1 到达、S5
// handoff 和 S0 claim 同候选完成后，才可写 resident；离去同理保真正原债。
struct ResidenceLineTransition {
  StableId id;
  ResidenceIntakeLineRef line;
  ResidenceIntakePhase before = ResidenceIntakePhase::accepted_waiting;
  ResidenceIntakePhase after = ResidenceIntakePhase::accepted_waiting;
  WorldMoment occurred_at;
  MomentOwnerSource original_cause;
  std::optional<FacilityClaimReceiptRef> claim_mutation;
  std::optional<MomentOwnerSource> provision_handoff;
  CheckpointVector<MomentOwnerSource> obligation_continuations;
};
// 只持本次约定、实际接受、逐原 line 和不可变转移来源；容量/人口/钱粮仍归
// S0/S5。shared codec/copy/retention 整组启用前，这个候选不能成为 World 根。
struct ResidenceServiceState {
  Integer schema_version = residence_service_schema;
  CheckpointVector<ResidenceOffer> offers;
  CheckpointVector<ResidenceAgreement> agreements;
  CheckpointVector<ResidenceAcceptance> acceptances;
  CheckpointVector<ResidenceIntakeLine> intake_lines;
  CheckpointVector<ResidenceLineTransition> transitions;
};
// 返回同一 job/root 的真实行；不能把这些借指针存回持久根或跨写使用。
struct ResidenceActorRightRead {
  const ResidenceTerms *terms = nullptr;
  const ResidenceOffer *offer = nullptr;
  const ResidenceAgreement *agreement = nullptr;
  const ResidenceIntakeLine *line = nullptr;
  // waiting 有真实接受/床位而尚无 intake；消费者不得把 null 当已入住。
  const ResidenceAcceptance *actual_acceptance = nullptr;
  const ResidenceLineTransition *actual_intake = nullptr;
  ResidenceIntakePhase phase_at_cut = ResidenceIntakePhase::accepted_waiting;
  const FacilityClaim2 *actual_claim = nullptr;
};
} // namespace hanmo::resident
