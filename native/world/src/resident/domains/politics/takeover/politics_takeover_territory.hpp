// 实际控制只消费同一次军事应用和既有责任登记；政治归属与任命原快照不改。
#pragma once
#include "politics_takeover_rules.hpp"
#include "resident/domains/politics/politics_territory_contract.hpp"
namespace hanmo::resident {
struct GovernanceProgramState;
struct PoliticalBudgetState;
struct BudgetAuthorizationSource;
struct GovernanceProgramApproval;
struct PoliticalHeadHandoverContext;
struct PoliticalSuccessionPolicy;
struct NamedMilitaryCustodyRelease;
struct FieldExactContactReadContext;
struct FieldBattleCatalog;
Status apply_political_takeover_control_in_candidate(
    const CommonStore &, PoliticsState &, const PoliticalTakeoverCatalog &,
    const PoliticalTakeoverState &, const OccupationAppliedSource &,
    const StableId &registration_fact_id);
class PoliticalTakeoverTerritoryAccess final : public PoliticalTerritoryRead {
public:
  explicit PoliticalTakeoverTerritoryAccess(PoliticalTakeoverContext context)
      : context_(context) {}
  Outcome<std::optional<PoliticalTerritoryControl>>
  jurisdiction_control(const StableId &) const override;
  Outcome<Integer> jurisdiction_revision_before_fact(const StableId &,
                                                     const StableId &,
                                                     Integer) const override;
  // 原治理批准的具名切点。不是任命fact的别名，也不以今日控制倒填历史。
  // 返回revision=0时仍有原PoliticsCatalog行及完整接管源覆盖证明。
  Outcome<PoliticalTerritoryControl>
  governance_jurisdiction_control_current(const StableId &) const;
  // 暂定内部接缝：只逆查原治所所在 S1
  // 节点；非治所节点无绑定则拒，不猜下辖范围。 调用方仍须枚举完整同图 S1
  // 节点，不能把本读口有结果的子集当全集。
  Outcome<const StableId *>
  military_settlement_jurisdiction(const StableId &) const;
  // 同据点可有多个接任规则；只聚合相同原辖区映射，不选择其中一个接任者。
  // 返回值借原锁定目录，缺映射或不同辖区/site/garrison 明确拒绝。
  Outcome<const StableId *>
  military_stronghold_jurisdiction(const StableId &) const;
  // 仅同 Military 原关闭行与其真 Common 切点可借历史控制，不验证完整释放业务。
  Outcome<PoliticalTerritoryControl>
  military_jurisdiction_control_before_custody_release(
      const NamedMilitaryCustodyRelease &, const StableId &jurisdiction) const;
  Outcome<PoliticalTerritoryControl>
  governance_jurisdiction_control_before_approval(
      const GovernanceProgramState &, const GovernanceProgramApproval &) const;
  Status validate_head_handover_seat2(const PoliticalHeadHandoverContext &,
                                      const PoliticalSuccessionPolicy &,
                                      Integer projected_tick,
                                      const StableId &actual_boundary) const;

  Outcome<PoliticalTerritoryControl>
  public_budget_jurisdiction_control_before_approval(
      const PoliticalBudgetState &, const BudgetAuthorizationSource &,
      const StableId &jurisdiction) const;

  // Only a retained actual exact initial Field contact may define this cut.
  // S1 geometry/contact identity is read by its original military owner; no
  // local battle seconds, current controller, or legacy ratio becomes a M.
  Outcome<PoliticalTerritoryControl>
  military_jurisdiction_control_before_initial_field_contact(
      const FieldExactContactReadContext &, const FieldBattleCatalog &,
      const SourceRef &actual_field_contact,
      const StableId &jurisdiction) const;

  // 热安装尚未产生 Field 头；只借同候选实际当前 Common 前缀与 S0 时钟。
  // 安装后的冷验证必须走 initial_field_contact 的原实际切点，不持久化当前读权。
  Outcome<PoliticalTerritoryControl>
  military_jurisdiction_control_before_field_contact_install(
      const FieldExactContactReadContext &, const StableId &jurisdiction) const;

private:
  Outcome<PoliticalTerritoryControl>
  control_before_verified_original(const StableId &jurisdiction,
                                   const StableId &actual_source_id,
                                   const WorldMoment &actual_at) const;
  PoliticalTakeoverContext context_;
};
} // namespace hanmo::resident
