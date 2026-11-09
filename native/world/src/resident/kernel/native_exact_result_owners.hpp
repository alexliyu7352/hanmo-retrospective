// Compiled finite owner routing for P10. No caller-supplied registration, kind
// prefix guessing, or fallback tick coercion.
#pragma once
#include "economic_result_projection2.hpp"
#include "movement_result_projection2.hpp"
#include "resident/domains/interaction/communication_links.hpp"
#include "resident/domains/social/knowledge_retention.hpp"
#include "resident/domains/social/social_period_sources.hpp"
#include "resident/domains/tasks/task_result_time2.hpp"
namespace hanmo::resident {
struct NativeCompleteWorldCoreSources;
class CompleteExactWorldBindingOwner;
struct PublicSupplyInspectionReadContext;
struct CollateralControlState;
class NativeActorSocialOccupancyRead;
class NativeOriginalSocialWorldRead;
class CharacterConditionWorldRead;
struct PersonalAppointmentTime2Context;
struct ContactInquiryTime2Context;
struct VisitAppointmentMeetingTime2Context;
struct PrivateShopOperationSourceContext;
struct PrivateShopOpeningSources3;
struct PoliticalShopLicensingContext;
struct NativeExactResultAccess {
  const CommonStore &common;
  const TaskState &tasks;
  const AffairsState &affairs;
  const SocialState &social;
  const InteractionState *interaction = nullptr;
  const InteractionCatalog *interaction_catalog = nullptr;
  const CommunicationLinksState *communication = nullptr;
  const SocialPeriodSources *social_periods = nullptr;
  std::optional<EconomicResultProjectionAccess> economic_publications;
  std::optional<MovementResultProjectionAccess> movement;
  // These adapters are built from each named original domain in the same read
  // slice. They own no world state.
  std::shared_ptr<const ExactResultOwnerRead> growth, military, economy,
      organization, politics, family, personal, estate, commerce;
  std::shared_ptr<const ExactResultOwnerRead> s3_execution, medicine,
      scholarship, arcana;
  std::shared_ptr<const ExactResultOwnerRead> organization_authority;
  std::shared_ptr<const ExactResultOwnerRead> work, politics_time2;
  const NativeActorSocialOccupancyRead *social_occupancy = nullptr;
  std::shared_ptr<const ExactResultOwnerRead> political_fiscal_time2;
  std::shared_ptr<const ExactResultOwnerRead> military_supporting;
  std::shared_ptr<const ExactResultOwnerRead> treaty_revisions,
      treaty_procedures;
  const PersonalAppointmentTime2Context *personal_appointments = nullptr;
  const ContactInquiryTime2Context *contact_inquiries = nullptr;
  const VisitAppointmentMeetingTime2Context *visit_appointments = nullptr;
  std::shared_ptr<const ExactResultOwnerRead> safe_exit_bindings;
  std::shared_ptr<const ExactResultOwnerRead> visit_originals;
  // Borrowed from the same complete lease; each call reconstructs its actual-M
  // context.
  const NativeOriginalSocialWorldRead *original_social = nullptr;
  // The original S7 acceptance/labor/terminal readers own their exact moments.
  // This does not turn legacy Shop ticks into exact source timestamps.
  const PrivateShopOperationSourceContext *private_shop_operations = nullptr;
  // 同一 Complete owner 的真实 body；每次刷新 actual-M，不持 receipt 副本。
  const CharacterConditionWorldRead *condition_applications = nullptr;
  std::shared_ptr<const ExactResultOwnerRead> communication_execution;
  std::shared_ptr<const ExactResultOwnerRead> family_initial;
  // 与新旁支owner同根，仅实际typed source2选择新分派，旧schema1不覆盖。
  const CollateralControlState *family_collateral2 = nullptr;
  // 同World唯一 Inspect 来源，clock 与context由本reader真实 Sources 保活。
  const PublicSupplyInspectionReadContext *public_supply_inspection = nullptr;
  // 明选 Shop3/Law2 时才借同一 Core 的真实源；旧 tick 头不升级 schema。
  const PrivateShopOpeningSources3 *private_shop_opening3 = nullptr;
  const PoliticalShopLicensingContext *shop_licensing2 = nullptr;
};
// Transient owned enumeration, not a source-release permission or a second log.
// The actual text and exact fresh-array lease precedes every payload field.
class NativeExactResultRetainedSources final {
  friend class NativeExactResultRetainedCollector;
  CheckpointMemoryCharge memory_;
  std::vector<MomentOwnerSource> sources_;

public:
  NativeExactResultRetainedSources() = default;
  NativeExactResultRetainedSources(const NativeExactResultRetainedSources &) =
      delete;
  NativeExactResultRetainedSources &
  operator=(const NativeExactResultRetainedSources &) = delete;
  NativeExactResultRetainedSources(
      NativeExactResultRetainedSources &&) noexcept = default;
  NativeExactResultRetainedSources &
  operator=(NativeExactResultRetainedSources &&) = delete;
  const std::vector<MomentOwnerSource> &sources() const noexcept {
    return sources_;
  }
};
class NativeExactResultOwnerRead final : public ExactResultOwnerRead,
                                         public CheckpointAllocatedObject {
  // Declared first so every reader/context in access_ is destroyed before the
  // real owning graph it borrows. The World itself remains the caller's owner.
  struct Sources;
  static Outcome<std::unique_ptr<NativeExactResultOwnerRead>>
  compose(const WorldBundle &, const NativeCompleteWorldCoreSources *);
  std::unique_ptr<Sources> sources_;
  NativeExactResultAccess access_;
  const ExactResultOwnerRead *domain_owner(const StableId &) const;
  NativeExactResultOwnerRead(std::unique_ptr<Sources>, NativeExactResultAccess);
  friend Outcome<std::unique_ptr<NativeExactResultOwnerRead>>
  make_native_exact_result_sources(const WorldBundle &);
  friend Outcome<std::unique_ptr<NativeExactResultOwnerRead>>
  make_native_exact_result_sources_bound(
      const WorldBundle &, const NativeCompleteWorldCoreSources &);

public:
  // Narrow original-owner callers may borrow a graph already owned by their
  // caller. The whole-World entry below always installs its retained Sources.
  explicit NativeExactResultOwnerRead(NativeExactResultAccess);
  ~NativeExactResultOwnerRead() override;
  NativeExactResultOwnerRead(const NativeExactResultOwnerRead &) = delete;
  NativeExactResultOwnerRead &
  operator=(const NativeExactResultOwnerRead &) = delete;
  NativeExactResultOwnerRead(NativeExactResultOwnerRead &&) = delete;
  NativeExactResultOwnerRead &operator=(NativeExactResultOwnerRead &&) = delete;
  Outcome<ExactOwnerFactRead>
  read_selected_fact(const FactRecord &) const override;
  Outcome<WorldMoment>
  read_selected_source_moment(const MomentOwnerSource &) const override;
  Outcome<NativeExactResultRetainedSources> retained_sources_owned() const;
  // Legacy value ABI only; formal P10 callers use the lease-bearing result.
  Outcome<std::vector<MomentOwnerSource>> retained_sources() const;
};
// WorldBundle composition is root-owned. Every selected owner is bound from its
// actual state and catalogue.
Outcome<std::unique_ptr<NativeExactResultOwnerRead>>
make_native_exact_result_sources(const WorldBundle &);
// 外层持活同一个Core/Complete owner；只复用真实成员，不再次acquire完整世界。
Outcome<std::unique_ptr<NativeExactResultOwnerRead>>
make_native_exact_result_sources_bound(const WorldBundle &,
                                       const NativeCompleteWorldCoreSources &);
} // namespace hanmo::resident

namespace hanmo::resident {
// 兼容旧内部入口，只借已发布的同World真实core，不重新装配。
Outcome<std::unique_ptr<NativeExactResultOwnerRead>>
make_native_exact_result_sources_in_owner(
    const WorldBundle &, const CompleteExactWorldBindingOwner &);
} // namespace hanmo::resident
