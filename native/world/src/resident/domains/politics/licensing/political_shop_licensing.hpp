// 私铺许可只证明具名辖区的通缉查核与经营准入；租赁、钱货和铺位产权仍由Commerce拥有。
#pragma once
#include "resident/domains/commerce/private_shop_contract.hpp"
#include "resident/domains/family/family_death.hpp"
#include "resident/domains/interaction/dialogue_provider_source.hpp"
#include "resident/domains/politics/politics_model.hpp"
namespace hanmo::resident {
class PrivateShopOpeningProvenanceSink;
class PoliticalShopLicensingOriginalSink;
struct PoliticalTakeoverCatalog;
struct PoliticalTakeoverState;
class PoliticalShopRegistrySuccessionRead;
class OrganizationAuthorityClock;
class CurrentMomentRead;
class OriginalWorldReadBoundary;
struct CharacterConditionSources;
struct PrivateShopOpeningSources3;
struct MilitaryState;
struct MilitaryCatalog;
struct MilitaryAccess;
// 控制证据只借军事与接管原账，不含originals，避免私铺责任枚举反借法律形成递归。
struct PoliticalShopControlContext {
  const CommonStore &common;
  const PoliticalTakeoverCatalog &catalog;
  const PoliticalTakeoverState &state;
  const MilitaryState &military;
  const MilitaryCatalog &military_catalog;
  const MilitaryAccess &military_access;
  // 只扩原军史观察上界；司法事实和开设截面仍不得超过Common已提交时钟。
  const DynamicStepInput *phase = nullptr;
};
inline constexpr const char *political_shop_licensing_module =
    "political_shop_licensing";
inline constexpr const char *political_shop_permit_kind =
    "political_shop_permit_issued";
inline constexpr const char *political_shop_clearance_kind =
    "political_shop_clearance_recorded";
// 完整初册仅在显式作者辖区内有效；空册与未声明/未初始化辖区严格区分。
struct PoliticalWantedSeed {
  StableId actor_id, reason_code;
};
struct PoliticalShopCourt {
  StableId id, jurisdiction_id, office_id, role_id, dialogue_id,
      request_node_id;
  std::vector<StableId> premises_ids, warrant_reason_codes;
  std::vector<PoliticalWantedSeed> initial_wanted;
  bool complete_registry = false;
  Integer permit_ticks = 20;
};
struct PoliticalShopLicensingCatalog {
  Integer version = 1;
  OrderedTable<PoliticalShopCourt> courts;
};
struct PoliticalShopRegistry {
  StableId id, court_id;
  Integer tick = 0;
  // 显式许可2的真实发生时刻；旧1必须为空，不回填历史。
  std::optional<WorldMoment> occurred_at;
};
// initial仅对应完整作者初册；普通登记必须保存实际签发人、同一专项Grant和原事实。
struct PoliticalWantedRecord {
  StableId id, court_id, actor_id, reason_code, registry_fact_id, grant_id;
  std::optional<ActingAuthority> authority;
  Integer tick = 0;

  // 显式许可2的真实发生时刻；旧1必须为空，不回填历史。
  std::optional<WorldMoment> occurred_at;
};
struct PoliticalWantedRelease {
  StableId id, warrant_fact_id, grant_id;
  ActingAuthority authority;
  Integer tick = 0;

  // 显式许可2的真实发生时刻；旧1必须为空，不回填历史。
  std::optional<WorldMoment> occurred_at;
};
// 柜台永久来源保实际说话人和原节点；今日role换任不会改写当时的Office签字。
struct PoliticalShopClearance {
  StableId id, court_id, registry_fact_id, grant_id;
  PrivateShopPermissionRequest request;
  ActingAuthority authority;
  StableId session_id, role_id, dialogue_id, node_id;
  Integer tick = 0, valid_through_tick = 0;

