// 群体与个人共用MovementBasis/Prefix/Unit；真实承运资格/预算/政治许可保原owner。
#pragma once
#include "../local_economy/economic_transport_sources.hpp"
#include "../local_economy/military_march_payment.hpp"
#include "../military/military_march_contract.hpp"
#include "../politics/treaty/political_passage_decision.hpp"
#include "group_transport_registration.hpp"
#include "group_member_loss_handoff.hpp"
#include "transport_manifest_history.hpp"
namespace hanmo::resident {
class PersonalMovementFundingRead;
struct GroupMotionProfile {
  SourceRef actual_rule_source;
  Integer source_schema = 0;
  TravelProfile original_profile;
  Integer additional_cost_per_unit = 0;
  StableId fare_payee_wallet_id, fare_destination_id;
  // Army 原均摊公式的真实分子/人数；防止取整掩盖外部改载。
  Integer original_load_total = 0, original_load_people = 0;
};
struct StartGroupJourney {
  TransportUnitRef transport_unit_ref;
  Integer expected_composition_revision = 0;
  GroupInitiator initiator;
  SubjectRef sponsor;
  TravelDestination destination;
  std::vector<StableId> via, cargo_custody_refs;
  StableId payer_wallet_id;
  TransportExpenseAuthority expense_authority;
  std::vector<SourceRef> shipment_refs;
  StableId route_profile;
};
// Pure destination interpretation shared with the original arrival writer. This
// is a proposed endpoint, never an arrival/presence fact or reachability grant.
Outcome<Location>
read_group_route_destination_location(const TravelCatalog &,
                                      const TravelDestination &,
                                      const MovementAnchor &actual_origin);
struct GroupMovementOpeningSource {
  StableId id, journey_id;
  std::variant<TransportUnitRef, ArmyCarrierRef, FleetCarrierRef>
      carrier_origin;
  Integer composition_revision = 0;
  WorldMoment occurred_at;
  MovementAnchor original_anchor;
  GroupMotionProfile motion_profile;
  std::vector<SourceRef> manifest_sources;
  std::vector<StableId> named_participants;
  std::optional<MilitaryGroupOrderAcceptance> military_order;
};
struct GroupMovementUnitTerms {
  StableId unit_ref, journey_id;
  GroupMotionProfile motion_profile;
  Integer fare_amount = 0;
  SourceRef actual_start_source;
};
struct GroupExpensePrefixCause {
  MomentSpan first_actual_span;
};
struct GroupExpenseContactCause {
  SourceRef actual_contact;
};
struct GroupTransportExpenseSource {
  StableId id, journey_id, prefix_ref, unit_ref;
  WorldMoment occurred_at;
  std::variant<GroupExpensePrefixCause, GroupExpenseContactCause> actual_cause;
  std::optional<SourceRef> original_payment_receipt;
};
enum class GroupControlOperation { pause, resume, cancel };
struct GroupMovementControlSource {
  StableId id, journey_id;
  GroupControlOperation operation = GroupControlOperation::pause;
  SourceRef authority_source;
  WorldMoment occurred_at;
  MovementAnchor actual_anchor;
};
// An actual member handoff out of one contact-paused original journey.
// No position, new Army, new cargo account, fee or rewritten opening is stored.
// The actual release source and original S1 application prove each named exit.
struct GroupContactMemberReleaseSource {
  StableId id, journey_id;
  SourceRef original_contact, actual_release_source;
  Integer release_schema = 1;
  WorldMoment occurred_at;
  std::vector<StableId> named_participants;
};
enum class GroupSupplyRateDisposition { running, held, closed_unit };
// 同趟供养的原重算记录；只改变生效点后的映射，不改旧 opening、单位工期或已付费。
struct GroupMilitarySupplyRebindSource {
  StableId id, journey_id, army_id, prefix_ref, unit_ref, basis_ref;
  std::vector<StableId> cause_fact_ids;
  WorldMoment occurred_at;
  Integer before_composition_revision = 0, after_composition_revision = 0;
  GroupMotionProfile before_profile, after_profile;
  std::vector<SourceRef> before_manifest_sources, after_manifest_sources;
  GroupSupplyRateDisposition disposition = GroupSupplyRateDisposition::running;
  ActivityWorldMap original_mapping;
  CanonicalFraction original_cursor, original_active_elapsed;
  Integer before_budget = 0, after_budget = 0;
};
struct GroupMovementExecutionState {
  OrderedTable<GroupMovementOpeningSource> openings;
  OrderedTable<GroupMovementUnitTerms> unit_terms;
  OrderedTable<GroupTransportExpenseSource> expenses;
  OrderedTable<GroupMovementControlSource> controls;
  OrderedTable<GroupMilitarySupplyRebindSource> military_supply_rebinds;
  OrderedTable<TransportEntryManifestSource> entry_manifests;
  OrderedTable<GroupContactMemberReleaseSource> contact_member_releases;
  // Independent movement_group_member_losses/1 extension; old bytes unchanged.
  OrderedTable<GroupMemberLossHandoffSource> member_loss_handoffs;
};
// Concrete original Field/S1 source owner, never a caller-supplied member list.
class GroupContactMemberReleaseRead : public GroupMemberLossHandoffRead {
public:
  virtual ~GroupContactMemberReleaseRead() = default;
  virtual bool bound_to(const CommonStore &, const MovementExactState &,
                        const GroupMovementExecutionState &) const noexcept = 0;
  virtual Status validate_actual_group_member_release(
      const GroupContactMemberReleaseSource &) const = 0;
};
Outcome<CheckpointVector<const StableId *>>
read_active_group_named_members(const CommonStore &, const MovementExactState &,
                                const GroupMovementExecutionState &,
                                const GroupMovementJourney &,
                                const std::vector<StableId> &original_members,
                                const GroupContactMemberReleaseRead *);
Status validate_group_contact_member_releases(
    const CommonStore &, const MovementExactState &,
    const GroupMovementExecutionState &, const GroupContactMemberReleaseRead *);
// 以下读口只折叠真实 Common cut 前的原 opening/供养重算，不改历史记录。
Outcome<Integer> read_group_composition_revision_before(
    const CommonStore&,const GroupMovementExecutionState&,const StableId& journey_id,
    const SourceRef* actual_before=nullptr);
Outcome<GroupMotionProfile> read_group_effective_motion_profile(
    const CommonStore&,const GroupMovementExecutionState&,const GroupMovementUnitTerms&,
    const SourceRef* actual_before=nullptr);
Outcome<const GroupMilitarySupplyRebindSource*> read_group_military_supply_rebind_source(
    const CommonStore&,const SpatialCarrierState&,const MovementExactState&,
    const GroupMovementExecutionState&,const SourceRef&,Integer schema,
    ExactArithmeticBudget={});
bool same_group_motion_profile(const GroupMotionProfile&,const GroupMotionProfile&);
class GroupMovementSources {
public:
  virtual ~GroupMovementSources() = default;
  virtual Outcome<GroupMotionProfile>
  read_motion_profile(const TransportManifestRead &,
                      const StableId &route_profile,
                      const WorldMoment &) const = 0;
  virtual Status validate_motion_profile(const TransportManifestRead &,
                                         const GroupMotionProfile &,
                                         const WorldMoment &) const = 0;
  // 未实现的商业提供者不能承接军事供养来源；真实 Army owner 逐源闭合。
  // 原军旅 owner 对全部 Supply cause 做正反覆盖；民用 reader 不假装拥有军粮来源。
  virtual Status validate_military_supply_causes() const {
    return common_failure("MilitarySupplyRebindOriginalOwnerRequired",{},ErrorKind::unsupported_capability);
  }
  virtual Status validate_military_supply_rebind(const GroupMilitarySupplyRebindSource&) const {
    return common_failure("MilitarySupplyRebindOriginalOwnerRequired");
  }
  virtual Status
  validate_start_authority(const StartGroupJourney &,
                           const TransportUnitState &,
                           const TransportManifestRead &) const = 0;
  // Current trusted clock is span.from; this is real pre-interval eligibility.
  virtual Status validate_motion_conditions(const GroupMovementJourney &,
                                            const TransportManifestRead &,
                                            const MomentSpan &) const = 0;
  virtual Status validate_control_authority(const GroupMovementJourney &,
                                            const GroupInitiator &,
                                            GroupControlOperation,
                                            const SourceRef &) const = 0;
};
// S6只在真正进入的当前边界产决定；S1付款/跨界前采用此真实来源。
struct ActualGroupPassageEntry {
  GroupMovementExecution execution;
  StableId prefix_ref, unit_ref, basis_ref;
  Integer entry_ordinal = 0, composition_revision = 0, manifest_revision = 0;
  WorldMoment occurred_at;
  MovementAnchor actual_anchor;
  std::optional<StableId> existing_crossing_id;
};
// The original road basis and locked route determine this directed boundary.
// Null means the real execution is already inside its edge; terrain gates,
// ferries and ports require their original authored boundary adapter.
Outcome<std::optional<PassageBoundaryRef>>
read_actual_group_road_entry_boundary(const SpatialCarrierState &,
                                      const MovementExactState &,
                                      const TravelCatalog &,
                                      const GroupMovementExecution &,
                                      const WorldMoment &,
                                      ExactArithmeticBudget = {});
Outcome<ActualGroupPassageEntry> read_actual_group_passage_entry(
    const SpatialCarrierState &, const MovementExactState &,
    const TravelCatalog &, const GroupMovementExecution &,
    const PassageBoundaryRef &, const WorldMoment &,
    ExactArithmeticBudget budget = {});
Status validate_group_boundary_crossing_source(
    const CommonStore&,const GroupMovementExecutionState&,
    const SpatialCarrierState &, const MovementExactState &,
    const TravelCatalog &, const BoundaryCrossingReceipt &,
    ExactArithmeticBudget budget = {});
// The original crossing proves entry. This reader separately proves completion
// of its finite directed edge from the actual close/arrival chain.
// The immutable first-span acceptance is retained in the original expense
// cause. Its applied endpoint is proven against the actual unit activity; it is
// not the expense occurrence (the latter is the original from adoption).
struct ActualGroupCrossingPositiveSpan {
  MomentOwnerSource actual_span_source;
  StableId unit_ref;
  MomentSpan actual_span;
};
Outcome<ActualGroupCrossingPositiveSpan>
read_actual_group_crossing_first_positive_span(
    const CommonStore &, const SpatialCarrierState &,
    const MovementExactState &, const GroupMovementExecutionState &,
    const TravelCatalog &,
    const BoundaryCrossingReceipt &actual_borrowed_crossing,
    ExactArithmeticBudget budget = {});
struct ActualPassageBoundaryCompletion {
  MomentOwnerSource actual_source;
  WorldMoment completed_at;
  MovementAnchor actual_anchor;
  StableId reached_site_id;
};
Outcome<std::optional<ActualPassageBoundaryCompletion>>
read_actual_group_boundary_completion(
    const CommonStore &, const SpatialCarrierState &,
    const MovementExactState &, const GroupMovementExecutionState&, const TravelCatalog &,
    const BoundaryCrossingReceipt &actual_borrowed_crossing,
    ExactArithmeticBudget budget = {});
struct PreparedGroupPassage {
  PassageRequest request;
  PoliticalPassageDecision decision;
};
class GroupPoliticalPassage {
public:
  virtual ~GroupPoliticalPassage() = default;
  virtual Outcome<std::vector<PreparedGroupPassage>>
  prepare_actual_entry(const GroupMovementJourney &,
                       const TransportManifestRead &, const MovementBasis &,
                       const WorldMoment &) const = 0;
  virtual Outcome<SourceRef>
  adopt_actual_decision_in_candidate(CommonStore &,
                                     const PreparedGroupPassage &) = 0;
  virtual Outcome<ActualTransportExpense>
  read_adopted_toll(const SourceRef &, const GroupMovementJourney &,
                    const WorldMoment &) const = 0;
  virtual Status
  validate_crossing_decision(const BoundaryCrossingReceipt &) const = 0;
  // Called only after the original positive prefix has actually been written.
  // The trusted clock is span.to; the retained crossing owns
  // entry_at=span.from. Null is permitted only when the actual adopted decision
  // has no TreatyPassage.
  virtual Outcome<std::optional<SourceRef>>
  adopt_after_positive_crossing_in_candidate(
      CommonStore &, const SourceRef &actual_crossing) = 0;
  virtual Status
  validate_after_positive_crossing(const BoundaryCrossingReceipt &) const = 0;
  // Actual directed boundary completion is separate from positive entry.
  virtual Status adopt_after_boundary_completion_in_candidate(
      CommonStore &, const SourceRef &actual_crossing) = 0;
  virtual Status
  validate_after_boundary_completion(const BoundaryCrossingReceipt &) const = 0;
};
struct GroupExpenseBasisRead {
  const GroupTransportExpenseSource *source = nullptr;
  const GroupMovementUnitTerms *terms = nullptr;
};
Outcome<GroupExpenseBasisRead> read_verified_group_expense_basis(
    const MovementExactState &, const SpatialCarrierState &,
    const GroupMovementExecutionState &, const TravelCatalog &,
    const SourceRef &, Integer schema);
class NativeGroupTransportExpenseRead final
    : public TransportExpenseSourceRead {
public:
  NativeGroupTransportExpenseRead(const MovementExactState &,
                                  const SpatialCarrierState &,
                                  const GroupMovementExecutionState &,
                                  const TravelCatalog &,
                                  const SpatialCarrierSources &);
  Outcome<ActualTransportExpense>
  read_actual_transport_expense(const SourceRef &, Integer) const override;
  Outcome<WorldMoment> current_transport_expense_moment() const override;
  Status validate_transport_expense_authority(
      const SourceRef &, Integer,
      const TransportExpenseAuthority &) const override;

private:
  const MovementExactState &movement_;
  const SpatialCarrierState &carriers_;
  const GroupMovementExecutionState &executions_;
  const TravelCatalog &catalog_;
  const SpatialCarrierSources &spatial_sources_;
};
struct HouseholdPurchaseTransportContext;
struct GroupMovementContext {
  const TravelCatalog &travel_catalog;
  const TravelServices &travel_services;
  const SpatialCarrierSources &carriers;
  const GroupMovementSources &actual_sources;
  // 军原费用使用下方 military_expenses；没有商业选择时不伪造费用 reader。
  // 只有实际商业/关费支付分支才解引用，并必须保留缺原源拒门。
  std::optional<std::reference_wrapper<const TransportExpenseAuthorityRead>>
      expense_authorities;
  GroupPoliticalPassage &passage;
  const TransportManifestHistorySourceRead &manifest_history;
  const CommonCatalog &common_catalog;
  const InventoryEntryItemSources *inventory_item_sources = nullptr;
  const MilitaryMarchExpenseSourceRead *military_expenses = nullptr;
  const HouseholdPurchaseTransportContext *household_transport = nullptr;
  // Same-world original P11 source-only obligation/charge owner. Required by
  // the actual nonzero commercial payment path; no private debt state here.
  const EconomicObligationReadContext *obligations = nullptr;
  EconomicObligationWriteContext *obligation_writes = nullptr;
  const GroupContactMemberReleaseRead *member_releases = nullptr;
};
// 准备值只保实际前驱，不跨 writer 借 read ticket。供养写后才按原消费差量采用。
Outcome<GroupMilitarySupplyRebindSource> prepare_group_military_supply_rebind(
    const CommonStore&,const SpatialCarrierState&,const MovementExactState&,
    const GroupMovementExecutionState&,const GroupMovementContext&,const StableId& journey_id,
    const WorldMoment&,ExactArithmeticBudget={});
struct MilitarySupplyFact;
Status apply_group_military_supply_rebind_in_candidate(
    CommonStore&,SpatialCarrierState&,MovementExactState&,GroupMovementExecutionState&,
    const GroupMovementContext&,const GroupMilitarySupplyRebindSource& prepared,
    const MilitarySupplyFact&,const WorldMoment&,ExactArithmeticBudget={});
// Only the original S2 command owner constructs this request and adopts its
// real successful command in the very same candidate, after S1 has allocated
// its source.
struct ArmyGroupJourneyOpeningInput {
  ArmyCarrierRef carrier;
  Integer expected_composition_revision = 0;
  StableId issuing_actor_id, campaign_id, organization_id, payer_wallet_id;
  TravelDestination destination;
  std::vector<StableId> via, named_march_participants;
  StableId route_profile;
};
class MilitaryGroupOpeningAdoption {
public:
  virtual ~MilitaryGroupOpeningAdoption() = default;
  virtual Status adopt_original_order_in_candidate(
      CommonStore &, SpatialCarrierState &, MovementExactState &,
      GroupMovementExecutionState &, const StableId &journey_id) = 0;
};
Outcome<StableId> start_army_group_journey_in_candidate(
    CommonStore &, SpatialCarrierState &, MovementExactState &,
    GroupMovementExecutionState &, const GroupMovementContext &,
    const ArmyGroupJourneyOpeningInput &, MilitaryGroupOpeningAdoption &,
    const WorldMoment &, std::vector<BoundSpatialLocationWrite> &,
    ExactArithmeticBudget budget = {});
Outcome<StableId> start_group_journey_in_candidate(
    CommonStore &, SpatialCarrierState &, MovementExactState &,
    GroupMovementExecutionState &, const GroupMovementContext &,
    const StartGroupJourney &, const WorldMoment &,
    std::vector<BoundSpatialLocationWrite> &,
    ExactArithmeticBudget budget = {});
// Called only for S0's selected positive span, at the true from phase, before
// any interval writer. Entry/first cost and the actual tail are one candidate.
Status adopt_group_positive_span_from_in_candidate(
    CommonStore &, LocalEconomyState &, EconomicTransportExpenseState &,
    SpatialCarrierState &, MovementExactState &, GroupMovementExecutionState &,
    const GroupMovementContext &, const StableId &journey_id,
    const MomentSpan &, ExactArithmeticBudget budget = {});
Status apply_exact_group_prefix_in_candidate(
    CommonStore &, LocalEconomyState &, EconomicTransportExpenseState &,
    SpatialCarrierState &, MovementExactState &, GroupMovementExecutionState &,
    const GroupMovementContext &, const StableId &journey_id,
    const MomentSpan &, std::vector<BoundSpatialLocationWrite> &,
    ExactArithmeticBudget budget = {});
Outcome<StableId> close_exact_group_unit_in_candidate(
    CommonStore &, SpatialCarrierState &, MovementExactState &,
    GroupMovementExecutionState &, const GroupMovementContext &,
    const StableId &journey_id, MovementCloseKind,
    const SourceRef &actual_cause, const WorldMoment &,
    std::vector<BoundSpatialLocationWrite> &,
    ExactArithmeticBudget budget = {});
Status control_group_journey_in_candidate(
    CommonStore &, SpatialCarrierState &, MovementExactState &,
    GroupMovementExecutionState &, const GroupMovementContext &,
    const StableId &journey_id, Integer expected_composition_revision,
    const GroupInitiator &, GroupControlOperation,
    const SourceRef &actual_authority, const WorldMoment &,
    std::vector<BoundSpatialLocationWrite> &,
    ExactArithmeticBudget budget = {});
// 同一接触两侧分别调实际个人/群体writer，再一次交S4真实用力源。
Outcome<std::vector<StableId>> apply_exact_world_contact_in_candidate(
    CommonStore &, TravelState &, LocalEconomyState &,
    EconomicTransportExpenseState &, SpatialCarrierState &,
    MovementExactState &, GroupMovementExecutionState &,
    const GroupMovementContext &, const ExactMovementContact &,
    const StableId &encounter_id, bool interactive,
    std::vector<BoundSpatialLocationWrite> &, ExactArithmeticBudget budget = {},
    const PersonalMovementFundingRead *personal_funding = nullptr,
    const CurrentMomentRead *clock = nullptr);
Status validate_group_movement_execution(const CommonStore &,
                                         const SpatialCarrierState &,
                                         const MovementExactState &,
                                         const GroupMovementExecutionState &,
                                         const EconomicTransportExpenseState &,
                                         const GroupMovementContext &,
                                         ExactArithmeticBudget budget = {});
Outcome<WorldMoment>
read_group_source_moment(const CommonStore &, const SpatialCarrierState &,
                         const MovementExactState &,
                         const GroupMovementExecutionState &,
                         const TransportRegistrationState &, const SourceRef &,
                         Integer schema, ExactArithmeticBudget budget = {});
// Rechecks the real arrival/cancel producer and original closed execution.
// Transport completion does not constitute shipment delivery or military duty
// success.
struct ActualGroupJourneyTerminal {
  GroupJourneyTerminal terminal;
  MovementExecutionRef execution;
  MovementAnchor actual_anchor;
};
Outcome<ActualGroupJourneyTerminal> read_actual_group_journey_terminal(
    const CommonStore &, const SpatialCarrierState &,
    const MovementExactState &, const GroupMovementExecutionState &,
    const StableId &journey_id, ExactArithmeticBudget budget = {});
Status validate_group_journey_terminals(const CommonStore &,
                                        const SpatialCarrierState &,
                                        const MovementExactState &,
                                        const GroupMovementExecutionState &,
                                        ExactArithmeticBudget budget = {});
Status visit_group_movement_execution_checkpoint(CodecVisitor &,
                                                 GroupMovementExecutionState &);
Status
visit_group_movement_execution_checkpoint(CodecVisitor &,
                                          GroupMovementExecutionState &,
                                          Integer selected_group_version);
// Real finite revision: original opening and adopted releases only; no zero
// revision and no present Army roster used as a membership-history substitute.
Outcome<std::uint64_t> read_group_member_presence_revision(
    const CommonStore &, const MovementExactState &,
    const GroupMovementExecutionState &, const StableId &journey_id,
    const GroupContactMemberReleaseRead *);

// 真胜方终局之后，在下一原经济 Opening 续同 Army 原旅程与余程。
// 只开启原新 unit，正时间片才按原费用写者收费，不重发命令、不改位置；
// 所有成员后果先完成，失败必须丢弃整个外层候选。
struct FieldExactContactReadContext;
struct FieldBattleCatalog;
struct FieldBattleWorldState;
Outcome<SourceRef> resume_exact_group_field_winner_in_candidate(
    CommonStore &, SpatialCarrierState &, MovementExactState &,
    GroupMovementExecutionState &, FieldBattleWorldState &,
    const GroupMovementContext &, const FieldExactContactReadContext &,
    const FieldBattleCatalog &, const StableId &battle_id,
    const StableId &army_id, CheckpointMemoryCharge &,
    ExactArithmeticBudget = {});
} // namespace hanmo::resident
