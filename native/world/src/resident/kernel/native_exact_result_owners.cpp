#include "native_political_relations.hpp"
#include "resident/kernel/world_stage_observation.hpp"
#include "native_exact_result_owners.hpp"
#include "resident/domains/local_economy/population_selection_source.hpp"
#include "common_fact_prefix.hpp"
#include "exact_complete_world.hpp"
#include "growth_result_projection2.hpp"
#include "military_result_projection2.hpp"
#include "resident/domains/military/field_battle/field_state_codec.hpp"
#include "resident/domains/military/field_battle/field_validation.hpp"
#include "native_actor_social_occupancy.hpp"
#include "native_communication_selection.hpp"
#include "native_complete_world_core.hpp"
#include "native_interaction_world.hpp"
#include "native_organization_runtime.hpp"
#include "native_original_social_world.hpp"
#include "native_private_shop_exact_sources.hpp"
#include "native_private_shop_operations.hpp"
#include "native_production_cycles.hpp"
#include "native_public_supply_inspection.hpp"
#include "native_transport_catalog.hpp"
#include "organization_result_projection2.hpp"
#include "original_catalog_identity.hpp"
#include "political_shop_licensing_integration.hpp"
#include "politics_result_projection2.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/domains/actor_foundation/character_condition_world.hpp"
#include "resident/domains/actor_foundation/condition_application_fact.hpp"
#include "resident/domains/affairs/affairs_model.hpp"
#include "resident/domains/affairs/state_need_period.hpp"
#include "resident/domains/commerce/private_shop_operation_sources.hpp"
#include "resident/domains/family/collateral_control_state.hpp"
#include "resident/domains/interaction/appointments/personal_appointment_time2.hpp"
#include "resident/domains/interaction/contact_inquiries/contact_inquiry_time2.hpp"
#include "resident/domains/interaction/face_to_face_contact_source.hpp"
#include "resident/domains/interaction/face_to_face_report.hpp"
#include "resident/domains/interaction/household_purchase_report.hpp"
#include "resident/domains/interaction/interaction_exact_encounter.hpp"
#include "resident/domains/interaction/occupancy/actor_social_occupancy.hpp"
#include "resident/domains/interaction/visit_appointments/visit_appointment_meeting_time2.hpp"
#include "resident/domains/interaction/visit_appointments/visit_appointment_receipts.hpp"
#include "resident/domains/interaction/visit_messages/visit_original_result_read.hpp"
#include "resident/domains/interaction/visit_messages/visit_report_custody.hpp"
#include "resident/domains/politics/licensing/political_shop_licensing_exact2.hpp"
#include "resident/domains/politics/political_exact_phase.hpp"
#include "resident/domains/politics/treaty/safe_exit_binding_moment.hpp"
#include "resident/domains/politics/treaty/treaty_procedure_moment.hpp"
#include "resident/domains/politics/treaty/treaty_result_projection.hpp"
#include "resident/domains/social/social_original_result_projection.hpp"
#include "resident/domains/tasks/work_execution/work_result_projection.hpp"
#include "resident/domains/travel/movement_action_time.hpp"
#include "world_bundle.hpp"
#include "world_read_stamp.hpp"
#include <algorithm>
#include <new>
#include <stdexcept>
namespace hanmo::resident {
namespace {
bool is(const StableId &v, std::initializer_list<const char *> values) {
  for (auto *x : values)
    if (v == x)
      return true;
  return false;
}
// 只列许可原六表的实际 Common kind；不是任意政治事实 schema 推断。
bool shop_licensing_kind(const StableId &kind) {
  return is(kind, {"political_shop_registry_initialized",
                   "political_wanted_registered", "political_wanted_released",
                   political_shop_clearance_kind, political_shop_permit_kind,
                   "political_shop_permit_revoked"});
}
bool appointment_kind(const SourceRef &ref) {
  return ref.domain == personal_appointment_module &&
         is(ref.kind, {personal_appointment_accepted_kind,
                       personal_appointment_cancelled_kind,
                       personal_appointment_started_kind,
                       personal_appointment_finished_kind});
}
bool visit_meeting_kind(const SourceRef &ref) {
  return ref.domain == visit_appointment_meeting_module &&
         is(ref.kind, {visit_appointment_meeting_started_kind,
                       visit_appointment_meeting_cancelled_kind,
                       visit_appointment_meeting_finished_kind});
}
bool visit_invitation_kind(const SourceRef &ref) {
  return ref.domain == visit_appointment_module &&
         is(ref.kind,
            {visit_appointment_offered_kind, visit_appointment_collected_kind,
             visit_appointment_accepted_kind});
}
bool inquiry_kind(const SourceRef &ref) {
  return ref.domain == contact_inquiry_module &&
         is(ref.kind,
            {contact_inquiry_started_kind, contact_inquiry_finished_kind});
}
bool custody_kind(const StableId &kind) {
  return is(kind, {visit_report_deposited_kind, visit_report_picked_up_kind,
                   visit_report_withdrawn_kind});
}
// 只识别原财政/2 producer 已发布的唯一 kind；实际来源仍由选定 owner 反验。
bool native_political_fiscal_time2_result_kind(const FactRecord &head) {
  return head.domain == "politics" &&
         head.kind == "political_fiscal_policy_changed";
}
bool task_kind(const StableId &kind) {
  return is(kind,
            {"task_action_started", "task_action_finished", "task_delivery",
             "task_transfer", "task_claim_established", "state_task_accepted",
             "state_task_closing", "state_task_settled",
             "task_carrier_agreement_accepted",
             "task_report_carriage_dispatched", "task_report_handed_over",
             "task_objective_accepted", "task_objective_contribution_used",
             "task_objective_report_received"});
}
template <class T> Outcome<T> missing(const char *code, const StableId &id) {
  return {false,
          {},
          common_failure(code, id, ErrorKind::unsupported_capability).error};
}
// P10只借原經濟開期來源；不把內部期間憑據捏造成公開動態成果。
class NativeEconomicCalendarResultRead final
    : public ExactResultOwnerRead,
      public CheckpointAllocatedObject {
  const WorldBundle &world_;
  WorldMoment at_;
  const ActionTimeOriginalSourceRead &actions_;
  const CompleteExactWorldBindingOwner &original_owner_;

  // 每次沿原完整開期reader驗來源、參與集合與原時刻，不以當前tick替代歷史。
  Outcome<WorldMoment> read_original(const MomentOwnerSource &source) const {
    const auto &ref = source.source_ref;
    if (ref.domain != "local_economy" || ref.kind != "economic_period_opened" ||
        source.source_schema != 1)
      return missing<WorldMoment>("UnsupportedEconomicCalendarResultSource",
                                  ref.id);
    const auto &w = world_;
    auto layout = validate_native_production_cycles_layout(w);
    if (!layout.ok)
      return {false, {}, std::move(layout.error)};
    if (!w.production_cycles || !w.actor_foundation || !w.catalog ||
        !w.catalog->exact_time || !w.catalog->complete_exact_time_runtime ||
        !w.catalog->production_cycles || !w.catalog->actor_foundation)
      return missing<WorldMoment>("EconomicCalendarResultOriginalOwnerRequired",
                                  ref.id);
    auto original = original_owner_.read_economic(at_);
    if (!original.ok)
      return {false, {}, std::move(original.error)};
    if (!original.value)
      return missing<WorldMoment>("EconomicCalendarResultOriginalOwnerRequired",
                                  ref.id);
    const auto binding = original.value->bindings();
    const auto &c = binding.original.calendar;
    const auto &p = binding.original.production;
    if (&binding.catalog != w.catalog->local_economy.get() ||
        &c.common != &w.common || &c.economy != &w.local_economy ||
        &c.sources != &*w.production_cycles ||
        &c.foundation != &*w.actor_foundation ||
        &c.foundation_catalog != w.catalog->actor_foundation.get() ||
        &c.organization != &w.organization ||
        &c.action_registrations !=
            &w.catalog->complete_exact_time_runtime->action_times ||
        &c.action_sources != &actions_ ||
        &p.catalog != w.catalog->production_cycles.get() ||
        &p.sources != &c.sources || &p.clock != &c.clock ||
        p.units_per_half_day !=
            w.catalog->exact_time->scale_ref.units_per_half_day)
      return {false,
              {},
              common_failure("EconomicCalendarResultCandidateMismatch", ref.id,
                             ErrorKind::internal)
                  .error};
    auto now = w.actual_phase_moment();
    if (!now.ok)
      return {false, {}, std::move(now.error)};
    auto clock = c.clock.read_current_moment();
    if (!clock.ok)
      return {false, {}, std::move(clock.error)};
    auto current = compare_world_moments(at_, now.value);
    if (!current.ok)
      return {false, {}, std::move(current.error)};
    auto same_clock = compare_world_moments(at_, clock.value);
    if (!same_clock.ok)
      return {false, {}, std::move(same_clock.error)};
    if (current.value || same_clock.value)
      return {false,
              {},
              common_failure("StaleEconomicCalendarResultRead", ref.id).error};
    auto opening = read_economic_period_opening(c, ref, source.source_schema);
    if (!opening.ok)
      return {false, {}, std::move(opening.error)};
    // 租約只活在本次同步來源讀中；只把原時刻深值交給P10，不外洩借用。
    return {true, opening.value->opening_at, {}};
  }

public:
  // 構圖只借外層已持活的同一Complete根；真來源驗讀延後至具名原頭實際被讀。
  NativeEconomicCalendarResultRead(
      const WorldBundle &world, const WorldMoment &at,
      const ActionTimeOriginalSourceRead &actions,
      const CompleteExactWorldBindingOwner &original_owner)
      : world_(world), at_(at), actions_(actions),
        original_owner_(original_owner) {}

  // 原opening_at本來就是完整Moment；返回原值，不推斷或重新執行開期。
  Outcome<WorldMoment>
  read_selected_source_moment(const MomentOwnerSource &source) const override {
    return read_original(source);
  }

  // 非公開頭仍保原support來源可讀；公開Fact/Action查詢維持明確不支持。
  Outcome<ExactOwnerFactRead>
  read_selected_fact(const FactRecord &head) const override {
    const MomentOwnerSource source{{head.domain, head.kind, head.id}, 1};
    auto original = read_original(source);
    if (!original.ok)
      return {false, {}, std::move(original.error)};
    const auto *stored = world_.common.state().facts.find(head.id);
    if (!stored || stored->id != head.id || stored->domain != head.domain ||
        stored->kind != head.kind)
      return {false,
              {},
              common_failure("EconomicCalendarResultHeaderMismatch", head.id)
                  .error};
    ExactOwnerFactRead out;
    out.disposition = ExactOwnerFactRead::Disposition::no_dynamic_projection;
    out.supporting_sources.push_back(source);
    return {true, std::move(out), {}};
  }
};
// 只借同一已发布政治原 owner 反验作者起源；不安装成立、继任或解散写者。
class NativeOrganizationAuthoredResultRead final
    : public ExactResultOwnerRead,
      public CheckpointAllocatedObject {
  const WorldBundle &world_;
  WorldMoment at_;
  std::shared_ptr<const PoliticalExactReadLease> original_;

public:
  // 真租约保活原生命周期来源与读票，不复制组织账，也不回持完整世界 owner。
  NativeOrganizationAuthoredResultRead(
      const WorldBundle &world, const WorldMoment &at,
      std::shared_ptr<const PoliticalExactReadLease> original)
      : world_(world), at_(at), original_(std::move(original)) {}

  // 起源记录只有原作者身份，没有发生时刻；任何时间查询必须明确拒绝。
  Outcome<WorldMoment>
  read_selected_source_moment(const MomentOwnerSource &source) const override {
    return missing<WorldMoment>("OrganizationAuthoredOriginHasNoMoment",
                                source.source_ref.id);
  }

  // 逐次核同根、原票与完整作者来源；静态头不可伪装为动态事实或成功空值。
  Outcome<ExactOwnerFactRead>
  read_selected_fact(const FactRecord &head) const override {
    if (head.domain != "organization" ||
        head.kind != "organization_lifecycle_origin")
      return missing<ExactOwnerFactRead>(
          "OrganizationAuthoredResultSourceRequired", head.id);
    const auto &w = world_;
    auto layout = validate_native_organization_runtime_layout(w);
    if (!layout.ok)
      return {false, {}, std::move(layout.error)};
    if (!original_ || !w.organization_runtime || !w.catalog->organization ||
        !w.catalog->organization_lifecycle || !w.read_stamps)
      return missing<ExactOwnerFactRead>(
          "OrganizationAuthoredResultOriginalOwnerRequired", head.id);
    const auto binding = original_->bindings();
    const auto &x = binding.organization;
    if (&x.runtime != &*w.organization_runtime ||
        &x.lifecycle_catalog != w.catalog->organization_lifecycle.get() ||
        &x.stamp_authority != &*w.read_stamps ||
        &x.market.common != &w.common ||
        &x.market.organization != &w.organization ||
        &x.market.catalog != w.catalog->organization.get() ||
        &x.market.runtime != &*w.organization_runtime ||
        &x.market.economy != &w.local_economy || &x.clock != &x.market.clock ||
        !same_original_catalog_identity(x.market.catalog_identity,
                                        w.catalog->identity))
      return {false,
              {},
              common_failure("OrganizationAuthoredResultCandidateMismatch",
                             head.id, ErrorKind::internal)
                  .error};
    auto actual = w.actual_phase_moment();
    if (!actual.ok)
      return {false, {}, std::move(actual.error)};
    auto clock = x.clock.read_organization_authority_now();
    if (!clock.ok)
      return {false, {}, std::move(clock.error)};
    auto current = compare_world_moments(at_, actual.value);
    if (!current.ok)
      return {false, {}, std::move(current.error)};
    auto same_clock = compare_world_moments(at_, clock.value);
    if (!same_clock.ok)
      return {false, {}, std::move(same_clock.error)};
    if (current.value || same_clock.value)
      return {
          false,
          {},
          common_failure("StaleOrganizationAuthoredResultRead", head.id).error};
    const auto &stamp = x.office_sources.stamp;
    if (stamp.mode() != ReadMode::phase_current &&
        stamp.mode() != ReadMode::committed_current)
      return {false,
              {},
              common_failure("OrganizationAuthoredResultCurrentStampRequired",
                             head.id)
                  .error};
    auto ticket = x.stamp_authority.validate(
        stamp, w.common.state().identity, stamp.mode(), w.common, x.versions);
    if (!ticket.ok)
      return {false, {}, std::move(ticket.error)};
    const auto *stored = w.common.state().facts.find(head.id);
    const auto *origin = x.runtime.lifecycle_origins.find(head.id);
    if (!stored || stored->id != head.id || stored->domain != head.domain ||
        stored->kind != head.kind || !origin || origin->id != head.id)
      return {
          false,
          {},
          common_failure("OrganizationAuthoredResultHeaderMismatch", head.id)
              .error};
    if (!std::holds_alternative<OrganizationAuthoredOrigin>(origin->origin))
      return missing<ExactOwnerFactRead>("NOT_IMPLEMENTED", head.id);
    auto valid = validate_organization_lifecycle(
        w.common, w.organization, x.runtime, *w.catalog->organization,
        x.lifecycle_catalog, stamp, x.lifecycle_sources, actual.value);
    if (!valid.ok)
      return {false, {}, std::move(valid.error)};
    ExactOwnerFactRead out;
    out.disposition = ExactOwnerFactRead::Disposition::no_dynamic_projection;
    return {true, std::move(out), {}};
  }
};
bool condition_application_kind(const FactRecord &head) {
  return head.domain == "actor_foundation" &&
         owns_condition_application_fact_kind(head.kind);
}
Outcome<const ConditionReceiptV2 *>
condition_application(const NativeExactResultAccess &access,
                      const FactRecord &head) {
  if (!access.condition_applications)
    return missing<const ConditionReceiptV2 *>(
        "ConditionApplicationResultOwnerNotSelected", head.id);
  auto sources = access.condition_applications->sources();
  if (!sources.ok)
    return {false, {}, std::move(sources.error)};
  if (&sources.value.common != &access.common)
    return {false,
            {},
            common_failure("ExactConditionApplicationCandidateMismatch",
                           head.id, ErrorKind::internal)
                .error};
  auto header = read_condition_application_fact_header(
      sources.value.common, sources.value.state, sources.value.catalog, head);
  if (!header.ok)
    return {false, {}, std::move(header.error)};
  // 原 API 需要 owning ref；先预留这一次精确 fresh ID，租约只活到反验结束。


  auto memory = CheckpointMemoryCharge(0);
  ConditionReceiptRefV2 ref{checkpoint_fresh_string_copy(head.id)};
  auto original =
      read_character_condition_receipt_provenance(sources.value, ref);
  if (!original.ok)
    return original;
  if (header.value.source_schema != 2 ||
      original.value != header.value.receipt ||
      original.value->effective_at.kind !=
          ActorFoundationAt::Kind::exact_moment)
    return {false,
            {},
            common_failure("ExactConditionApplicationSourceMismatch", head.id,
                           ErrorKind::internal)
                .error};
  auto before = compare_world_moments(original.value->effective_at.exact_moment,
                                      sources.value.actual_now);
  if (!before.ok)
    return {false, {}, std::move(before.error)};
  if (before.value > 0)
    return {
        false,
        {},
        common_failure("ExactConditionApplicationFutureSource", head.id).error};
  return original;
}
} // namespace
struct NativeExactResultOwnerRead::Sources final : CheckpointAllocatedObject {
  const WorldBundle *original_world = nullptr;
  // 自持 complete 或外层真实 core 保活此借用；不另造行动来源图。
  const CompleteExactWorldBindingOwner *original_owner = nullptr;
  // P10 is a borrower of the actual same-World body/Family/Interaction graph,
  // never a prerequisite of constructing it. Do not capture P10 in this ctor.
  std::unique_ptr<CompleteExactWorldBindingOwner> complete;
  // Field 明选的 P10 适配器只借这一真实 Complete owner；逆序析构先退
  // 适配器，再退 complete，不缓存其他 World/clone 的读图或动态选择。
  std::unique_ptr<const ExactResultOwnerRead> field_military_results;
  std::unique_ptr<const ExactResultOwnerRead> field_movement_results;
  // 已有 Core 先存活；Law 的 body/opening 借 exact，不回完整工厂。
  std::unique_ptr<NativePrivateShopExactSources> shop_exact;
  std::unique_ptr<PoliticalShopLicensingWorldAccess> shop_law;
  std::unique_ptr<EconomicSupplyWorldAccess> supply;
  std::unique_ptr<NativePoliticalRelationsWorldRead> political_relations;
  std::optional<WorldCurrentMomentRead> inspection_clock;
  std::optional<PublicSupplyInspectionReadContext> inspection;
};
NativeExactResultOwnerRead::NativeExactResultOwnerRead(
    NativeExactResultAccess access)
    : access_(std::move(access)) {}
NativeExactResultOwnerRead::NativeExactResultOwnerRead(
    std::unique_ptr<Sources> sources, NativeExactResultAccess access)
    : sources_(std::move(sources)), access_(std::move(access)) {}
NativeExactResultOwnerRead::~NativeExactResultOwnerRead() = default;

Outcome<std::unique_ptr<NativeExactResultOwnerRead>>
NativeExactResultOwnerRead::compose(
    const WorldBundle &world,
    const NativeCompleteWorldCoreSources *bound_core) {
  using Result = Outcome<std::unique_ptr<NativeExactResultOwnerRead>>;
  try {
    if (!world.catalog || !world.catalog->exact_time ||
        !exact_complete_selected(*world.catalog->exact_time) ||
        !world.catalog->complete_exact_time_runtime || !world.world_timeline ||
        !world.action_elapsed || !world.movement_exact)
      return missing<std::unique_ptr<NativeExactResultOwnerRead>>(
          "ExactResultOriginalWorldRootsRequired", {});
    // 本图尚未安装旁支 source2/Family 自主 owner；真实选择不能以空根降版。
    for (const auto &module : world.catalog->identity.module_versions) {
      if ((module.first == "family_collateral_legacy" &&
           module.second == "2") ||
          module.first == "director_family_autonomy")
        return missing<std::unique_ptr<NativeExactResultOwnerRead>>(
            "NOT_IMPLEMENTED", module.first);
    }
    // 仅复用原物理1形状门；完整目录、Common及原家族来源校验仍由原图负责。
    auto collateral_shape =
        validate_collateral_control_shape(world.family_collateral_legacy, 1);
    if (!collateral_shape.ok)
      return {false, {}, std::move(collateral_shape.error)};
    auto sources = std::unique_ptr<NativeExactResultOwnerRead::Sources>(
        new NativeExactResultOwnerRead::Sources);
    sources->original_world = &world;
    const CompleteExactWorldBindingOwner *original_owner = nullptr;
    if (bound_core) {
      if (&bound_core->characters.early.world != &world)
        return missing<std::unique_ptr<NativeExactResultOwnerRead>>(
            "ExactResultBoundWorldRequired", {});
      original_owner = &bound_core->characters.early.complete;
      const auto &actual = original_owner->reads();
      if (actual.original_core != bound_core ||
          &actual.action_sources != &bound_core->action_time)
        return missing<std::unique_ptr<NativeExactResultOwnerRead>>(
            "ExactResultBoundOriginalCoreRequired", {});
    } else {
      auto complete = world.complete_exact_time_bindings();
      if (!complete.ok)
        return {false, {}, std::move(complete.error)};
      if (!complete.value)
        return missing<std::unique_ptr<NativeExactResultOwnerRead>>(
            "ExactResultCompleteOwningLeaseRequired", {});
      sources->complete = std::move(complete.value);
      original_owner = sources->complete.get();
    }
    sources->original_owner = original_owner;
    const auto &binding = original_owner->reads();
    if (&binding.runtime != world.catalog->complete_exact_time_runtime.get() ||
        &binding.movement != &*world.movement_exact)
      return {false,
              {},
              common_failure("ExactResultOwningCandidateMismatch", {},
                             ErrorKind::internal)
                  .error};
    NativeExactResultAccess access{world.common, world.tasks, world.affairs,
                                   world.social};
    // 两个 factory 由真实目录显式选择；未选返回空，不用历史行是否非空
    // 猜模块。fact 与 source-moment 都复用同一原 owner，旧域分派仍保留。
    if (binding.original_core) {
      auto field_military = make_native_field_military_result_read(
          world, *original_owner, *binding.original_core);
      if (!field_military.ok)
        return {false, {}, std::move(field_military.error)};
      sources->field_military_results = std::move(field_military.value);
      auto field_movement = make_native_field_movement_result_read(
          world, *original_owner, *binding.original_core);
      if (!field_movement.ok)
        return {false, {}, std::move(field_movement.error)};
      sources->field_movement_results = std::move(field_movement.value);
    } else {
      // 非 Field 的旧窄绑定不因此新增 core 要求；真实 Field 选择不可空降。
      auto field_selected = selected_field_battle_version(world.catalog->identity);
      if (!field_selected.ok)
        return {false, {}, std::move(field_selected.error)};
      if (field_selected.value)
        return missing<std::unique_ptr<NativeExactResultOwnerRead>>(
            "FieldResultOriginalCoreRequired", {});
    }
    auto relations_layout = validate_native_political_relations_layout(world);
    if (!relations_layout.ok)
      return {false, {}, std::move(relations_layout.error)};
    if (world.political_relations) {
      auto original = NativePoliticalRelationsWorldRead::create(world);
      if (!original.ok)
        return {false, {}, std::move(original.error)};
      sources->political_relations = std::move(original.value);
    }
    auto inspection_layout = validate_native_public_supply_inspection_layout(
        *world.catalog, world.common,
        world.public_supply_inspection ? &*world.public_supply_inspection
                                       : nullptr);
    if (!inspection_layout.ok)
      return {false, {}, std::move(inspection_layout.error)};
    if (world.public_supply_inspection) {
      sources->inspection_clock.emplace(world);
      sources->inspection.emplace(PublicSupplyInspectionReadContext{
          world.common, world.social, *world.catalog->interaction,
          *world.catalog->local_economy, world.local_economy,
          *world.catalog->public_supply_inspection,
          *world.public_supply_inspection, *sources->inspection_clock});
      access.public_supply_inspection = &*sources->inspection;
    }
    auto condition_selected =
        condition_application_facts_selected(world.catalog->identity);
    if (!condition_selected.ok)
      return {false, {}, std::move(condition_selected.error)};
    if (condition_selected.value) {
      auto body = binding.character_conditions.sources();
      if (!body.ok)
        return {false, {}, std::move(body.error)};
      if (!world.actor_foundation || !world.catalog->actor_foundation ||
          &body.value.common != &world.common ||
          &body.value.state != &*world.actor_foundation ||
          &body.value.catalog != world.catalog->actor_foundation.get() ||
          !same_original_catalog_identity(body.value.catalog.identity,
                                          world.catalog->identity))
        return {false,
                {},
                common_failure("ExactConditionApplicationCandidateMismatch", {},
                               ErrorKind::internal)
                    .error};
      access.condition_applications = &binding.character_conditions;
    }
    access.interaction = &world.interaction;
    access.interaction_catalog = world.catalog->interaction.get();
    // 正式根仍是原三表通讯；未施工的 /2 不能借旧根或空 owner 冒充。
    auto communication_version =
        selected_native_communication_links_version(world.catalog->identity);
    if (!communication_version.ok)
      return {false, {}, std::move(communication_version.error)};
    if (communication_version.value == 2)
      return missing<std::unique_ptr<NativeExactResultOwnerRead>>(
          "NOT_IMPLEMENTED", "communication_links/2");
    access.communication = world.interaction.communication_links
                               ? &*world.interaction.communication_links
                               : nullptr;
    access.social_periods =
        world.social_period_sources ? &*world.social_period_sources : nullptr;
    access.movement.emplace(MovementResultProjectionAccess{
        world.common, *world.movement_exact,
        world.spatial_carriers ? &*world.spatial_carriers : nullptr,
        world.group_movement_executions ? &*world.group_movement_executions
                                        : nullptr,
        world.transport_registrations ? &*world.transport_registrations
                                      : nullptr});
    auto growth = make_native_growth_result_sources(world, binding.runtime);
    if (!growth.ok)
      return {false, {}, std::move(growth.error)};
    access.growth = std::move(growth.value);
    if (world.economic_supply_policies_enabled()) {
      auto supply = EconomicSupplyWorldAccess::create(world);
      if (!supply.ok)
        return {false, {}, std::move(supply.error)};
      sources->supply = std::move(supply.value);
    }
    access.economic_publications.emplace(EconomicResultProjectionAccess{
        world.common, world.local_economy,
        world.catalog->fiscal_settlement.get(),
        world.fiscal_settlement_enabled() ? &world.fiscal_settlement : nullptr,
        world.occupation_fiscal_enabled() ? &world.occupation_fiscal_settlement
                                          : nullptr,
        sources->supply ? &sources->supply->context() : nullptr,
        world.economic_transport_expenses ? &*world.economic_transport_expenses
                                          : nullptr});
    auto shop_selected = selected_native_private_shop_operation_versions(
        world.catalog->identity);
    if (!shop_selected.ok)
      return {false, {}, std::move(shop_selected.error)};
    if (shop_selected.value.exact_operations()) {
      const auto *original = binding.private_shop_operations;
      if (!original || !world.private_shop_operation_sources ||
          !world.private_shop_operation_capabilities || !world.action_elapsed ||
          &original->shops.common != &world.common ||
          &original->shops.state != &world.private_shops ||
          &original->shops.catalog != world.catalog->private_shops.get() ||
          &original->sources != &*world.private_shop_operation_sources ||
          &original->capabilities !=
              &*world.private_shop_operation_capabilities ||
          &original->markets != world.catalog->household_purchases.get() ||
          &original->elapsed != &*world.action_elapsed)
        return {false,
                {},
                common_failure("ExactResultShopCandidateMismatch", {},
                               ErrorKind::internal)
                    .error};
      access.private_shop_operations = original;
    } else if (binding.private_shop_operations)
      return {false,
              {},
              common_failure("UnexpectedExactResultShopOwner", {},
                             ErrorKind::internal)
                  .error};
    if (shop_selected.value.shops == 3) {
      const auto *core = binding.original_core;
      if (!core || &core->characters.early.world != &world ||
          &core->characters.early.complete != original_owner)
        return missing<std::unique_ptr<NativeExactResultOwnerRead>>(
            "ExactShopResultOriginalCoreRequired", {});
      auto exact = NativePrivateShopExactSources::create(*core);
      if (!exact.ok)
        return {false, {}, std::move(exact.error)};
      sources->shop_exact = std::move(exact.value);
      // 若原 graph 已持真实宏观 phase，就借同一个；绝不从整数 tick 拼 phase。
      const auto *phase = access.private_shop_operations
                              ? access.private_shop_operations->shops.phase
                              : nullptr;
      auto law = PoliticalShopLicensingWorldAccess::create(
          world, phase, sources->shop_exact.get());
      if (!law.ok)
        return {false, {}, std::move(law.error)};
      sources->shop_law = std::move(law.value);
      access.private_shop_opening3 = &sources->shop_exact->opening();
      access.shop_licensing2 = &sources->shop_law->context();
    }
    if (const auto *occupancy = world.services.actor_social_occupancy_world()) {
      if (!occupancy->bound_to(world.common, world.interaction))
        return {false,
                {},
                common_failure("ExactResultOccupancyCandidateMismatch", {},
                               ErrorKind::internal)
                    .error};
      access.social_occupancy = &occupancy->read();
    }
    auto selected =
        selected_native_interaction_versions(world.catalog->identity);
    if (!selected.ok)
      return {false, {}, std::move(selected.error)};
    if (selected.value.exact_sources()) {
      auto interaction = read_native_interaction_world(world, binding);
      if (!interaction.ok)
        return {false, {}, std::move(interaction.error)};
      auto bound = interaction.value->bind_result_sources(access);
      if (!bound.ok)
        return {false, {}, std::move(bound.error)};
    } else if (binding.interaction_sources)
      return {false,
              {},
              common_failure("UnexpectedExactResultInteractionOwner", {},
                             ErrorKind::internal)
                  .error};
    if (world.social.original_relation_version == 2) {
      auto social = read_native_original_social_world(world, binding);
      if (!social.ok)
        return {false, {}, std::move(social.error)};
      auto bound = social.value->bind_result_sources(access);
      if (!bound.ok)
        return {false, {}, std::move(bound.error)};
    } else if (binding.original_social)
      return {false,
              {},
              common_failure("UnexpectedExactResultSocialOwner", {},
                             ErrorKind::internal)
                  .error};
    // 只接原作者组织的静态起源读取；P5 动态写路径不由此注册或启用。
    auto organization_layout =
        validate_native_organization_runtime_layout(world);
    if (!organization_layout.ok)
      return {false, {}, std::move(organization_layout.error)};
    auto organization_selected =
        selected_native_organization_runtime(world.catalog->identity);
    if (!organization_selected.ok)
      return {false, {}, std::move(organization_selected.error)};
    if (organization_selected.value) {
      auto now = world.actual_phase_moment();
      if (!now.ok)
        return {false, {}, std::move(now.error)};
      auto original = original_owner->read_political(now.value);
      if (!original.ok)
        return {false, {}, std::move(original.error)};
      if (!original.value)
        return missing<std::unique_ptr<NativeExactResultOwnerRead>>(
            "OrganizationAuthoredResultOriginalOwnerRequired", {});
      access.organization = checkpoint_share(
          std::unique_ptr<NativeOrganizationAuthoredResultRead>(
              new NativeOrganizationAuthoredResultRead(
                  world, now.value, std::move(original.value))));
    }
    // 只接已選production/3的原開期來源；其他經濟kind仍走原有限分派或明拒。
    auto production_selected =
        selected_native_production_cycles_version(world.catalog->identity);
    if (!production_selected.ok)
      return {false, {}, std::move(production_selected.error)};
    if (production_selected.value) {
      auto now = world.actual_phase_moment();
      if (!now.ok)
        return {false, {}, std::move(now.error)};
      access.economy =
          checkpoint_share(std::unique_ptr<NativeEconomicCalendarResultRead>(
              new NativeEconomicCalendarResultRead(
                  world, now.value, binding.action_sources, *original_owner)));
    }
    // Source binding deliberately does not perform the full published-state
    // walk. The single capture below the real Common seal performs it. Other
    // professional source routes remain unsupported until their original
    // complete owners are installed; no empty reader or tick fallback is added.
    return Result{true,
                  std::unique_ptr<NativeExactResultOwnerRead>(
                      new NativeExactResultOwnerRead(std::move(sources),
                                                     std::move(access))),
                  {}};
  } catch (const std::bad_alloc &) {
    return {false,
            {},
            common_failure("ExactResultOwningResourceExhausted", {},
                           ErrorKind::internal)
                .error};
  } catch (const std::length_error &) {
    return {false,
            {},
            common_failure("ExactResultOwningResourceExhausted", {},
                           ErrorKind::internal)
                .error};
  }
}
Outcome<std::unique_ptr<NativeExactResultOwnerRead>>
make_native_exact_result_sources(const WorldBundle &world) {
  return NativeExactResultOwnerRead::compose(world, nullptr);
}
Outcome<std::unique_ptr<NativeExactResultOwnerRead>>
make_native_exact_result_sources_bound(
    const WorldBundle &world, const NativeCompleteWorldCoreSources &core) {
  return NativeExactResultOwnerRead::compose(world, &core);
}
const ExactResultOwnerRead *
NativeExactResultOwnerRead::domain_owner(const StableId &domain) const {
  if (is(domain,
         {"growth", "growth_market_history", "personal_injury_recovery"}))
    return access_.growth.get();
  if (is(domain, {"military", "military_naval", "military_tactics",
                  "population_military", "naval"}))
    return access_.military.get();
  if (is(domain, {"local_economy", "economic_supply_policies", "cultivation",
                  "economy"}))
    return access_.economy.get();
  if (domain == "organization")
    return access_.organization.get();
  if (is(domain, {"politics", "diplomacy", "political_fiscal"}))
    return access_.politics.get();
  if (is(domain, {"family", "family_birth", "family_care_relief",
                  "family_collateral_legacy", "family_life"}))
    return access_.family.get();
  if (domain == "personal")
    return access_.personal.get();
  if (domain == "estate")
    return access_.estate.get();
  if (is(domain, {"commerce", "household_retail_settlements", "private_shops",
                  "services"}))
    return access_.commerce.get();
  if (domain == "medicine")
    return access_.medicine.get();
  if (domain == "scholarship")
    return access_.scholarship.get();
  if (domain == "arcana")
    return access_.arcana.get();
  if (is(domain, {"tasks",
                  "interaction",
                  "personal_appointments",
                  "visit_appointments",
                  "visit_appointment_meetings",
                  "visit_messages",
                  "visit_message_replies",
                  "visit_reception_claims",
                  "contact_inquiries",
                  "known_personal_dialogues",
                  "born_personal_dialogues",
                  "intelligence_probe",
                  "task_shop_orders",
                  "task_extensions",
                  "task_personal_message_acceptances",
                  "task_personal_message_reports",
                  "task_personal_message_deliveries",
                  "task_personal_cancellations",
                  "task_message_returns",
                  "task_shop_order_contacts"}))
    return access_.s3_execution.get();
  return nullptr;
}
Outcome<ExactOwnerFactRead>
NativeExactResultOwnerRead::read_selected_fact(const FactRecord &h) const {
  world_stage_observation::observe_node({world_stage_observation::NodeMetric::api_attempt, "P10.central.fact", &access_.common.state(), nullptr, {"fact", h.domain, h.kind, h.id, 0}, {}, -1});
  const auto *actual = access_.common.state().facts.find(h.id);
  if (!actual || actual->kind != h.kind || actual->domain != h.domain)
    return {false,
            {},
            common_failure("ExactFactHeaderMismatch", h.id, ErrorKind::internal)
                .error};
  // 只接同一完整 travel/4、/5 原表的永久计划头；不是任意 travel 白名单。
  // 此读口只证明内部头归属，不增加当前行动、basis 或完成安装的前置条件。
  if (h.domain == "travel" && h.kind == "terrain_journey") {
    const auto *w = sources_ ? sources_->original_world : nullptr;
    if (!w || &w->common != &access_.common || !w->catalog ||
        !w->catalog->travel || !w->catalog->exact_time ||
        !exact_complete_selected(*w->catalog->exact_time) ||
        !w->movement_exact || !access_.movement ||
        &access_.movement->common != &w->common ||
        &access_.movement->movement != &*w->movement_exact ||
        w->common.state().facts.find(h.id) != actual)
      return missing<ExactOwnerFactRead>("TerrainJourneyOriginalWorldRequired",
                                         h.id);
    auto selected = selected_native_transport_versions(w->catalog->identity);
    if (!selected.ok)
      return {false, {}, std::move(selected.error)};
    if (selected.value.travel != 4 && selected.value.travel != 5)
      return missing<ExactOwnerFactRead>(
          "TerrainJourneyOriginalVersionRequired", h.id);
    // 拒绝无正文、错索引或错插入序；Common 与 Travel 仍各自拥有原表。
    const auto *plan = w->travel.journey_facts.find(h.id);
    const auto &rows = w->travel.journey_facts.rows();
    const auto &order = w->travel.journey_facts.order();
    const auto row = std::find_if(rows.begin(), rows.end(), [&](const auto &v) {
      return &v.second == plan;
    });
    const auto position = std::find(order.begin(), order.end(), h.id);
    if (!plan || plan->id != h.id || row == rows.end() || row->first != h.id ||
        position == order.end() ||
        row - rows.begin() != position - order.begin() ||
        std::count(order.begin(), order.end(), h.id) != 1)
      return {
          false, {}, common_failure("TerrainJourneyFactMismatch", h.id).error};
    const auto &heads = w->common.state().facts.rows();
    const auto head =
        std::find_if(heads.begin(), heads.end(),
                     [&](const auto &v) { return &v.second == actual; });
    if (actual->id != h.id || head == heads.end() || head->first != h.id)
      return {
          false, {}, common_failure("TerrainJourneyFactMismatch", h.id).error};
    auto head_position = read_actual_common_fact_position(w->common, *actual);
    if (!head_position.ok)
      return {false, {}, std::move(head_position.error)};
    if (!plan->field_order.empty()) {
      if (plan->field_order.size() != 11)
        return {
            false, {}, common_failure("InvalidTravelFieldOrder", h.id).error};
      for (const auto *key :
           {"kind", "actor_id", "action_id", "tick", "map_id", "origin",
            "destination", "mode", "budget", "starting_load", "points"})
        if (std::count(plan->field_order.begin(), plan->field_order.end(),
                       key) != 1)
          return {
              false, {}, common_failure("InvalidTravelFieldOrder", h.id).error};
    }
    ExactOwnerFactRead out;
    out.disposition = ExactOwnerFactRead::Disposition::no_dynamic_projection;
    // 原计划只有 tick；不伪造动态事实、观察者、行动终局或精确支援时间源。
    out.reportable = false;
    return {true, std::move(out), {}};
  }
  // 只投影原个人行动真实释放的两种终局；到达不表示别域交付成功。
  // 同候选原 reader 先验证 opening、close、Common 与释放，再生成成对读面。
  if (h.domain == "travel" && h.kind == "personal_movement_terminal") {
    auto source_memory = CheckpointCopyLease::reserve();
    const MomentOwnerSource source{{checkpoint_fresh_string_copy(h.domain),
                                    checkpoint_fresh_string_copy(h.kind),
                                    checkpoint_fresh_string_copy(h.id)},
                                   1};
    auto at = read_selected_source_moment(source);
    if (!at.ok)
      return {false, {}, std::move(at.error)};
    const auto &movement = *sources_->original_world->movement_exact;
    const auto *terminal = movement.personal_terminals.find(h.id);
    const auto *prefix =
        terminal ? movement.prefixes.find(terminal->prefix_id) : nullptr;
    if (!terminal || !prefix)
      return {false,
              {},
              common_failure("MovementActionTerminalMismatch", h.id).error};
    const auto &rule = std::visit(
        [](const auto &basis) -> const StableId & {
          return basis.catalog_rule_ref;
        },
        prefix->basis);
    if (rule.empty())
      return {false, {}, common_failure("MovementFactRuleMissing", h.id).error};

    ExactOwnerFactRead out;
    out.payload_memory = CheckpointCopyLease::reserve();
    out.fact.emplace();
    auto &fact = *out.fact;
    fact.id = checkpoint_fresh_string_copy(terminal->id);
    fact.kind = checkpoint_fresh_string_copy(h.kind);
    fact.rule_version = checkpoint_fresh_string_copy(rule);
    fact.occurred_at = at.value;
    fact.time_basis =
        OwnerFactMoment{{checkpoint_fresh_string_copy(h.domain),
                         checkpoint_fresh_string_copy(h.kind),
                         checkpoint_fresh_string_copy(terminal->id)},
                        1};
    fact.subject_id = checkpoint_fresh_string_copy(terminal->actual_action.id);
    fact.outcome = terminal->kind == PersonalMovementTerminalKind::arrived
                       ? "arrived"
                       : "cancelled";
    fact.cause_fact_ids.push_back(
        checkpoint_fresh_string_copy(terminal->actual_close_source.id));
    fact.participants.reserve(terminal->actual_action.participants.size());
    for (const auto &actor : terminal->actual_action.participants)
      fact.participants.push_back(
          {{SubjectKind::actor, checkpoint_fresh_string_copy(actor)},
           "participant"});

    out.action.emplace();
    auto &action = *out.action;
    action.action_id = checkpoint_fresh_string_copy(terminal->actual_action.id);
    action.action_kind =
        checkpoint_fresh_string_copy(terminal->actual_action.kind);
    action.source_fact_id = checkpoint_fresh_string_copy(terminal->id);
    action.completed_at = at.value;
    action.time_basis =
        OwnerFactMoment{{checkpoint_fresh_string_copy(h.domain),
                         checkpoint_fresh_string_copy(h.kind),
                         checkpoint_fresh_string_copy(terminal->id)},
                        1};
    // 用户核准的有限映射；原来源拒绝不会进入此业务结果。
    action.success = terminal->kind == PersonalMovementTerminalKind::arrived;
    if (!action.success)
      action.code = "cancelled";
    action.participant_actor_ids.reserve(
        terminal->actual_action.participants.size());
    for (const auto &actor : terminal->actual_action.participants)
      action.participant_actor_ids.push_back(
          checkpoint_fresh_string_copy(actor));
    // 沿原 movement 读面保持非公开、不可报告且无额外观察者。
    return {true, std::move(out), {}};
  }
  // 原 Selection/1 只有 created_tick，绝不把它升级为精确时刻来源。
  // 此有限支只证明同 World 真行/真头身份；业务权限仍由原接受链验证。
  if (h.domain == "local_economy" &&
      h.kind == population_selection_source_kind) {
    const auto *w = sources_ ? sources_->original_world : nullptr;
    if (!w || &w->common != &access_.common || !w->catalog ||
        !w->population_transport || !access_.economic_publications ||
        &access_.economic_publications->common != &w->common ||
        &access_.economic_publications->economy != &w->local_economy ||
        w->common.state().facts.find(h.id) != actual)
      return missing<ExactOwnerFactRead>(
          "PopulationSelectionOriginalWorldRequired", h.id);
    auto selected =
        selected_native_population_transport_version(w->catalog->identity);
    if (!selected.ok)
      return {false, {}, std::move(selected.error)};
    if ((selected.value != 2 && selected.value != 3) ||
        w->population_transport->version != selected.value)
      return missing<ExactOwnerFactRead>(
          "PopulationSelectionOriginalVersionRequired", h.id);


    auto source_memory = CheckpointMemoryCharge(0);
    SourceRef source{checkpoint_fresh_string_copy(actual->domain),
                     checkpoint_fresh_string_copy(actual->kind),
                     checkpoint_fresh_string_copy(actual->id)};
    auto position = read_actual_common_fact_position(w->common, source);
    if (!position.ok)
      return {false, {}, std::move(position.error)};
    auto original = read_population_selection_source(
        w->local_economy, *w->population_transport, source,
        population_selection_source_schema);
    if (!original.ok)
      return {false, {}, std::move(original.error)};
    auto now = w->actual_phase_moment();
    if (!now.ok)
      return {false, {}, std::move(now.error)};
    if (original.value != w->population_transport->selections.find(h.id) ||
        original.value->created_tick > now.value.half_day_tick)
      return {false,
              {},
              common_failure("PopulationSelectionOriginalSourceMismatch", h.id)
                  .error};
    ExactOwnerFactRead out;
    out.disposition = ExactOwnerFactRead::Disposition::no_dynamic_projection;
    out.reportable = false;
    return {true, std::move(out), {}};
  }
  if (h.domain == "interaction" &&
      h.kind == public_supply_inspection_source_kind) {
    if (!access_.public_supply_inspection)
      return missing<ExactOwnerFactRead>(
          "PublicSupplyInspectionOriginalOwnerRequired", h.id);
    // 先为本栈原SourceRef逐串预留，再交实际投影owner形成自己的返回租约。


    auto source_memory = CheckpointMemoryCharge(0);
    SourceRef source{checkpoint_fresh_string_copy(h.domain),
                     checkpoint_fresh_string_copy(h.kind),
                     checkpoint_fresh_string_copy(h.id)};
    return project_public_supply_inspection_source(
        *access_.public_supply_inspection, source, 1);
  }
  // 新开设3/许可2与原四种营业头只作已选原来源支撑；不捏公开事件/旁观者。
  Integer supporting_schema = 0;
  if (h.domain == private_shop_module &&
      is(h.kind,
         {"private_shop_operating_started", "private_shop_labor_applied",
          "private_shop_operating_completed",
          "private_shop_operating_interrupted"}))
    supporting_schema = h.kind == "private_shop_labor_applied" ? 1 : 2;
  else if (h.domain == private_shop_module &&
           h.kind == private_shop_open_kind && access_.private_shop_opening3)
    supporting_schema = 3;
  else if (h.domain == political_shop_licensing_module &&
           shop_licensing_kind(h.kind) && access_.shop_licensing2)
    supporting_schema = 2;
  else if (h.domain == "politics" &&
           is(h.kind, {"political_relations_initialized",
                       "political_battle_relation_changed",
                       "political_alliance_breached"}))
    supporting_schema = 1;
  if (supporting_schema) {
    const Integer schema = supporting_schema;
    // 深值与返回租约配对；原 reader 仍逐次核同根、真实行、时刻和因果。


    ExactOwnerFactRead out;
    out.payload_memory = CheckpointCopyLease::reserve();
    out.supporting_sources.reserve(1);
    out.supporting_sources.push_back({{checkpoint_fresh_string_copy(h.domain),
                                       checkpoint_fresh_string_copy(h.kind),
                                       checkpoint_fresh_string_copy(h.id)},
                                      schema});
    auto original = read_selected_source_moment(out.supporting_sources.front());
    if (!original.ok)
      return {false, {}, std::move(original.error)};
    out.disposition = ExactOwnerFactRead::Disposition::no_dynamic_projection;
    out.reportable = false;
    return {true, std::move(out), {}};
  }
  if (condition_application_kind(h)) {
    auto source = condition_application(access_, *actual);
    if (!source.ok)
      return {false, {}, std::move(source.error)};
    ExactOwnerFactRead out;
    out.disposition = ExactOwnerFactRead::Disposition::no_dynamic_projection;
    out.reportable = false;
    out.supporting_sources.push_back({{h.domain, h.kind, h.id}, 2});
    return {true, std::move(out), {}};
  }
  if (original_social_supporting_fact_kind(h)) {
    if (!access_.original_social)
      return missing<ExactOwnerFactRead>("OriginalSocialResultOwnerNotSelected",
                                         h.id);
    auto bound = access_.original_social->validate_result_binding(access_);
    if (!bound.ok)
      return {false, {}, std::move(bound.error)};
    return access_.original_social->read_supporting_fact(h);
  }
  if (native_visit_original_result_kind(h)) {
    if (!access_.visit_originals)
      return missing<ExactOwnerFactRead>("VisitOriginalResultOwnerNotSelected",
                                         h.id);
    return access_.visit_originals->read_selected_fact(h);
  }
  if (visit_meeting_kind({h.domain, h.kind, h.id})) {
    auto time = read_selected_source_moment({{h.domain, h.kind, h.id}, 2});
    if (!time.ok)
      return {false, {}, time.error};
    ExactOwnerFactRead out;
    out.disposition = ExactOwnerFactRead::Disposition::no_dynamic_projection;
    out.supporting_sources.push_back({{h.domain, h.kind, h.id}, 2});
    return {true, std::move(out), {}};
  }
  if (visit_invitation_kind({h.domain, h.kind, h.id})) {
    const auto *x = access_.visit_appointments;
    if (!x)
      return missing<ExactOwnerFactRead>(
          "VisitAppointmentOriginalOwnerRequired", h.id);
    if (&x->common != &access_.common || !access_.interaction ||
        &x->interaction != access_.interaction ||
        !access_.interaction->visit_appointment_invitations ||
        &x->invitations != &*access_.interaction->visit_appointment_invitations)
      return {false,
              {},
              common_failure("VisitAppointmentResultCandidateMismatch", h.id,
                             ErrorKind::internal)
                  .error};
    auto valid = validate_visit_appointment_original_sources(
        x->common, *x->definitions.common, x->invitation_life.messages,
        x->invitation_life.message_catalog, x->catalog, x->personal,
        x->definitions, x->invitation_life.catalog, x->invitations,
        x->invitation_history, x->invitation_life.ordinary);
    if (!valid.ok)
      return {false, {}, valid.error};
    ExactOwnerFactRead out;
    out.disposition = ExactOwnerFactRead::Disposition::no_dynamic_projection;
    return {true, std::move(out), {}};
  }
  if (inquiry_kind({h.domain, h.kind, h.id})) {
    auto time = read_selected_source_moment({{h.domain, h.kind, h.id}, 2});
    if (!time.ok)
      return {false, {}, time.error};
    ExactOwnerFactRead out;
    out.disposition = ExactOwnerFactRead::Disposition::no_dynamic_projection;
    out.supporting_sources.push_back({{h.domain, h.kind, h.id}, 2});
    return {true, std::move(out), {}};
  }
  if (appointment_kind({h.domain, h.kind, h.id})) {
    if (!access_.personal_appointments)
      return missing<ExactOwnerFactRead>(
          "PersonalAppointmentTime2OwnerRequired", h.id);
    auto time = read_selected_source_moment({{h.domain, h.kind, h.id}, 2});
    if (!time.ok)
      return {false, {}, time.error};
    ExactOwnerFactRead out;
    out.disposition = ExactOwnerFactRead::Disposition::no_dynamic_projection;
    out.supporting_sources.push_back({{h.domain, h.kind, h.id}, 2});
    return {true, std::move(out), {}};
  }
  if (native_treaty_revision_result_kind(h)) {
    if (!access_.treaty_revisions)
      return missing<ExactOwnerFactRead>("TreatyRevisionResultOwnerNotSelected",
                                         h.id);
    return access_.treaty_revisions->read_selected_fact(h);
  }
  if (native_safe_exit_binding_source_kind(h)) {
    if (!access_.safe_exit_bindings)
      return missing<ExactOwnerFactRead>(
          "SafeExitBindingResultOwnerNotSelected", h.id);
    return access_.safe_exit_bindings->read_selected_fact(h);
  }
  if (native_treaty_procedure_source_kind(h)) {
    if (!access_.treaty_procedures)
      return missing<ExactOwnerFactRead>(
          "TreatyProcedureResultOwnerNotSelected", h.id);
    return access_.treaty_procedures->read_selected_fact(h);
  }
  // Field 的 battle/casualties 与旧 dynamic2 共用 Common kind，但原
  // producer/物理根不同；只在明确 Field factory 存在时走这条有限路由。
  if (sources_ && sources_->field_military_results &&
      native_field_military_result_kind(h.domain, h.kind))
    return sources_->field_military_results->read_selected_fact(h);
  if (sources_ && sources_->field_movement_results &&
      native_field_movement_result_kind(h.domain, h.kind))
    return sources_->field_movement_results->read_selected_fact(h);
  if (native_military_supporting_source2({h.domain, h.kind, h.id})) {
    if (!access_.military_supporting)
      return missing<ExactOwnerFactRead>(
          "MilitarySupportingResultOwnerNotSelected", h.id);
    return access_.military_supporting->read_selected_fact(h);
  }
  if (h.domain == "interaction" && h.kind == "actor_social_occupancy_changed") {
    if (!access_.social_occupancy)
      return missing<ExactOwnerFactRead>("SocialOccupancyOwnerNotSelected",
                                         h.id);
    auto time = access_.social_occupancy->read_source_moment(
        {h.domain, h.kind, h.id}, 1);
    if (!time.ok)
      return {false, {}, time.error};
    ExactOwnerFactRead out;
    out.disposition = ExactOwnerFactRead::Disposition::no_dynamic_projection;
    out.supporting_sources.push_back({{h.domain, h.kind, h.id}, 1});
    return {true, std::move(out), {}};
  }
  if (h.domain == "interaction" &&
      is(h.kind, {"encounter_created", "encounter_resolved"})) {
    if (!access_.interaction)
      return missing<ExactOwnerFactRead>("EncounterResultOwnerNotSelected",
                                         h.id);
    auto time = read_exact_encounter_source_moment(
        access_.common, *access_.interaction, {h.domain, h.kind, h.id}, 1);
    if (!time.ok)
      return {false, {}, time.error};
    ExactOwnerFactRead out;
    out.disposition = ExactOwnerFactRead::Disposition::no_dynamic_projection;
    out.supporting_sources.push_back({{h.domain, h.kind, h.id}, 1});
    return {true, std::move(out), {}};
  }
  if (h.domain == "interaction" && h.kind == face_to_face_contact_source_kind) {
    if (!access_.interaction || !access_.interaction_catalog)
      return missing<ExactOwnerFactRead>("FaceToFaceContactOwnerNotSelected",
                                         h.id);
    auto contact = read_face_to_face_contact_source(
        access_.common, *access_.interaction, *access_.interaction_catalog,
        {h.domain, h.kind, h.id}, 1);
    if (!contact.ok)
      return {false, {}, contact.error};
    ExactOwnerFactRead out;
    out.disposition = ExactOwnerFactRead::Disposition::no_dynamic_projection;
    out.supporting_sources.push_back({{h.domain, h.kind, h.id}, 1});
    return {true, std::move(out), {}};
  }
  if (h.domain == "interaction" &&
      h.kind == household_purchase_report_source_kind) {
    if (!access_.interaction)
      return missing<ExactOwnerFactRead>("HouseholdReportOwnerNotSelected",
                                         h.id);
    return project_household_purchase_report_source(
        access_.common, *access_.interaction, {h.domain, h.kind, h.id}, 1);
  }
  if (h.domain == "interaction" && h.kind == face_to_face_report_source_kind) {
    if (!access_.interaction || !access_.interaction_catalog)
      return missing<ExactOwnerFactRead>("FaceToFaceReportOwnerNotSelected",
                                         h.id);
    auto actual = read_original_face_to_face_report_source(
        access_.common, *access_.interaction, *access_.interaction_catalog,
        {h.domain, h.kind, h.id}, 1);
    if (!actual.ok)
      return {false, {}, actual.error};
    ExactOwnerFactRead out;
    out.disposition = ExactOwnerFactRead::Disposition::no_dynamic_projection;
    out.supporting_sources.push_back({{h.domain, h.kind, h.id}, 1});
    return {true, std::move(out), {}};
  }
  if (h.domain == "visit_messages" && custody_kind(h.kind)) {
    auto time = read_selected_source_moment({{h.domain, h.kind, h.id}, 3});
    if (!time.ok)
      return {false, {}, time.error};
    ExactOwnerFactRead out;
    out.disposition = ExactOwnerFactRead::Disposition::no_dynamic_projection;
    out.supporting_sources.push_back({{h.domain, h.kind, h.id}, 3});
    return {true, std::move(out), {}};
  }
  if (native_political_fiscal_time2_result_kind(h)) {
    if (!access_.political_fiscal_time2)
      return missing<ExactOwnerFactRead>(
          "PoliticalFiscalTime2ResultOwnerNotSelected", h.id);
    return access_.political_fiscal_time2->read_selected_fact(h);
  }
  if (native_politics_time2_result_kind(h)) {
    if (!access_.politics_time2)
      return missing<ExactOwnerFactRead>("PoliticsTime2ResultOwnerNotSelected",
                                         h.id);
    return access_.politics_time2->read_selected_fact(h);
  }
  if (native_work_result_kind(h)) {
    if (!access_.work)
      return missing<ExactOwnerFactRead>("WorkResultOwnerNotSelected", h.id);
    return access_.work->read_selected_fact(h);
  }
  if (h.domain == "tasks" && task_kind(h.kind))
    return read_task_dynamic_fact_v2(access_.common, access_.tasks,
                                     access_.affairs, h);
  if (native_economic_publication_kind(h)) {
    if (!access_.economic_publications)
      return missing<ExactOwnerFactRead>("EconomicResultOwnerNotSelected",
                                         h.id);
    return read_economic_publication_v2(*access_.economic_publications, h);
  }
  if (native_movement_result_kind(h)) {
    if (!access_.movement)
      return missing<ExactOwnerFactRead>("MovementResultOwnerNotSelected",
                                         h.id);
    return read_movement_fact_v2(*access_.movement, h);
  }
  if ((h.domain == "cultivation" &&
       (h.kind == "training" || h.kind == "skill_study")) ||
      (h.domain == "services" && h.kind == "long_rest")) {
    if (!access_.growth)
      return missing<ExactOwnerFactRead>("GrowthResultOwnerNotSelected", h.id);
    return access_.growth->read_selected_fact(h);
  }
  if (native_organization_authority_result_kind(h)) {
    if (!access_.organization_authority)
      return missing<ExactOwnerFactRead>(
          "OrganizationAuthorityResultOwnerNotSelected", h.id);
    return access_.organization_authority->read_selected_fact(h);
  }
  if (const auto *owner = domain_owner(h.domain))
    return owner->read_selected_fact(h);
  return missing<ExactOwnerFactRead>("ExactResultOwnerNotSelected", h.id);
}
Outcome<WorldMoment> NativeExactResultOwnerRead::read_selected_source_moment(
    const MomentOwnerSource &source) const {
  world_stage_observation::observe_node({world_stage_observation::NodeMetric::api_attempt, "P10.central.moment", &access_.common.state(), nullptr, {"fact", source.source_ref.domain, source.source_ref.kind, source.source_ref.id, source.source_schema}, {}, -1});
  // 终局精确时刻只能由同候选已安装的原 ActionTime reader 反验。
  // 不回完整工厂、不重写旅行校验，也不把 Common tick 升格成精确来源。
  if (source.source_ref.domain == "travel" &&
      source.source_ref.kind == "personal_movement_terminal") {
    const auto &ref = source.source_ref;
    const auto *w = sources_ ? sources_->original_world : nullptr;
    const auto *owner = sources_ ? sources_->original_owner : nullptr;
    if (source.source_schema != 1 || !w || !owner ||
        &w->common != &access_.common || !w->catalog || !w->catalog->travel ||
        !w->catalog->exact_time ||
        !exact_complete_selected(*w->catalog->exact_time) ||
        !w->catalog->complete_exact_time_runtime || !w->movement_exact ||
        !access_.movement || &access_.movement->common != &w->common ||
        &access_.movement->movement != &*w->movement_exact)
      return missing<WorldMoment>("MovementTerminalOriginalWorldRequired",
                                  ref.id);
    const auto &binding = owner->reads();
    const auto *core = binding.original_core;
    if (!core || &core->characters.early.world != w ||
        &core->characters.early.complete != owner ||
        &binding.action_sources != &core->action_time ||
        &binding.runtime != w->catalog->complete_exact_time_runtime.get() ||
        &binding.movement != &*w->movement_exact)
      return {false,
              {},
              common_failure("MovementTerminalCandidateMismatch", ref.id,
                             ErrorKind::internal)
                  .error};
    auto selected = selected_native_transport_versions(w->catalog->identity);
    if (!selected.ok)
      return {false, {}, std::move(selected.error)};
    if (selected.value.travel != 4 && selected.value.travel != 5)
      return missing<WorldMoment>("MovementTerminalOriginalVersionRequired",
                                  ref.id);
    const auto &terminals = w->movement_exact->personal_terminals;
    const auto *terminal = terminals.find(ref.id);
    const auto &rows = terminals.rows();
    const auto &order = terminals.order();
    const auto row = std::find_if(rows.begin(), rows.end(), [&](const auto &v) {
      return &v.second == terminal;
    });
    const auto position = std::find(order.begin(), order.end(), ref.id);
    // 反验真实表键、索引与唯一插入序；不让副本行或错序行冒作原终局。
    if (!terminal || terminal->id != ref.id || row == rows.end() ||
        row->first != ref.id || position == order.end() ||
        row - rows.begin() != position - order.begin() ||
        std::count(order.begin(), order.end(), ref.id) != 1)
      return {false,
              {},
              common_failure("MovementActionTerminalRequired", ref.id).error};
    auto head = read_actual_common_fact_position(w->common, ref);
    if (!head.ok)
      return {false, {}, std::move(head.error)};
    auto cause = read_actual_common_fact_position(
        w->common, terminal->actual_close_source);
    if (!cause.ok)
      return {false, {}, std::move(cause.error)};
    auto proof = binding.action_sources.read_action_time_owner(
        movement_action_time_registration(selected.value.travel),
        terminal->actual_action.id);
    if (!proof.ok)
      return {false, {}, std::move(proof.error)};
    const auto *ended =
        proof.value.ended_source
            ? std::get_if<MomentOwnerSource>(&*proof.value.ended_source)
            : nullptr;
    if (proof.value.phase != ActionTimeOwnerPhase::terminal ||
        proof.value.terminal_result_ref != std::optional<StableId>{ref.id} ||
        proof.value.action_ref != terminal->actual_action.id ||
        proof.value.action_kind != terminal->actual_action.kind ||
        proof.value.actor_id != terminal->actual_action.actor_id ||
        proof.value.participant_actor_ids !=
            terminal->actual_action.participants ||
        !ended || ended->source_schema != source.source_schema ||
        ended->source_ref.domain != ref.domain ||
        ended->source_ref.kind != ref.kind || ended->source_ref.id != ref.id)
      return {false,
              {},
              common_failure("MovementActionTerminalMismatch", ref.id).error};
    return binding.action_sources.read_action_time_source(source);
  }
  if (source.source_ref.domain == "local_economy" &&
      source.source_ref.kind == population_selection_source_kind)
    return missing<WorldMoment>("PopulationSelectionExactMomentUnsupported",
                                source.source_ref.id);
  // P3 三个原头只提供完整原源时点，不派生公众事实或观察者。
  if (source.source_ref.domain == "politics" &&
      is(source.source_ref.kind, {"political_relations_initialized",
                                  "political_battle_relation_changed",
                                  "political_alliance_breached"})) {
    const auto *world = sources_ ? sources_->original_world : nullptr;
    const auto *owner =
        sources_ ? sources_->political_relations.get() : nullptr;
    if (!world || &world->common != &access_.common || !owner ||
        !owner->bound_to(*world))
      return missing<WorldMoment>(
          "PoliticalRelationsOriginalResultOwnerRequired",
          source.source_ref.id);
    auto original =
        read_political_relations_original_source(owner->context(), source);
    if (!original.ok)
      return {false, {}, std::move(original.error)};
    if (!original.value.actual_source || !original.value.occurred_at ||
        original.value.source_schema != 1)
      return missing<WorldMoment>(
          "PoliticalRelationsOriginalResultSourceRequired",
          source.source_ref.id);
    return {true, *original.value.occurred_at, {}};
  }
  if (is_public_supply_inspection_source(source.source_ref)) {
    if (!access_.public_supply_inspection)
      return missing<WorldMoment>("PublicSupplyInspectionOriginalOwnerRequired",
                                  source.source_ref.id);
    auto original = read_public_supply_inspection_source(
        *access_.public_supply_inspection, source.source_ref,
        source.source_schema);
    if (!original.ok)
      return {false, {}, std::move(original.error)};
    return {true, original.value->inspected_at, {}};
  }
  if (source.source_ref.domain == private_shop_module &&
      source.source_ref.kind == private_shop_open_kind &&
      source.source_schema == 3) {
    const auto *opening = access_.private_shop_opening3;
    if (!opening || &opening->common != &access_.common)
      return missing<WorldMoment>("ShopOpeningOriginalResultOwnerRequired",
                                  source.source_ref.id);
    auto actual =
        read_private_shop_opening_actual3(*opening, source.source_ref);
    if (!actual.ok)
      return {false, {}, std::move(actual.error)};
    return {true, *actual.value, {}};
  }
  if (source.source_ref.domain == political_shop_licensing_module &&
      shop_licensing_kind(source.source_ref.kind)) {
    const auto *law = access_.shop_licensing2;
    if (source.source_schema != 2 || !law || &law->common != &access_.common)
      return missing<WorldMoment>("ShopLicensingOriginalResultOwnerRequired",
                                  source.source_ref.id);
    auto actual =
        read_political_shop_licensing_actual_source2(*law, source.source_ref);
    if (!actual.ok)
      return {false, {}, std::move(actual.error)};
    return {true, *actual.value.occurred_at, {}};
  }
  if (source.source_ref.domain == "actor_foundation" &&
      owns_condition_application_fact_kind(source.source_ref.kind)) {
    const auto &ref = source.source_ref;
    if (source.source_schema != 2)
      return missing<WorldMoment>("ConditionApplicationSourceSchemaRequired",
                                  ref.id);
    const auto *header = access_.common.state().facts.find(ref.id);
    if (!header || header->domain != ref.domain || header->kind != ref.kind)
      return {
          false,
          {},
          common_failure("ExactFactHeaderMismatch", ref.id, ErrorKind::internal)
              .error};
    auto actual = condition_application(access_, *header);
    return actual.ok
               ? Outcome<WorldMoment>{true,
                                      actual.value->effective_at.exact_moment,
                                      {}}
               : Outcome<WorldMoment>{false, {}, std::move(actual.error)};
  }
  if (source.source_ref.domain == private_shop_module &&
      is(source.source_ref.kind,
         {"private_shop_operating_started", "private_shop_labor_applied",
          "private_shop_operating_completed",
          "private_shop_operating_interrupted"})) {
    const auto *original = access_.private_shop_operations;
    if (!original)
      return missing<WorldMoment>("ShopOperationOriginalResultOwnerRequired",
                                  source.source_ref.id);
    if (&original->shops.common != &access_.common)
      return {false,
              {},
              common_failure("ExactResultShopCandidateMismatch",
                             source.source_ref.id, ErrorKind::internal)
                  .error};
    if (source.source_ref.kind == "private_shop_operating_started") {
      auto actual =
          read_original_private_shop_operation_acceptance(*original, source);
      return actual.ok
                 ? Outcome<WorldMoment>{true, actual.value->accepted_at, {}}
                 : Outcome<WorldMoment>{false, {}, std::move(actual.error)};
    }
    if (source.source_ref.kind == "private_shop_labor_applied") {
      auto actual = read_original_private_shop_labor(*original, source);
      return actual.ok
                 ? Outcome<WorldMoment>{true, actual.value->occurred_at, {}}
                 : Outcome<WorldMoment>{false, {}, std::move(actual.error)};
    }
    auto actual =
        read_original_private_shop_operation_terminal(*original, source);
    return actual.ok ? Outcome<WorldMoment>{true, actual.value->occurred_at, {}}
                     : Outcome<WorldMoment>{false, {}, std::move(actual.error)};
  }
  // Use the real stored header for the finite Social classifier, without
  // allocating a copied FactRecord or inventing a header for an owned source.
  if (source.source_ref.domain == "social") {
    const auto *header =
        access_.common.state().facts.find(source.source_ref.id);
    if (header && header->domain == source.source_ref.domain &&
        header->kind == source.source_ref.kind &&
        original_social_supporting_fact_kind(*header)) {
      if (!access_.original_social)
        return missing<WorldMoment>("OriginalSocialResultOwnerNotSelected",
                                    source.source_ref.id);
      auto bound = access_.original_social->validate_result_binding(access_);
      if (!bound.ok)
        return {false, {}, std::move(bound.error)};
      return access_.original_social->read_supporting_source_moment(source);
    }
  }
  if (native_visit_original_result_kind({source.source_ref.id,
                                         source.source_ref.kind,
                                         source.source_ref.domain})) {
    if (!access_.visit_originals)
      return missing<WorldMoment>("VisitOriginalResultOwnerNotSelected",
                                  source.source_ref.id);
    return access_.visit_originals->read_selected_source_moment(source);
  }
  if (visit_meeting_kind(source.source_ref)) {
    const auto *x = access_.visit_appointments;
    if (!x)
      return missing<WorldMoment>("VisitAppointmentMeetingTime2OwnerRequired",
                                  source.source_ref.id);
    if (&x->common != &access_.common || !access_.interaction ||
        &x->interaction != access_.interaction ||
        !access_.interaction->visit_appointment_meetings ||
        &x->state != &*access_.interaction->visit_appointment_meetings)
      return {false,
              {},
              common_failure("VisitAppointmentMeetingResultCandidateMismatch",
                             source.source_ref.id, ErrorKind::internal)
                  .error};
    return read_visit_appointment_meeting_source_moment(*x, source.source_ref,
                                                        source.source_schema);
  }
  if (visit_invitation_kind(source.source_ref)) {
    const auto *x = access_.visit_appointments;
    const auto &ref = source.source_ref;
    if (source.source_schema != 1 || !x || x->invitations.version != 2 ||
        ref.kind != visit_appointment_offered_kind ||
        !x->invitation_history.actual_commands)
      return missing<WorldMoment>(
          "VisitAppointmentOriginalCalendarHasNoMomentSource", ref.id);
    auto checked = read_selected_fact({ref.id, ref.kind, ref.domain});
    if (!checked.ok)
      return {false, {}, checked.error};
    const auto *row = x->invitations.offered.find(ref.id);
    if (!row || !row->authored_origin)
      return missing<WorldMoment>(
          "VisitAppointmentOriginalCalendarHasNoMomentSource", ref.id);
    auto valid = validate_visit_appointment_director_actor_effect(
        x->common, x->invitations, *x->invitation_history.actual_commands,
        row->contact.actor_id, ref);
    if (!valid.ok)
      return {false, {}, valid.error};
    auto original = read_visit_authored_command(
        *x->invitation_history.actual_commands, source, row->contact.actor_id,
        "offer_visit_appointment_authored", row->contact.tick);
    return original.ok
               ? Outcome<WorldMoment>{true,
                                      original.value.command->accepted_at,
                                      {}}
               : Outcome<WorldMoment>{false, {}, original.error};
  }
  if (inquiry_kind(source.source_ref)) {
    const auto *x = access_.contact_inquiries;
    if (!x)
      return missing<WorldMoment>("ContactInquiryTime2OwnerRequired",
                                  source.source_ref.id);
    if (&x->common != &access_.common || !access_.interaction ||
        &x->interaction != access_.interaction ||
        !access_.interaction->contact_inquiries ||
        &x->state != &*access_.interaction->contact_inquiries)
      return {false,
              {},
              common_failure("ContactInquiryResultCandidateMismatch",
                             source.source_ref.id, ErrorKind::internal)
                  .error};
    return read_contact_inquiry_source_moment(*x, source.source_ref,
                                              source.source_schema);
  }
  if (appointment_kind(source.source_ref)) {
    const auto *x = access_.personal_appointments;
    if (!x)
      return missing<WorldMoment>("PersonalAppointmentTime2OwnerRequired",
                                  source.source_ref.id);
    if (&x->common != &access_.common || !access_.interaction ||
        &x->interaction != access_.interaction ||
        !access_.interaction->personal_appointments ||
        &x->state != &*access_.interaction->personal_appointments)
      return {false,
              {},
              common_failure("PersonalAppointmentResultCandidateMismatch",
                             source.source_ref.id, ErrorKind::internal)
                  .error};
    return read_personal_appointment_source_moment(*x, source.source_ref,
                                                   source.source_schema);
  }
  if (native_treaty_revision_result_kind({source.source_ref.id,
                                          source.source_ref.kind,
                                          source.source_ref.domain})) {
    if (!access_.treaty_revisions)
      return missing<WorldMoment>("TreatyRevisionResultOwnerNotSelected",
                                  source.source_ref.id);
    return access_.treaty_revisions->read_selected_source_moment(source);
  }
  if (native_safe_exit_binding_source_kind({source.source_ref.id,
                                            source.source_ref.kind,
                                            source.source_ref.domain})) {
    if (!access_.safe_exit_bindings)
      return missing<WorldMoment>("SafeExitBindingResultOwnerNotSelected",
                                  source.source_ref.id);
    return access_.safe_exit_bindings->read_selected_source_moment(source);
  }
  if (native_treaty_procedure_source_kind({source.source_ref.id,
                                           source.source_ref.kind,
                                           source.source_ref.domain})) {
    if (!access_.treaty_procedures)
      return missing<WorldMoment>("TreatyProcedureResultOwnerNotSelected",
                                  source.source_ref.id);
    return access_.treaty_procedures->read_selected_source_moment(source);
  }
  // 同 fact 路由的有限身份判定；原 source_schema/真正 WorldMoment
  // 仍由所选原域 reader 逐次证明，绝不把 Field 局部秒或 period tick 冒充。
  if (sources_ && sources_->field_military_results &&
      native_field_military_result_kind(source.source_ref.domain,
                                       source.source_ref.kind))
    return sources_->field_military_results->read_selected_source_moment(source);
  if (sources_ && sources_->field_movement_results &&
      native_field_movement_result_kind(source.source_ref.domain,
                                       source.source_ref.kind))
    return sources_->field_movement_results->read_selected_source_moment(source);
  if (native_military_supporting_source2(source.source_ref)) {
    if (!access_.military_supporting)
      return missing<WorldMoment>("MilitarySupportingResultOwnerNotSelected",
                                  source.source_ref.id);
    return access_.military_supporting->read_selected_source_moment(source);
  }
  const auto &ref = source.source_ref;
  if (native_political_fiscal_time2_result_kind(
          {ref.id, ref.kind, ref.domain})) {
    if (!access_.political_fiscal_time2)
      return missing<WorldMoment>("PoliticalFiscalTime2ResultOwnerNotSelected",
                                  ref.id);
    return access_.political_fiscal_time2->read_selected_source_moment(source);
  }
  if (ref.domain == "visit_messages" && custody_kind(ref.kind)) {
    if (!access_.interaction || !access_.interaction_catalog ||
        !access_.interaction->visit_messages ||
        !access_.interaction_catalog->visit_messages)
      return missing<WorldMoment>("VisitReportCustodyOwnerNotSelected", ref.id);
    return read_original_visit_report_custody_moment(
        access_.common, *access_.interaction->visit_messages,
        *access_.interaction_catalog->visit_messages, *access_.interaction,
        *access_.interaction_catalog, ref, source.source_schema);
  }
  if (native_politics_time2_result_kind({ref.id, ref.kind, ref.domain})) {
    if (!access_.politics_time2)
      return missing<WorldMoment>("PoliticsTime2ResultOwnerNotSelected",
                                  ref.id);
    return access_.politics_time2->read_selected_source_moment(source);
  }
  if (native_work_result_kind({ref.id, ref.kind, ref.domain})) {
    if (!access_.work)
      return missing<WorldMoment>("WorkResultOwnerNotSelected", ref.id);
    return access_.work->read_selected_source_moment(source);
  }
  if (ref.domain == "tasks" && task_kind(ref.kind))
    return read_task_result_source_moment(access_.common, access_.tasks,
                                          access_.affairs, source);
  if (ref.domain == "interaction" &&
      is(ref.kind, {"actor_social_occupancy_initial",
                    "actor_social_occupancy_changed"})) {
    if (!access_.social_occupancy)
      return missing<WorldMoment>("SocialOccupancyOwnerNotSelected", ref.id);
    return access_.social_occupancy->read_source_moment(ref,
                                                        source.source_schema);
  }
  if (ref.domain == "interaction" &&
      is(ref.kind, {"encounter_created", "encounter_resolved"})) {
    if (!access_.interaction)
      return missing<WorldMoment>("EncounterResultOwnerNotSelected", ref.id);
    return read_exact_encounter_source_moment(
        access_.common, *access_.interaction, ref, source.source_schema);
  }
  if (ref.domain == "interaction" &&
      ref.kind == face_to_face_contact_source_kind) {
    if (!access_.interaction || !access_.interaction_catalog)
      return missing<WorldMoment>("FaceToFaceContactOwnerNotSelected", ref.id);
    auto contact = read_face_to_face_contact_source(
        access_.common, *access_.interaction, *access_.interaction_catalog, ref,
        source.source_schema);
    if (!contact.ok)
      return {false, {}, contact.error};
    return {true, contact.value->occurred_at, {}};
  }
  if (ref.domain == "interaction" &&
      ref.kind == household_purchase_report_source_kind) {
    if (!access_.interaction)
      return missing<WorldMoment>("HouseholdReportOwnerNotSelected", ref.id);
    auto actual = read_original_household_purchase_report_source(
        access_.common, *access_.interaction, ref, source.source_schema);
    return actual.ok
               ? Outcome<WorldMoment>{true, actual.value->disclosed_at, {}}
               : Outcome<WorldMoment>{false, {}, actual.error};
  }
  if (ref.domain == "interaction" &&
      ref.kind == face_to_face_report_source_kind) {
    if (!access_.interaction || !access_.interaction_catalog)
      return missing<WorldMoment>("FaceToFaceReportOwnerNotSelected", ref.id);
    auto actual = read_original_face_to_face_report_source(
        access_.common, *access_.interaction, *access_.interaction_catalog, ref,
        source.source_schema);
    return actual.ok
               ? Outcome<WorldMoment>{true, actual.value->disclosed_at, {}}
               : Outcome<WorldMoment>{false, {}, actual.error};
  }
  if (ref.domain == "interaction" &&
      is(ref.kind,
         {"communication_delivery", "communication_dossier_access"})) {
    if (!access_.communication)
      return missing<WorldMoment>("CommunicationResultOwnerNotSelected",
                                  ref.id);
    return read_communication_source_moment(*access_.communication, ref,
                                            source.source_schema);
  }
  if (ref.domain == "affairs" && ref.kind == "state_need_period_open") {
    if (!access_.affairs.state_needs)
      return missing<WorldMoment>("StateNeedResultOwnerNotSelected", ref.id);
    return read_state_need_period_opening(*access_.affairs.state_needs, ref,
                                          source.source_schema);
  }
  if (ref.domain == "affairs" && is(ref.kind, {"state_need_deadline_close",
                                               "state_claim_deadline_close"})) {
    if (!access_.affairs.state_needs)
      return missing<WorldMoment>("StateNeedResultOwnerNotSelected", ref.id);
    return read_state_need_deadline_source(*access_.affairs.state_needs, ref,
                                           source.source_schema);
  }
  if (ref.domain == "social" && ref.kind == "knowledge_receipt_v2" &&
      source.source_schema == 2) {
    const auto *d = access_.social.dynamic ? &*access_.social.dynamic : nullptr;
    const auto *b =
        d && d->time2 ? d->time2->source_time_bindings.find(ref.id) : nullptr;
    const auto *k = b ? std::get_if<SocialKnowledgeKey>(&b->key) : nullptr;
    const auto *known = k ? find_social_knowledge(*d, *k) : nullptr;
    if (!k || !known || social_time_key(*k) != ref.id ||
        known->report_id != k->report_id ||
        !same_social_subject(known->receiver, k->receiver))
      return {false,
              {},
              common_failure("KnowledgeReceiptSourceMissing", ref.id).error};
    WorldMoment at;
    if (const auto *t = std::get_if<SocialExactMoment>(&b->time_source))
      at = t->moment;
    else if (const auto *t =
                 std::get_if<SocialLegacyBoundary>(&b->time_source)) {
      if (t->tick < 0)
        return {false,
                {},
                common_failure("InvalidSocialLegacyBoundary", ref.id).error};
      at = {t->tick, {}};
    } else {
      const auto &original =
          std::get<SocialExactSource>(b->time_source).original_timed_source_ref;
      if (original.source_ref.domain == "social" &&
          original.source_ref.kind == "knowledge_receipt_v2")
        return {false,
                {},
                common_failure("RecursiveKnowledgeTimeSource", ref.id).error};
      auto original_time = read_selected_source_moment(original);
      if (!original_time.ok)
        return original_time;
      at = std::move(original_time.value);
    }
    if (at.half_day_tick != known->learned_tick)
      return {false,
              {},
              common_failure("KnowledgeTimeProjectionMismatch", ref.id).error};
    return {true, std::move(at), {}};
  }
  if (ref.domain == "social" && source.source_schema == 1) {
    if (ref.kind == "collection_cursor") {
      const auto *d =
          access_.social.dynamic ? &*access_.social.dynamic : nullptr;
      const auto *t = d && d->time2 ? &*d->time2 : nullptr;
      if (!t || t->collection_cursor.last_scanned_fact_id !=
                    std::optional<StableId>{ref.id})
        return {false,
                {},
                common_failure("SocialCollectionSourceMissing", ref.id).error};
      return {true, t->collection_cursor.through_moment, {}};
    }
    if (ref.kind == "social_period_open" || ref.kind == "social_period_close") {
      if (!access_.social_periods)
        return missing<WorldMoment>("SocialPeriodResultOwnerNotSelected",
                                    ref.id);
      const auto *o = access_.social_periods->openings.find(ref.id);
      if (!o)
        return {false,
                {},
                common_failure("SocialOpeningSourceMissing", ref.id).error};
      if (ref.kind == "social_period_open")
        return {true, o->opened_at, {}};
      const auto *c = access_.social_periods->closures.find(ref.id);
      if (!c || c->delivered_message_ids != o->due_message_ids)
        return {false,
                {},
                common_failure("SocialClosingSourceMissing", ref.id).error};
      return {true, c->closed_at, {}};
    }
  }
  if (ref.domain == "travel" && access_.movement)
    return read_native_movement_source_moment(*access_.movement, source);
  if ((ref.domain == "cultivation" &&
       (ref.kind == "training" || ref.kind == "skill_study")) ||
      (ref.domain == "services" && ref.kind == "long_rest")) {
    if (!access_.growth)
      return missing<WorldMoment>("GrowthResultOwnerNotSelected", ref.id);
    return access_.growth->read_selected_source_moment(source);
  }
  if (ref.domain == "organization" &&
      is(ref.kind,
         {"organization_office_changed", "organization_grant_issued",
          "organization_grant_amended", "organization_grant_revoked"})) {
    if (!access_.organization_authority)
      return missing<WorldMoment>("OrganizationAuthorityResultOwnerNotSelected",
                                  ref.id);
    return access_.organization_authority->read_selected_source_moment(source);
  }
  if (const auto *owner = domain_owner(ref.domain))
    return owner->read_selected_source_moment(source);
  return missing<WorldMoment>("ExactSourceOwnerNotSelected", ref.id);
}
namespace {
// Uses the original pinned fresh-text planner for both layout support and
// exact capacity. It is called only after the real destination lease exists.
std::string fresh_retained_text(std::string_view value) {


  std::string result(value.empty() ? "" : value.data(), value.size());
  return result;
}
} // namespace
class NativeExactResultRetainedCollector final
    : public SocialKnowledgeSourcePinSink {
  struct BorrowedSource {
    SourceRefBuildView source;
    Integer schema;
  };
  NativeExactResultRetainedSources &out_;
  CheckpointVector<BorrowedSource> checked_;

public:
  explicit NativeExactResultRetainedCollector(
      NativeExactResultRetainedSources &out)
      : out_(out) {}
  // Original P10 consumes exact pins only; ordinary semantic release remains
  // with its original separate consumer.
  Status original(SourceRefBuildView) override { return common_success(); }
  Status exact(const MomentOwnerSource &source) override {
    return append({source.source_ref.domain, source.source_ref.kind,
                   source.source_ref.id},
                  source.source_schema);
  }
  Status append(SourceRefBuildView source, Integer schema) {
    if (schema <= 0 || source.domain.empty() || source.kind.empty() ||
        source.id.empty())
      return common_failure("InvalidExactRetainedSource");
    for (const auto &old : checked_)
      if (old.schema == schema && old.source.domain == source.domain &&
          old.source.kind == source.kind && old.source.id == source.id)
        return common_success();
    checked_.push_back({source, schema});
    return common_success();
  }
  void finish() {


    (out_.memory_).grow(0);
    out_.sources_.reserve(checked_.size());
    for (const auto &row : checked_)
      out_.sources_.push_back({{fresh_retained_text(row.source.domain),
                                fresh_retained_text(row.source.kind),
                                fresh_retained_text(row.source.id)},
                               row.schema});
  }
};
Outcome<NativeExactResultRetainedSources>
NativeExactResultOwnerRead::retained_sources_owned() const {
  try {
    NativeExactResultRetainedSources out;
    NativeExactResultRetainedCollector sink(out);
    auto status = visit_social_knowledge_source_pins(access_.common,
                                                     access_.social, sink);
    if (!status.ok)
      return {false, {}, std::move(status.error)};
    if (access_.interaction && access_.interaction->social_occupancy) {
      for (const auto &row : access_.common.state().actors.rows()) {
        status = sink.append(
            {"interaction", "actor_social_occupancy_initial", row.first}, 1);
        if (!status.ok)
          return {false, {}, std::move(status.error)};
      }
      for (const auto &row :
           access_.interaction->social_occupancy->transitions.rows()) {
        status = sink.append(
            {"interaction", "actor_social_occupancy_changed", row.first}, 1);
        if (!status.ok)
          return {false, {}, std::move(status.error)};
      }
    }
    if (access_.communication) {
      for (const auto &row : access_.communication->deliveries.rows()) {
        status = sink.append(
            {"interaction", "communication_delivery", row.first}, 1);
        if (!status.ok)
          return {false, {}, std::move(status.error)};
      }
      for (const auto &row : access_.communication->dossier_accesses.rows()) {
        status = sink.append(
            {"interaction", "communication_dossier_access", row.first}, 1);
        if (!status.ok)
          return {false, {}, std::move(status.error)};
      }
    }
    if (access_.affairs.state_needs) {
      const auto &state = *access_.affairs.state_needs;
      for (const auto &row : state.deadline_closures.rows()) {
        auto time = read_state_need_deadline_source(
            state, row.second.closing_source_ref, 1);
        if (!time.ok)
          return {false, {}, std::move(time.error)};
        const auto &s = row.second.closing_source_ref;
        status = sink.append({s.domain, s.kind, s.id}, 1);
        if (!status.ok)
          return {false, {}, std::move(status.error)};
      }
      for (const auto &row : state.period_openings.rows()) {
        // This original reader still accepts an owning SourceRef. Its temporary
        // is charged before construction and destroyed before its local lease.


        auto memory = CheckpointMemoryCharge(0);
        SourceRef source{fresh_retained_text("affairs"),
                         fresh_retained_text("state_need_period_open"),
                         checkpoint_fresh_string_copy(row.first)};
        auto time = read_state_need_period_opening(state, source, 1);
        if (!time.ok)
          return {false, {}, std::move(time.error)};
        status =
            sink.append({"affairs", "state_need_period_open", row.first}, 1);
        if (!status.ok)
          return {false, {}, std::move(status.error)};
      }
    }
    sink.finish();
    return {true, std::move(out), {}};
  } catch (const std::bad_alloc &) {
    return {false,
            {},
            common_failure("ExactRetainedSourcesResourceExhausted", {},
                           ErrorKind::internal)
                .error};
  } catch (const std::length_error &) {
    return {false,
            {},
            common_failure("ExactRetainedSourcesResourceExhausted", {},
                           ErrorKind::internal)
                .error};
  }
}
Outcome<std::vector<MomentOwnerSource>>
NativeExactResultOwnerRead::retained_sources() const {
  auto owned = retained_sources_owned();
  if (!owned.ok)
    return {false, {}, std::move(owned.error)};
  // ABI compatibility only. No formal selected P10 caller uses this bare value.
  return {
      true, {owned.value.sources().begin(), owned.value.sources().end()}, {}};
}

} // namespace hanmo::resident

namespace hanmo::resident {
Outcome<std::unique_ptr<NativeExactResultOwnerRead>>
make_native_exact_result_sources_in_owner(
    const WorldBundle &world, const CompleteExactWorldBindingOwner &owner) {
  const auto *core = owner.reads().original_core;
  if (!core || &core->characters.early.world != &world ||
      &core->characters.early.complete != &owner)
    return {false,
            {},
            common_failure("ExactResultBoundOriginalCoreRequired").error};
  return make_native_exact_result_sources_bound(world, *core);
}
} // namespace hanmo::resident