  // 显式许可2的真实发生时刻；旧1必须为空，不回填历史。
  std::optional<WorldMoment> occurred_at;
};
struct PoliticalShopPermit {
  StableId id, clearance_fact_id;
  Integer tick = 0;
  // 显式许可2的真实发生时刻；旧1必须为空，不回填历史。
  std::optional<WorldMoment> occurred_at;
};
struct PoliticalShopRevocation {
  StableId id, permit_fact_id, grant_id;
  ActingAuthority authority;
  Integer tick = 0;
  // 显式许可2的真实发生时刻；旧1必须为空，不回填历史。
  std::optional<WorldMoment> occurred_at;
};
struct PoliticalShopLicensingState {
  Integer version = 1;
  OrderedTable<PoliticalShopRegistry> registries;
  OrderedTable<PoliticalWantedRecord> warrants;
  OrderedTable<PoliticalWantedRelease> releases;
  OrderedTable<PoliticalShopClearance> clearances;
  OrderedTable<PoliticalShopPermit> permits;
  OrderedTable<PoliticalShopRevocation> revocations;
};
// 全部引用必须借同一World候选；premises仅借Commerce原目录，不生成平行铺位或Owner。
struct PoliticalShopLicensingContext {
  const CommonStore &common;
  const CommonCatalog &common_catalog;
  const PoliticsState &politics;
  const PoliticsCatalog &politics_catalog;
  const OrganizationState &organizations;
  const OrganizationCatalog &organization_catalog;
  const LocalEconomyState &economy;
  const ActivityRead &activity;
  const InteractionState &interactions;
  const InteractionCatalog &interaction_catalog;
  const InteractionAccess &interaction_access;
  const PoliticalShopLicensingCatalog &catalog;
  const PoliticalShopLicensingState &state;
  const OrderedTable<PrivateShopPremises> &premises;
  const PoliticalShopControlContext *takeover = nullptr;
  // 原Military死因读，不用Family后来登记的时刻反造签字时在世。
  FamilyDeathOriginRead death_origin;
  const DynamicStepInput *phase = nullptr;
  // 独立接册profile才绑定；缺省仍沿原RegistryContinuityRequired，不继承任何旧法官签权。
  const PoliticalShopRegistrySuccessionRead *registry_successions = nullptr;
  const OrganizationAuthorityClock *clock = nullptr;
  const CharacterConditionSources *character_sources = nullptr;
  const CurrentMomentRead *moment_clock = nullptr;
  const PrivateShopOpeningSources3 *opening_sources = nullptr;
  // 仅同步只读访问栈借；普通拥有图必须为空，不持久化观察者。
  PoliticalShopLicensingOriginalSink *original_sink = nullptr;
  // 原同World实际观察边界，不是可自选时间的clock；旧1/初册bootstrap不借此证。
  const OriginalWorldReadBoundary *read_boundary2 = nullptr;
};
struct PoliticalRequestShopPermission {
  StableId court_id, premises_id;
};
struct PoliticalRegisterWanted {
  ActingAuthority authority;
  StableId court_id, actor_id, reason_code;
};
struct PoliticalReleaseWanted {
  ActingAuthority authority;
  StableId warrant_fact_id;
};
struct PoliticalRevokeShopPermit {
  ActingAuthority authority;
  StableId permit_fact_id;
};
using PoliticalShopLicensingCommand =
    std::variant<PoliticalRequestShopPermission, PoliticalRegisterWanted,
                 PoliticalReleaseWanted, PoliticalRevokeShopPermit>;
struct PoliticalShopLicensingQuote {
  WorldIdentity identity;
  StableId actor_id, grant_id;
  PoliticalShopLicensingCommand command;
  std::optional<AuthoredProviderSourceProof> meeting;
  std::optional<ActingAuthority> authority;
};
Status validate_political_shop_licensing_catalog(
    const PoliticalShopLicensingContext &);
Status validate_political_shop_licensing(const PoliticalShopLicensingContext &);
// World事实登记和JSON导出仅豁免真实六类本账实体；完整权源史仍由上口先验。
Status validate_political_shop_licensing_fact_header(
    const CommonStore &, const PoliticalShopLicensingState &,
    const FactRecord &);
// 新Done只认本次具名司法事实；Common旧缓存结构与裁剪规则保持原样。
Status validate_political_shop_licensing_receipts(
    const PoliticalShopLicensingContext &,
    const std::function<bool(const CommitResult &)> &is_done);
// 初册只允许真正新局tick0空段；调用方须保Common与本段原子候选。
Status initialize_political_shop_licensing_in_candidate(
    CommonStore &, PoliticalShopLicensingState &,
    const PoliticalShopLicensingContext &);
Outcome<PoliticalShopLicensingQuote>
preview_political_shop_licensing(const PoliticalShopLicensingContext &,
                                 const StableId &actor,
                                 const PoliticalShopLicensingCommand &);
Outcome<std::vector<StableId>> apply_political_shop_licensing_in_candidate(
    CommonStore &, PoliticalShopLicensingState &,
    const PoliticalShopLicensingContext &, const StableId &,
    const PoliticalShopLicensingCommand &, const PoliticalShopLicensingQuote &);
// 只受理真实private_shop_opened截面，不能用任意fact或今日许可倒填过去开设。
class PoliticalPrivateShopPermissionAccess final
    : public PrivateShopPermissionRead {
public:
  explicit PoliticalPrivateShopPermissionAccess(
      PoliticalShopLicensingContext context)
      : context_(context) {}
  Outcome<PrivateShopPermissionSource> private_shop_opening_permission(
      const PrivateShopPermissionRequest &) const override;
  Outcome<PrivateShopPermissionSource>
  private_shop_opening_permission_before_fact(
      const PrivateShopPermissionRequest &, const StableId &,
      Integer) const override;
  // 与原full算法同一来源；正式Shop3持有字段租约，不返回默认可用许可。
  Outcome<OwnedPrivateShopPermissionSource>
  private_shop_opening_permission_owned(
      const PrivateShopPermissionRequest &) const override;
  Outcome<OwnedPrivateShopPermissionSource>
  private_shop_opening_permission_before_fact_owned(
      const PrivateShopPermissionRequest &, const StableId &,
      Integer) const override;
  Status visit_opening_permission_provenance(
      const PrivateShopPermissionRequest &, const StableId &actual_opening,
      Integer opened_tick, PrivateShopOpeningProvenanceSink &) const override;

private:
  PoliticalShopLicensingContext context_;
};
StableId
political_shop_licensing_command_kind(const PoliticalShopLicensingCommand &);
BoundaryValue
encode_political_shop_licensing_command(const PoliticalShopLicensingCommand &);
// 报价只投影本次本人参数与真实签权，不在页面序列化短栈借用对象。
BoundaryValue
encode_political_shop_licensing_quote(const PoliticalShopLicensingQuote &);
Outcome<PoliticalShopLicensingCommand>
decode_political_shop_licensing_command(const StableId &,
                                        const BoundaryValue &);
BoundaryValue
encode_political_shop_licensing_catalog(const PoliticalShopLicensingCatalog &);
Outcome<PoliticalShopLicensingCatalog>
decode_political_shop_licensing_catalog(const BoundaryValue &);
BoundaryValue
encode_political_shop_licensing(const PoliticalShopLicensingState &);
Outcome<PoliticalShopLicensingState>
decode_political_shop_licensing(const BoundaryValue &);
Status visit_political_shop_licensing_checkpoint(CodecVisitor &,
                                                 PoliticalShopLicensingState &);
} // namespace hanmo::resident
