// 政治辖区与受任历史只引用共同人物、公库和组织Office；不复制持有人或资产余额。
#pragma once
#include "resident/domains/organization/organization_model.hpp"
namespace hanmo::resident {
class PoliticalTerritoryRead;
class OrganizationAuthorityClock;
struct PoliticalOrganizationRuntimeReadContext;
struct PoliticalOrganizationRuntimeWriteContext;
enum class AppointmentStatus {
  offered,
  accepted,
  declined,
  withdrawn,
  expired,
  arrived,
  invalidated
};
// 政治归属与实际控制分别声明；本版作者只允许同属稳定辖区，后续占领业务显式扩展。
struct Jurisdiction {
  StableId id, name_key, office_name_key, seat_place_id,
      political_organization_id, controlling_organization_id, office_id;
  StableId public_wallet_id;
  std::vector<StableId> public_inventory_ids;
  Integer revision = 0;
};
// 期限是作者选定的受任时窗，不凭UI生成即刻到任或任命资格。
struct AppointmentPolicy {
  StableId id, name_key;
  Integer response_ticks = 0, arrival_ticks = 0, minimum_administration = 0,
          minimum_merit = 0;
};
struct PoliticalAppointment {
  StableId id, jurisdiction_id, policy_id, actor_id, issuer_actor_id,
      issuer_office_id, organization_id;
  Integer issuer_epoch = 0, target_office_epoch = 0, jurisdiction_revision = 0,
          created_tick = 0, expires_tick = 0, revision = 0;
  AppointmentStatus status = AppointmentStatus::offered;
  StableId offered_fact_id, last_fact_id;
};
// 每次决定均为永久政治事实；任命链与回执不依赖可裁剪命令历史。
struct PoliticsFact {
  StableId id, kind, jurisdiction_id, appointment_id, actor_id, issuer_actor_id,
      organization_id, office_id;
  StableId decision_actor_id;
  std::vector<StableId> witness_actor_ids;
  Integer tick = 0, office_epoch = 0;
  AppointmentStatus status = AppointmentStatus::offered;
  // physical /2原fact真发生时刻；/1为空，不能从tick或今日M回灌。
  std::optional<WorldMoment> occurred_at;
  std::vector<StableId> cause_fact_ids;
};
struct PoliticsCatalog {
  bool enabled = false;
  OrderedTable<Jurisdiction> jurisdictions;
  OrderedTable<AppointmentPolicy> appointment_policies;
  OrderedTable<StableId> command_name_keys;
};
struct PoliticsState {
  OrderedTable<Jurisdiction> jurisdictions;
  OrderedTable<PoliticalAppointment> appointments;
  OrderedTable<PoliticsFact> facts;
};
// 命令携带读取时的辖区/任职版本，过期按钮必须拒绝；command_actor另由中央身份核验。
struct PoliticalAppointCommand {
  ActingAuthority authority;
  StableId jurisdiction_id, target_actor_id, policy_id;
  Integer expected_jurisdiction_revision = 0, expected_office_epoch = 0;
};
struct PoliticalAnswerAppointmentCommand {
  StableId actor_id, appointment_id;
  Integer expected_revision = 0;
  bool accept = false;
  StableId source_fact_id;
};
struct PoliticalArriveCommand {
  StableId actor_id, appointment_id;
  Integer expected_revision = 0;
  StableId source_fact_id;
};
struct PoliticalWithdrawCommand {
  ActingAuthority authority;
  StableId appointment_id;
  Integer expected_revision = 0;
};
struct PoliticalDismissCommand {
  ActingAuthority authority;
  StableId jurisdiction_id;
  Integer expected_jurisdiction_revision = 0, expected_office_epoch = 0;
};
using PoliticsCommand =
    std::variant<PoliticalAppointCommand, PoliticalAnswerAppointmentCommand,
                 PoliticalArriveCommand, PoliticalWithdrawCommand,
                 PoliticalDismissCommand>;
struct PoliticsEffects {
  StableId appointment_id;
  std::vector<StableId> fact_ids, receipt_ids;
};
// 报价冻结同一typed命令和候选身份，不把一次授权证明变成可长期复用的公权。
struct PoliticsQuote {
  WorldIdentity identity;
  StableId actor_id;
  PoliticsCommand command;
  Integer duration = 0;
  std::vector<ReadVersion> resource_versions;
};
// 命令的全部依赖只借同一候选，失败由本叶savepoint和暂存组织/政治状态共同回滚。
Status preview_politics_command(
    const CommonStore &, const PoliticsState &, const PoliticsCatalog &,
    const OrganizationState &, const OrganizationCatalog &,
    const LocalEconomyState &, const ActivityRead &,
    const StableId &command_actor, const PoliticsCommand &,
    const PoliticalOrganizationRuntimeReadContext * = nullptr);
Outcome<PoliticsEffects>
apply_politics_command(CommonStore &, PoliticsState &, const PoliticsCatalog &,
                       OrganizationState &, const OrganizationCatalog &,
                       const LocalEconomyState &, const ActivityRead &,
                       const StableId &command_actor, const PoliticsCommand &);
// 调用方须持完整候选/子事务，失败须回滚共同状态；禁止独立调用。
Outcome<PoliticsEffects> apply_politics_command_in_transaction(
    CommonStore &, PoliticsState &, const PoliticsCatalog &,
    OrganizationState &, const OrganizationCatalog &, const LocalEconomyState &,
    const ActivityRead &, const StableId &command_actor,
    const PoliticsCommand &,
    const PoliticalOrganizationRuntimeWriteContext * = nullptr);
Outcome<PoliticsEffects>
reconcile_politics(CommonStore &, PoliticsState &, const PoliticsCatalog &,
                   const OrganizationState &, const OrganizationCatalog &,
                   const LocalEconomyState &,
                   const PoliticalOrganizationRuntimeWriteContext * = nullptr);
Outcome<PoliticsState> make_initial_politics(const CommonStore &,
                                             const CommonCatalog &,
                                             const PoliticsCatalog &,
                                             const OrganizationState &,
                                             const OrganizationCatalog &);
Status validate_politics(const CommonStore &, const CommonCatalog &,
                         const PoliticsState &, const PoliticsCatalog &,
                         const OrganizationState &,
                         const OrganizationCatalog &);
// 仅显式接管模块提供实际控制与历史签发时版本，旧调用仍严格原规则。
Status validate_politics(const CommonStore &, const CommonCatalog &,
                         const PoliticsState &, const PoliticsCatalog &,
                         const OrganizationState &, const OrganizationCatalog &,
                         const PoliticalTerritoryRead *,
                         const OrganizationAuthorityClock * = nullptr);
std::optional<DynamicFactView> politics_dynamic_fact(const PoliticsState &,
                                                     const StableId &);
Outcome<PoliticsCatalog> decode_politics_catalog(const BoundaryValue &);
Outcome<PoliticsState> decode_politics_state(const BoundaryValue &);
BoundaryValue encode_politics_catalog(const PoliticsCatalog &);
BoundaryValue encode_politics_state(const PoliticsState &);
Status visit_politics_checkpoint(CodecVisitor &, PoliticsState &);
} // namespace hanmo::resident
