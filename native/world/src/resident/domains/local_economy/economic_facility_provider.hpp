// 原经济09实际provider。借同一candidate的真实输入；容量/claim不存派生副本。
#pragma once
#include "economic_facility_sources.hpp"
#include "production/facility_condition_contract.hpp"
#include "production_management.hpp"
#include "resident/domains/commerce/private_shop.hpp"
#include "resident/domains/estate/estate_model.hpp"
#include "resident/facility_claims/facility_claim_runtime.hpp"
namespace hanmo::resident {
// 原InitialWorldSeed锁的完整资产行，不能只凭digest或今日owner发安装证。
using PhysicalFacilityOriginalSeedRow =
    std::variant<ProductionAsset, EstateDefinition, PrivateShopPremises>;
struct EconomicFacilityInitialSeed {
  FacilitySeedRowRef row_ref;
  StableId facility_id;
  PhysicalFacilityOriginalSeedRow original_asset_row;
  EconomicRuleRef definition_ref;
  Owner initial_title_owner;
  StableId place_ref;
};
struct EconomicFacilityOpenRule {
  EconomicRuleRef rule_ref;
  bool continuously_open = false;
  std::vector<MomentSpan> accepted_fixed_intervals;
};
struct EconomicFacilityUsagePolicy {
  EconomicRuleRef rule_ref;
  bool own_initial_asset_use = false;
};
struct EconomicFacilityCatalog {
  CatalogIdentity identity;
  std::vector<EconomicFacilitySlotDefinition> definitions;
  std::vector<EconomicFacilityInitialSeed> initial_rows;
  std::vector<EconomicFacilityOpenRule> opening_rules;
  std::vector<EconomicFacilityUsagePolicy> usage_policies;
};
// Physical identity is retained unchanged during parsing. Once the central
// authored compilation lock is final, compare the full original identity here.
Status validate_economic_facility_original_shop_catalog(
    const EconomicFacilityCatalog &, const PrivateShopCatalog *);
Status validate_economic_facility_catalog_identity(
    const EconomicFacilityCatalog &,
    const CatalogIdentity &actual_selected_identity);
// Immutable locked definitions adopted with the real original facility install.
// The final /1 physical root retains these full versions for old source/claim
// reads.
bool same_economic_facility_definition(const EconomicFacilitySlotDefinition &,
                                       const EconomicFacilitySlotDefinition &);
Status validate_adopted_economic_facility_definitions(
    const CommonStore &, const EconomicFacilitySourcesState &,
    const EconomicFacilityCatalog &);
Status adopt_locked_economic_facility_definitions_in_candidate(
    const CommonStore &, EconomicFacilitySourcesState &,
    const EconomicFacilityCatalog &);
struct EconomicFacilityActivationRead {
  StableId original_asset_ref;
  EconomicRuleRef definition_ref;
  WorldMoment occurred_at;
  Integer active_from_tick = 0, capacity_units = 0;
  StableId capacity_unit_id;
  SourceRef original_source;
};
// 此层只接已存在的真实专业结果；普通Installed/condition/原开店在Native
// provider直接读。
// 各原owner实现，不能用caller布尔值或FactRecord存在代替payload验证。
class EconomicFacilityAppliedBusinessRead {
public:
  virtual ~EconomicFacilityAppliedBusinessRead() = default;
  virtual Outcome<EconomicFacilityActivationRead>
  read_applied_facility_activation(
      const FacilityActivationReceiptRef &) const = 0;
  virtual Outcome<std::vector<MomentSpan>>
  read_complete_applied_facility_opening(const SourceRef &,
                                         const FacilitySlotRef &) const = 0;
  virtual Outcome<EconomicFacilityUsageRightBasis>
  read_applied_facility_right_basis(const FacilitySourceRef &,
                                    const FacilitySlotRef &) const = 0;
  virtual Outcome<std::vector<MomentSpan>>
  read_applied_facility_opening(const SourceRef &, const FacilitySlotRef &,
                                const MomentSpan &) const = 0;
  virtual Outcome<std::vector<MomentSpan>>
  read_applied_facility_usage_right(const EconomicFacilityUsageRightBasis &,
                                    const PhysicalServiceFacilityExtension &,
                                    const MomentSpan &) const = 0;
  virtual Outcome<WorldMoment>
  read_applied_facility_change(const FacilitySourceRef &,
                               const FacilitySlotRef &) const = 0;
  // 原实际源在Common事实顺序中的采用位置；inline安装为0，其余严格实际原头index+1。
  virtual Outcome<std::size_t>
  read_applied_facility_source_order(const FacilitySourceRef &,
                                     const FacilitySlotRef &) const = 0;
  virtual Status
  validate_original_retention_entry(const FacilityRetentionTransition &,
                                    const FacilityRetentionEntry &,
                                    const FacilityClaimsState &) const = 0;
  virtual Outcome<EconomicFacilityChangeCause>
  read_applied_capacity_change_basis(const FacilitySourceRef &,
                                     const FacilitySlotRef &) const = 0;
  virtual Outcome<std::optional<FacilityRetentionEntry>>
  read_original_facility_retention_disposition(
      const FacilityClaim2 &, const FacilityClaimMutationReceipt &) const = 0;
  virtual Outcome<std::vector<FacilityPhysicalUnitRef>>
  read_original_facility_admission_units(
      const FacilityClaim2 &, const FacilityClaimMutationReceipt &) const = 0;
  virtual Outcome<std::vector<FacilitySourceRef>>
  read_original_facility_dispositions(const FacilitySourceRef &,
                                      const FacilitySlotRef &) const = 0;
  virtual Outcome<std::optional<FacilitySourceRef>>
  read_applied_whole_slot_restriction(const FacilitySourceRef &,
                                      const FacilitySlotRef &) const = 0;
  virtual Outcome<bool>
  facility_conflict_visible(const ReservationRef &,
                            const FacilityClaim2 &) const = 0;
};
// The original Installed branch needs only locked original data, not service
// claims, occupancy callbacks or future activation providers. Both the full S0
// provider and production-condition history use this one verification path.
struct EconomicFacilityInstallationReadContext {
  const CommonStore &common;
  const EconomicFacilityCatalog &catalog;
  const EconomicFacilitySourcesState &state;
  const ProductionManagementCatalog &production_assets;
  const EstateCatalog &estate_catalog;
  const PrivateShopCatalog *shops_catalog;
  WorldMoment current;
  ExactArithmeticBudget arithmetic{};
  // 未选私铺只借空指针；每次使用仍核原选择及09全部反向来源。
  EconomicFacilityInstallationReadContext(const CommonStore &c,
                                          const EconomicFacilityCatalog &k,
                                          const EconomicFacilitySourcesState &s,
                                          const ProductionManagementCatalog &p,
                                          const EstateCatalog &e,
                                          const PrivateShopCatalog *h,
                                          WorldMoment now,
                                          ExactArithmeticBudget budget = {})
      : common(c), catalog(k), state(s), production_assets(p),
        estate_catalog(e), shops_catalog(h), current(std::move(now)),
        arithmetic(budget) {}
  EconomicFacilityInstallationReadContext(const CommonStore &c,
                                          const EconomicFacilityCatalog &k,
                                          const EconomicFacilitySourcesState &s,
                                          const ProductionManagementCatalog &p,
                                          const EstateCatalog &e,
                                          const PrivateShopCatalog &h,
                                          WorldMoment now,
                                          ExactArithmeticBudget budget = {})
      : EconomicFacilityInstallationReadContext(c, k, s, p, e, &h,
                                                std::move(now), budget) {}
};
Status validate_economic_facility_installation_source(
    const EconomicFacilityInstallationReadContext &,
    const PhysicalServiceFacilityExtension &);
struct EconomicFacilityProviderContext {
  const CommonStore &common;
  const EconomicFacilityCatalog &catalog;
  const EconomicFacilitySourcesState &state;
  const ProductionManagementCatalog &production_assets;
  const EstateCatalog &estate_catalog;
  const EstateState &estates;
  const PrivateShopCatalog *shops_catalog;
  const PrivateShopState &shops;
  const ProductionFacilityConditionsCatalog &condition_catalog;
  const ProductionFacilityConditionsState &conditions;
  const FacilityClaimsState &claims;
  const FacilityHolderSourceRead &holders;
  const EconomicFacilityAppliedBusinessRead &applied;
  WorldMoment current;
  ExactArithmeticBudget arithmetic{};
  EconomicFacilityProviderContext(
      const CommonStore &c, const EconomicFacilityCatalog &k,
      const EconomicFacilitySourcesState &s,
      const ProductionManagementCatalog &p, const EstateCatalog &e,
      const EstateState &estates_value, const PrivateShopCatalog *h,
      const PrivateShopState &shops_value,
      const ProductionFacilityConditionsCatalog &cc,
      const ProductionFacilityConditionsState &cs,
      const FacilityClaimsState &claims_value,
      const FacilityHolderSourceRead &holders_value,
      const EconomicFacilityAppliedBusinessRead &applied_value, WorldMoment now,
      ExactArithmeticBudget budget = {})
      : common(c), catalog(k), state(s), production_assets(p),
        estate_catalog(e), estates(estates_value), shops_catalog(h),
        shops(shops_value), condition_catalog(cc), conditions(cs),
        claims(claims_value), holders(holders_value), applied(applied_value),
        current(std::move(now)), arithmetic(budget) {}
  EconomicFacilityProviderContext(
      const CommonStore &c, const EconomicFacilityCatalog &k,
      const EconomicFacilitySourcesState &s,
      const ProductionManagementCatalog &p, const EstateCatalog &e,
      const EstateState &estates_value, const PrivateShopCatalog &h,
      const PrivateShopState &shops_value,
      const ProductionFacilityConditionsCatalog &cc,
      const ProductionFacilityConditionsState &cs,
      const FacilityClaimsState &claims_value,
      const FacilityHolderSourceRead &holders_value,
      const EconomicFacilityAppliedBusinessRead &applied_value, WorldMoment now,
      ExactArithmeticBudget budget = {})
      : EconomicFacilityProviderContext(
            c, k, s, p, e, estates_value, &h, shops_value, cc, cs, claims_value,
            holders_value, applied_value, std::move(now), budget) {}
};
// 仅变更候选中的预备投影，没有capacity source
// ID；最终不可变源在所有真实holder处置后产生。
struct EconomicFacilityServiceCapacityView {
  FacilitySlotRef slot_ref;
  WorldMoment occurred_at;
  EconomicRuleRef definition_ref;
  StableId place_ref, capacity_unit_id;
  Integer capacity_revision = 0, capacity = 0;
  std::vector<MomentSpan> availability_intervals;
  SourceRef usage_right_ref;
  std::vector<FacilityPhysicalUnitRef> physical_units;
  WorldIdentity identity;
};
struct EconomicFacilityProspectiveCapacity
    : EconomicFacilityServiceCapacityView {
  FacilitySourceRef original_change, previous_capacity_source;
};
struct EconomicFacilityExistingCapacity : EconomicFacilityServiceCapacityView {
  FacilitySourceRef original_capacity_source;
};
// 原 holder 准备结果只包含真实业务采用；正式容量版本由唯一物理终结器晚绑。
class EconomicFacilityPreparedDispositionRead {
public:
  virtual ~EconomicFacilityPreparedDispositionRead() = default;
  virtual bool bound_to(const CommonStore &,
                        const FacilityClaimsState &) const noexcept = 0;
  virtual Status validate_prepared_facility_disposition(
      const FacilitySlotRef &, const PreparedFacilityClaimMutation &) const = 0;
  virtual Outcome<std::optional<FacilityRetentionEntry>>
  read_prepared_facility_retention(
      const FacilityClaim2 &, const PreparedFacilityClaimMutation &) const = 0;
  virtual Outcome<std::vector<FacilityPhysicalUnitRef>>
  read_prepared_facility_admission_units(
      const PreparedFacilityClaimAdmission &,
      const PreparedFacilityClaimMutation &) const = 0;
};
class EconomicFacilityAppliedBusinessWrite {
public:
  virtual ~EconomicFacilityAppliedBusinessWrite() = default;
  virtual Status
  bind_original_facility_dispositions(const FacilitySourceRef &,
                                      const FacilitySlotRef &,
                                      const std::vector<FacilitySourceRef> &,
                                      const FacilityClaimReceiptRef &) = 0;
};
struct EconomicFacilityReconciliationWriteContext {
  const EconomicFacilityProviderContext &read;
  ProductionFacilityConditionsState &conditions;
  EconomicFacilityAppliedBusinessWrite &applied;
};
// Historical definition at the actual original change cut. Replays the
// immutable capacity predecessor and that original activation, never today's
// facility definition or a later finalizer result.
Outcome<EconomicFacilitySlotDefinition>
read_economic_facility_definition_after_original_change(
    const EconomicFacilityProviderContext &, const FacilitySlotRef &,
    const FacilitySourceRef &);
// Existing capacity for an actual owner review; this neither invents a change
// nor allocates a capacity revision. The owner separately proves its due cause.
Outcome<EconomicFacilityExistingCapacity>
read_economic_facility_existing_capacity(
    const EconomicFacilityProviderContext &, const FacilitySlotRef &,
    const FacilitySourceRef &, const MomentSpan &);
Outcome<EconomicFacilityExistingCapacity>
read_economic_facility_capacity_before_owner_source(
    const EconomicFacilityProviderContext &, const FacilitySlotRef &,
    const SourceRef &actual_owner_source, const MomentSpan &);
// Same actual projection at the original cause cut for cold owner decisions.
Outcome<EconomicFacilityProspectiveCapacity>
read_economic_facility_capacity_at_original_change(
    const EconomicFacilityProviderContext &, const FacilitySlotRef &,
    const FacilitySourceRef &, const MomentSpan &);
Outcome<EconomicFacilityProspectiveCapacity>
read_prospective_economic_facility_capacity(
    const EconomicFacilityProviderContext &, const FacilitySlotRef &,
    const FacilitySourceRef &original_change, const MomentSpan &);
Outcome<FacilityClaimMutationReceipt>
finalize_economic_facility_reconciliation_in_candidate(
    CommonStore &, EconomicFacilitySourcesState &,
    EconomicFacilityReconciliationWriteContext &, const FacilitySlotRef &,
    const FacilitySourceRef &original_change,
    const std::vector<FacilityClaimMutationReceipt> &owner_results);
// 原到期先读完整 holder 准备结果；实际 retention
// 才产生新容量，正常续期保原容量。
Outcome<FacilityClaimMutationReceipt>
finalize_economic_facility_until_in_candidate(
    CommonStore &, EconomicFacilitySourcesState &,
    EconomicFacilityReconciliationWriteContext &,
    const FacilityClaimUntilRequest &,
    const std::vector<PreparedFacilityClaimMutation> &,
    const EconomicFacilityPreparedDispositionRead &);
class NativeEconomicFacilitySourceRead final
    : public FacilityClaimSourceRead,
      public EconomicFacilityOriginalSourceRead {
  const EconomicFacilityProviderContext &x_;

public:
  explicit NativeEconomicFacilitySourceRead(
      const EconomicFacilityProviderContext &x)
      : x_(x) {}
  Outcome<EconomicFacilitySlotDefinition>
  read_facility_slot_definition(const FacilitySlotRef &) const override;
  Outcome<EconomicFacilitySlotDefinition>
  read_facility_slot_definition_at_source(
      const FacilitySourceRef &, const FacilitySlotRef &) const override;
  Outcome<std::vector<FacilityPhysicalUnitRef>>
  read_physical_units_at_capacity_source(
      const FacilitySourceRef &, const FacilitySlotRef &) const override;
  Outcome<FacilityPhysicalBindingProof>
  read_physical_binding_proof(const StableId &) const override;
  Outcome<FacilityRetentionProof>
  read_facility_retention_proof(const FacilitySourceRef &,
                                const FacilitySlotRef &,
                                const MomentSpan &) const override;
  Outcome<std::vector<VerifiedFacilityCapacitySegment>>
  read_economic_facility_capacity_source(const FacilitySourceRef &,
                                         const FacilitySlotRef &,
                                         const MomentSpan &) const override;
  Outcome<std::vector<FacilityCapacityChange>>
  enumerate_economic_facility_capacity_changes(
      const FacilitySlotRef &, const WorldMoment &,
      const WorldMoment &) const override;
  Outcome<WorldMoment>
  initial_source_at(const FacilitySlotRef &) const override;
  Outcome<std::vector<VerifiedFacilityCapacitySegment>>
  read_verified_capacity(const FacilitySourceRef &, const FacilitySlotRef &,
                         const MomentSpan &) const override;
  Outcome<FacilityCapacityChange>
  read_capacity_history_base(const FacilitySlotRef &,
                             const FacilityHistoryBase &) const override;
  Outcome<FacilitySourceRef> read_capacity_reconciliation_source(
      const FacilitySlotRef &, const FacilityCapacityChange &) const override;
  Outcome<FacilityMutationCapacityOrder> compare_mutation_to_capacity_change(
      const FacilitySlotRef &, const FacilityClaimMutationReceipt &,
      const FacilityCapacityChange &) const override;
  Outcome<FacilitySourceRef>
  capacity_source_at(const FacilitySlotRef &,
                     const WorldMoment &) const override;
  Outcome<std::vector<FacilityCapacityChange>>
  enumerate_capacity_changes(const FacilitySlotRef &, const WorldMoment &,
                             const WorldMoment &) const override;
  Outcome<std::vector<FacilityClaimDependency>>
  enumerate_claim_dependencies(const FacilitySlotRef &) const override;
  Status
  validate_claim_admission(const FacilityClaim2 &,
                           const FacilityClaimMutationReceipt &) const override;
  Status
  validate_claim_close(const FacilityClaim2 &, FacilityClaimCloseReason,
                       const FacilityClaimMutationReceipt &) const override;
  Status
  validate_mutation_source(const FacilitySlotRef &,
                           const FacilityClaimMutationReceipt &) const override;
  Status
  validate_pruned_prefix(const FacilitySlotRef &,
                         const VacantPrunedFacilityPrefix &) const override;
  Status validate_retention_and_physical_occupancy(
      const FacilitySlotRef &, const MomentSpan &,
      const std::vector<const FacilityClaim2 *> &,
      const FacilityCapacitySource2 &) const override;
  Outcome<Integer> read_additional_normal_capacity(
      const FacilitySlotRef &, const MomentSpan &,
      const std::vector<const FacilityClaim2 *> &,
      const FacilityCapacitySource2 &) const override;
  Outcome<bool>
  is_verified_retained_claim(const FacilityClaim2 &, const MomentSpan &,
                             const FacilityCapacitySource2 &) const override;
  Outcome<bool> source_change_affects_claims(
      const FacilityCapacityChange &,
      const std::vector<const FacilityClaim2 *> &) const override;
  Outcome<bool>
  conflicting_source_visible(const ReservationRef &,
                             const FacilityClaim2 &) const override;
  Status validate_source(const EconomicFacilityCapacitySource &) const;
  Status validate_installation(const PhysicalServiceFacilityExtension &) const;
};
// 初始化只接实际InitialWorldSeed锁行。动态登记只能消费原已应用activation。
Outcome<StableId> install_economic_facility_in_candidate(
    CommonStore &, EconomicFacilitySourcesState &,
    const EconomicFacilityProviderContext &, const StableId &seed_row_key);
// Fresh selected /1 only. The actual empty-cut root is populated from every
// locked initial row at the captured install moment, then its original
// condition seeds. This is not restoration and supplies no stand-in
// holder/business authority.
Status initialize_economic_facilities_in_candidate(
    CommonStore &, EconomicFacilitySourcesState &,
    ProductionFacilityConditionsState &,
    const EconomicFacilityInstallationReadContext &,
    const ProductionFacilityConditionsCatalog &, const FacilityClaimsState &,
    Integer selected_version);
Outcome<StableId> record_economic_facility_capacity_in_candidate(
    CommonStore &, EconomicFacilitySourcesState &,
    const EconomicFacilityProviderContext &, const FacilitySlotRef &,
    const EconomicFacilityChangeCause &, const FacilityAdmissionDisposition &);
Status record_facility_physical_binding_in_candidate(
    EconomicFacilitySourcesState &, const EconomicFacilityProviderContext &,
    const FacilityClaim2 &, const FacilityClaimMutationReceipt &,
    const std::vector<FacilityPhysicalUnitRef> &);
Outcome<StableId> record_facility_retention_in_candidate(
    CommonStore &, EconomicFacilitySourcesState &,
    const EconomicFacilityProviderContext &, const FacilitySlotRef &,
    const FacilitySourceRef &cause, const std::vector<FacilityRetentionEntry> &,
    const std::vector<FacilitySourceRef> &normal_results);
Status validate_production_facility_capacity_adoptions(
    const EconomicFacilityProviderContext &);
Status
validate_economic_facility_sources(const EconomicFacilityProviderContext &);
BoundaryValue
encode_economic_facility_sources(const EconomicFacilitySourcesState &);
Outcome<EconomicFacilitySourcesState>
decode_economic_facility_sources(const BoundaryValue &);
Status emit_economic_facility_sources(ProjectionWriter &,
                                      const EconomicFacilitySourcesState &);
Status visit_economic_facility_sources(CodecVisitor &,
                                       EconomicFacilitySourcesState &);
BoundaryValue encode_economic_facility_catalog(const EconomicFacilityCatalog &);
Outcome<EconomicFacilityCatalog>
decode_economic_facility_catalog(const BoundaryValue &);
// Authored ingress has no self-referential lock identity. Only this explicitly
// selected entrance binds the actual preparation identity; central must
// finalize its real compiled catalog lock before installation. Physical restore
// above requires and preserves the serialized full identity unchanged.
Outcome<EconomicFacilityCatalog> decode_authored_economic_facility_catalog(
    const BoundaryValue &, const CatalogIdentity &actual_preparation_identity);
// 作者同目录引用可省略摘要；显式摘要必须匹配实际身份与已解码09原种子。
Outcome<ProductionFacilityConditionsCatalog>
decode_authored_production_facility_conditions_catalog(
    const BoundaryValue &, const CatalogIdentity &actual_preparation_identity,
    const EconomicFacilityCatalog &);
Status emit_economic_facility_catalog(ProjectionWriter &,
                                      const EconomicFacilityCatalog &);
Status visit_economic_facility_catalog(CodecVisitor &,
                                       EconomicFacilityCatalog &);
} // namespace hanmo::resident
