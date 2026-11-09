// 同原 Field target 的独立组件断言；所有真实 owner 来自原 bank，不提供空生产替身。
#include "field_reinforcement_component_test.hpp"
#include "resident/domains/military/field_battle/field_army_exit_ready.hpp"
#include "resident/domains/military/field_battle/field_exit_placement.hpp"
#include "resident/domains/military/field_battle/field_army_exit_ordinary_loss.hpp"
#include "resident/domains/military/field_battle/field_army_exit_rally.hpp"
#include "resident/domains/military/field_battle/field_member_loss_group_source.hpp"
#include "resident/kernel/world_codec.hpp"
#include "resident/kernel/native_transport_catalog.hpp"
#include "resident/kernel/native_director_wake_world.hpp"
#include "resident/kernel/world_read_stamp.hpp"
#include "resident/kernel/exact_complete_world.hpp"
#include "resident/kernel/native_exact_result_owners.hpp"
#include "resident/kernel/military_result_projection2.hpp"
#include "resident/kernel/movement_result_projection2.hpp"
#include "resident/kernel/native_field_group_world.hpp"
#include "resident/kernel/native_field_world_scope.hpp"
#include "resident/kernel/native_movement_boundary_catalog.hpp"
#include "resident/kernel/native_movement_boundary_continuation_catalog.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/domains/military/field_battle/field_ability_sources.hpp"
#include "resident/domains/military/field_battle/field_author_data.hpp"
#include "resident/domains/military/field_battle/field_boundary_install.hpp"
#include "resident/domains/military/field_battle/field_boundary_continuation_source.hpp"
#include "resident/domains/military/field_battle/field_boundary_winner_source.hpp"
#include "resident/domains/travel/movement_boundary_continuation_source.hpp"
#include "resident/domains/travel/movement_record_codec_fields.hpp"
#include "resident/domains/military/field_battle/field_contact_origin.hpp"
#include "resident/domains/military/field_battle/field_deployment.hpp"
#include "resident/domains/military/field_battle/field_reinforcement_entry.hpp"
#include "resident/domains/military/field_battle/field_reinforcement_geometry.hpp"
#include "resident/domains/military/field_battle/field_reinforcement_moment.hpp"
#include "resident/domains/military/field_battle/field_segment_settlement.hpp"
#include "resident/domains/growth/growth_quiet_source.hpp"
#include "resident/domains/actor_foundation/character_condition_world.hpp"
#include <stdexcept>
#include <exception>
#include <limits>
#include <algorithm>
namespace hanmo::resident {
// 同一薄宿主私有观测入口；借来的字符串只在同步调用内使用。
void field_gate_observation(const char *, const char *, const char *) noexcept;
namespace {
// 只包原表达式一次；END 表示返回而非业务成功，异常展开标 ABORT 后原异常继续。
class FieldStageObservation {
  const char *label_, *source_;
  int exceptions_;
public:
  explicit FieldStageObservation(const char *label, const char *source = nullptr) noexcept
      : label_(label), source_(source), exceptions_(std::uncaught_exceptions()) {
    field_gate_observation("BEGIN", label_, source_);
  }
  ~FieldStageObservation() noexcept {
    field_gate_observation(std::uncaught_exceptions() > exceptions_ ? "ABORT" : "END", label_, source_);
  }
};
// 私有宏不改变原表达式的求值次数/值类别；不向公共头增加观测接口。
#define FIELD_OBSERVE(label, ...) ([&]() -> decltype(auto) { FieldStageObservation observation(label); return (__VA_ARGS__); }())
#define FIELD_OBSERVE_SOURCE(label, source, ...) ([&]() -> decltype(auto) { FieldStageObservation observation(label, source); return (__VA_ARGS__); }())

// 测试失败只报原原因；没有容错继续或业务回执替代。
void check(bool ok, const std::string &why) {
  if (!ok) throw std::runtime_error("M28 component: " + why);
}
template <class T> T take(Outcome<T> value, const char *why) {
  check(value.ok, std::string(why) + ": " + value.error.original.code);
  return std::move(value.value);
}
void take(Status value, const char *why) {
  check(value.ok, std::string(why) + ": " + value.error.original.code);
}
template <class T> void rejected(const Outcome<T> &value, const char *code) {
  check(!value.ok && value.error.original.code == code,
        std::string("exact original rejection ") + code + ": " + value.error.original.code);
}
// 真实目录的私有反例副本只测试选型合同，不作为世界或作者输入安装。
void continuation_selection(const WorldBundle &view,
                            FieldReinforcementComponentCoverage &coverage) {
  const auto &identity = view.catalog->identity;
  check(take(selected_native_movement_boundary_continuation_version(identity),
             "actual derived continuation identity") == 1,
        "Field plus boundary requires continuation even with zero rows");
  auto missing = identity;
  std::erase_if(missing.module_versions, [](const auto &row) {
    return row.first == movement_boundary_continuation_derivation_key;
  });
  rejected(selected_native_movement_boundary_continuation_version(missing),
           "MovementBoundaryContinuationDerivedSelectionRequired");
  auto wrong = identity;
  for (auto &row : wrong.module_versions)
    if (row.first == movement_boundary_continuation_derivation_key)
      row.second = "component:wrong-original-profile";
  rejected(selected_native_movement_boundary_continuation_version(wrong),
           "MovementBoundaryContinuationDerivationMismatch");
  auto duplicate = identity;
  duplicate.module_versions.emplace_back(movement_boundary_continuation_module, "1");
  rejected(selected_native_movement_boundary_continuation_version(duplicate),
           "MovementBoundaryContinuationModuleMismatch");
  auto no_parent = identity;
  std::erase_if(no_parent.module_versions, [](const auto &row) {
    return row.first == "movement_boundary_contacts";
  });
  rejected(selected_native_movement_boundary_continuation_version(no_parent),
           "MovementBoundaryContinuationDerivedSelectionRequired");
  std::erase_if(no_parent.module_versions, [](const auto &row) {
    return row.first == movement_boundary_continuation_module ||
           row.first == movement_boundary_continuation_derivation_key;
  });
  check(take(selected_native_movement_boundary_continuation_version(no_parent),
             "unselected identity without child") == 0,
        "no boundary means no derived child");
  coverage.continuation_selection_cases += 6;
  coverage.continuation_selection_checked = true;
}
// 仅真实bank已有未核过的续行行时往返既有独立codec；空载荷不算业务正例。
void continuation_codec(WorldBundle &view,
                        FieldReinforcementComponentCoverage &coverage) {
  if (!view.movement_exact) return;
  bool fresh = false;
  for (const auto &[id, row] : view.movement_exact->boundary_continuations.rows()) {
    (void)row;
    if (!coverage.continuation_codec_rows.contains(id)) fresh = true;
  }
  if (!fresh) return;
  take(validate_native_movement_boundary_continuation_layout(view),
       "actual continuation physical owners");
  const auto records = take(native_movement_boundary_codec_selection(*view.catalog),
                            "actual movement record selection");
  const auto encoded = take(encode_movement_boundary_continuations(
      *view.movement_exact, 1, records), "original continuation JSON");
  // 这两个对象是codec的唯一解码目标，不充任生产World或伪Common owner。
  MovementExactState json_cold;
  take(decode_movement_boundary_continuations(encoded, json_cold, 1, records),
       "continuation JSON cold");
  check(compare_boundary(encoded, take(encode_movement_boundary_continuations(json_cold, 1, records),
                        "continuation JSON reencode")).exact(), "all nine JSON fields preserved");
  CodecVisitor::Buffer bytes;
  CodecVisitor writer(bytes);
  take(visit_movement_boundary_continuation_checkpoint(writer, *view.movement_exact, 1, records),
       "continuation binary write");
  take(writer.finish(), "continuation binary finish");
  MovementExactState binary_cold;
  CodecVisitor reader(static_cast<const CodecVisitor::Buffer &>(bytes));
  take(visit_movement_boundary_continuation_checkpoint(reader, binary_cold, 1, records),
       "continuation binary read");
  take(reader.finish(), "continuation binary exact end");
  check(compare_boundary(encoded, take(encode_movement_boundary_continuations(binary_cold, 1, records),
                        "continuation binary reencode")).exact(), "all nine binary fields preserved");
  rejected(encode_movement_boundary_continuations(*view.movement_exact, 2, records),
           "UnsupportedMovementBoundaryContinuationPhysicalRevision");
  check(!bytes.empty(), "continuation original bytes nonempty");
  bytes.pop_back();
  MovementExactState truncated;
  CodecVisitor short_reader(static_cast<const CodecVisitor::Buffer &>(bytes));
  auto short_read = visit_movement_boundary_continuation_checkpoint(short_reader, truncated, 1, records);
  check(!short_read.ok || !short_reader.finish().ok, "truncated continuation cannot restore");
  for (const auto &[id, row] : view.movement_exact->boundary_continuations.rows()) {
    (void)row;
    coverage.continuation_codec_rows.insert(id);
  }
  coverage.continuation_codec_cases += 4;
}
// 仅测试无来源参数的原纯几何函数，不把这些值安装成世界、军队或接触源。
void geometry(FieldReinforcementComponentCoverage &coverage) {
  FieldBattleSession session;
  FieldFormation formation;
  formation.formation_id = "component.previous";
  formation.state = FieldFormationState::Moving;
  formation.ranks = 1;
  formation.fit = {"component.a", "component.b"};
  formation.position_dm = {0, 0};
  session.formations.emplace(formation.formation_id, formation);
  const auto radius = field_formation_radius_dm(formation);
  check(radius > 0, "original nonzero circle");
  check(!take(read_field_reinforcement_entry_clear(session, {0, 0}, formation.formation_id), "center"), "occupied center blocks");
  check(!take(read_field_reinforcement_entry_clear(session, {radius, 0}, formation.formation_id), "boundary"), "circle equality blocks");
  check(take(read_field_reinforcement_entry_clear(session, {radius + 1, 0}, formation.formation_id), "outside"), "strictly outside clears");
  rejected(read_field_reinforcement_entry_clear(session, {0, 0}, "missing"), "FieldReinforcementPreviousEntryRequired");
  session.formations.at(formation.formation_id).ranks = 0;
  rejected(read_field_reinforcement_entry_clear(session, {0, 0}, formation.formation_id), "InvalidFieldFormation");
  session.formations.at(formation.formation_id).ranks = 1;
  session.formations.at(formation.formation_id).state = FieldFormationState::Withdrawn;
  check(take(read_field_reinforcement_entry_clear(session, {0, 0}, formation.formation_id), "withdrawn"), "real removed circle clears");
  coverage.geometry_cases += 6;
}
// 只在真实 bank 已有决定时冷解隔离副本；先过完整正片门，再破坏原正文。
// 副本没有外借 owner/票，普通拒绝不清 job 首错，也不改原 World 或 Common 头。
void political_payload_rejections(const WorldBundle &view, const Bytes &bank) {
  check(!view.has_active_read_phase(), "payload probes start outside view phase");
  field_gate_observation("BEGIN", "payload.construct", nullptr);
  WorldBundle negative(view.catalog, FIELD_OBSERVE("payload.decode",
      take(decode_world_checkpoint(bank, *view.catalog), "political payload original bank")));
  field_gate_observation("END", "payload.construct", nullptr);
  take(FIELD_OBSERVE("payload.transport_restore", restore_native_transport_derived_values_in_candidate(negative)),
       "political payload original transport derived values");
  take(FIELD_OBSERVE("payload.director_restore", restore_native_complete_director_wake_cache_in_candidate(negative)),
       "political payload original director derived cache");
  FIELD_OBSERVE("payload.validate_positive", take(negative.validate(), "political payload whole original validation"));
  check(negative.political_passage && !negative.political_passage->decisions.empty() &&
            !negative.has_active_read_phase(),
        "real decision and released validation phase");
  const auto id = negative.political_passage->decisions.rows().front().first;
  auto *row = negative.political_passage->decisions.find(id);
  check(row && row->decision.source_schema_version == 2, "actual selected schema2 row");
  row->decision.source_schema_version = 1;
  FIELD_OBSERVE("payload.validate_wrong_schema", rejected(negative.validate(), "FieldPassageUnsupportedDecision"));
  row->decision.source_schema_version = 2;
  check(negative.political_passage->decisions.erase(id), "remove actual payload only");
  const SourceRef source{"politics", "political_passage_decided", id};
  check(negative.common.state().facts.find(id), "original Common header remains");
  rejected(read_political_passage_decision(negative.common,
      *negative.political_passage, source), "PoliticalPassageDecisionSourceMissing");
  FIELD_OBSERVE("payload.validate_orphan", rejected(negative.validate(), "FieldPassageOrphanDecision"));
  check(!negative.has_active_read_phase(), "failed validation releases its own phase");
}
// 只检查真实 bank 里已发生的有限 Field/行军头；不编造事实，不用空表
// 算正例。篡改的是传入读口的 ref/头副本，原 World 与所有写口保持不变。
void selected_result_owners(const WorldBundle &view,
                            const CompleteExactWorldBindingOwner &owner,
                            FieldReinforcementComponentCoverage &coverage) {
  auto original = take(make_native_exact_result_sources_in_owner(view, owner),
                       "same-world P10 result owner");
  check(bool(original), "real P10 owner lifetime");
  for (const auto &[id, head] : view.common.state().facts.rows()) {
    if ((!native_field_military_result_kind(head.domain, head.kind) &&
         !native_field_movement_result_kind(head.domain, head.kind)) ||
        coverage.result_owner_facts.contains(id))
      continue;
    auto actual = take(FIELD_OBSERVE_SOURCE("P10.fact.actual", head.id.c_str(), original->read_selected_fact(head)),
                       "actual selected Field result header");
    check(actual.disposition == ExactOwnerFactRead::Disposition::no_dynamic_projection &&
              !actual.fact && !actual.action && !actual.reportable &&
              actual.direct_observers.empty(),
          "original internal Field source creates no public result or observer");
    if (head.domain == "politics" && head.kind == "political_passage_decided") {
      // 只用真实 bank 的决定行；通行 /2 不得借旧 S1 /1 获得精确时点。
      check(actual.supporting_sources.size() == 1,
            "original Field decision has one exact supporting source");
      const auto& source = actual.supporting_sources.front();
      check(source.source_schema == 2 && source.source_ref.domain == head.domain &&
                source.source_ref.kind == head.kind && source.source_ref.id == head.id,
            "original Field decision keeps schema2 and exact source identity");
      auto wrong_schema = source;
      wrong_schema.source_schema = 1;
      rejected(FIELD_OBSERVE_SOURCE("P10.moment.wrong_schema", wrong_schema.source_ref.id.c_str(), original->read_selected_source_moment(wrong_schema)),
               "UnsupportedFieldMovementResultSource");
      ++coverage.result_owner_negative_cases;
    }
    if (actual.supporting_sources.empty()) {
      // 没有原精确时刻的calendar/session头不能因为读取头成功就冒Moment源。
      const MomentOwnerSource unsupported{{head.domain, head.kind, head.id}, 1};
      check(!FIELD_OBSERVE_SOURCE("P10.moment.unsupported", head.id.c_str(), original->read_selected_source_moment(unsupported)).ok,
            "original header without exact moment rejects moment request");
      ++coverage.result_owner_negative_cases;
    }
    auto forged = head;
    forged.kind = "component:unknown-field-result-kind";
    rejected(FIELD_OBSERVE_SOURCE("P10.fact.forged", head.id.c_str(), original->read_selected_fact(forged)), "ExactFactHeaderMismatch");
    ++coverage.result_owner_negative_cases;
    for (const auto &source : actual.supporting_sources) {
      (void)take(FIELD_OBSERVE_SOURCE("P10.moment.actual", source.source_ref.id.c_str(), original->read_selected_source_moment(source)),
                 "actual selected Field result moment");
      auto wrong_schema = source;
      wrong_schema.source_schema = 97;
      check(!FIELD_OBSERVE_SOURCE("P10.moment.wrong_schema", wrong_schema.source_ref.id.c_str(), original->read_selected_source_moment(wrong_schema)).ok,
            "wrong selected result schema is rejected");
      auto wrong_domain = source;
      wrong_domain.source_ref.domain = "component:foreign-field-result-domain";
      check(!FIELD_OBSERVE_SOURCE("P10.moment.wrong_domain", wrong_domain.source_ref.id.c_str(), original->read_selected_source_moment(wrong_domain)).ok,
            "wrong selected result domain is rejected");
      coverage.result_owner_negative_cases += 2;
      ++coverage.result_owner_moment_cases;
      ++coverage.result_owner_moment_kinds[source.source_ref.domain + "/" + source.source_ref.kind];
    }
    coverage.result_owner_facts.insert(id);
    ++coverage.result_owner_fact_kinds[head.domain + "/" + head.kind];
  }
}
// 仅以原bank、原时钟和原来源头核quiet；没有long_rest也不能省掉选中读口。
void selected_growth_quiet(const WorldBundle &view,
                           const CompleteExactWorldBindingOwner &owner,
                           const FieldExactContactReadContext &field,
                           const WorldMoment &now,
                           FieldReinforcementComponentCoverage &coverage) {
  if (coverage.quiet_initial_checked &&
      (coverage.quiet_army_checked || view.military.armies.empty())) return;
  const auto body = take(owner.reads().character_conditions.sources(),
                         "same-world quiet body sources");
  check(body.fixed_quiet && body.fixed_quiet->bound_to(view.common),
        "selected occupancy supplies original quiet owner without dynamic2");
  const MilitaryFieldHistoryRead history{*view.field_battle, field.clock};
  const MilitarySelectedHistoryRead selected{view.catalog->identity,
                                             view.military_exact ? &*view.military_exact : nullptr, &history};
  for (const auto &[actor, row] : view.common.state().actors.rows()) {
    (void)row;
    const auto actual = take(body.fixed_quiet->read_actual(actor, now),
                             "original quiet current source");
    const auto frozen = take(body.fixed_quiet->read_at_source_head(
        actor, now, actual.original_sources, now), "original quiet frozen source head");
    check(actual.quiet == frozen.quiet &&
              actual.original_sources.size() == frozen.original_sources.size(),
          "quiet actual and frozen same-head values agree");
    for (std::size_t i = 0; i < actual.original_sources.size(); ++i) {
      const auto &a = actual.original_sources[i];
      const auto &b = frozen.original_sources[i];
      check(a.domain == b.domain && a.kind == b.kind && a.id == b.id,
            "quiet complete source head preserves exact original ordering");
    }
    check(!take(body.fixed_quiet->read_next_change(actor, now,
        actual.original_sources, now, actual.original_sources, now),
        "quiet unchanged original head has no successor"),
        "no fabricated quiet transition");
    const auto army = take(read_actual_actor_active_armies_selected(
        view.common, view.military, selected, &view.naval, actor, now, {},
        body.military_formations, body.military_returns), "selected actual Army replay");
    if (!army.active_army_ids.empty()) {
      check(!actual.quiet, "real active Army membership is never quiet");
      ++coverage.quiet_army_cases;
    }
    check(!actual.original_sources.empty(), "quiet has mandatory original Social head");
    auto duplicate = actual.original_sources;
    duplicate.push_back(duplicate.front());
    rejected(body.fixed_quiet->read_at_source_head(actor, now, duplicate, now),
             "GrowthQuietDuplicateSource");
    rejected(body.fixed_quiet->read_at_source_head(actor, now, {}, now),
             "GrowthQuietSocialInitialHeadRequired");
    ++coverage.quiet_current_cases;
    ++coverage.quiet_head_cases;
    coverage.quiet_negative_cases += 2;
  }
  // 只改传入目录身份副本；不改World，不用空dynamic owner充选型。
  auto wrong_identity = view.catalog->identity;
  wrong_identity.content_digest += ":component-foreign";
  const MilitarySelectedHistoryRead wrong{wrong_identity,
                                          view.military_exact ? &*view.military_exact : nullptr, &history};
  if (!view.common.state().actors.rows().empty()) {
    rejected(read_actual_actor_active_armies_selected(view.common, view.military,
        wrong, &view.naval, view.common.state().actors.rows().begin()->first,
        now, {}, body.military_formations, body.military_returns),
        "MilitaryRosterCatalogIdentityMismatch");
    ++coverage.quiet_negative_cases;
  }
  coverage.quiet_initial_checked = true;
  if (!view.military.armies.empty()) coverage.quiet_army_checked = true;
}
} // namespace
// 真 bank 的读源正例只在原执行确实产生该源时计数；空历史仅是空覆盖。
void check_field_reinforcement_components(WorldBundle &view, const Bytes &bank,
                                         FieldReinforcementComponentCoverage &coverage) {
  check(view.field_battle && view.catalog && view.catalog->field_battle, "selected original Field roots");
  if (!coverage.continuation_selection_checked)
    continuation_selection(view, coverage);
  continuation_codec(view, coverage);
  const bool passage_payload_probe = view.political_passage &&
      !view.political_passage->decisions.empty() &&
      !coverage.result_owner_fact_kinds.contains("politics/political_passage_decided");
  const bool byte_probe = !coverage.initial_checked || passage_payload_probe ||
      (!coverage.entry_rejections_checked && !view.field_battle->contacts.empty());
  Bytes before;
  if (byte_probe) before = take(FIELD_OBSERVE("component.encode", encode_world_checkpoint(view)), "before component writers");
  if (passage_payload_probe) {
    FIELD_OBSERVE("payload.probe", political_payload_rejections(view, bank));
    coverage.result_owner_negative_cases += 3;
  }
  // checkpoint 在借票外；所有探针结束、owner析构后才再编码，避免失效借用。
  {
  const auto now = take(view.actual_phase_moment(), "component actual clock");
  auto phase = take(view.enter_phase_read_context(
      "field_reinforcement_components", "field_battle", {now, now}, now),
      "original component read phase");
  // 同一来源门在已有票内只借不退；冷导出无外票的路径由 full_json 另验。
  take(FIELD_OBSERVE("component.field_sources", validate_native_field_world_sources(view)), "borrowed-phase Field source closure");
  check(view.has_active_read_phase() &&
            take(compare_world_moments(now, take(view.actual_phase_moment(),
                 "component clock after source closure")), "same component clock") == 0,
        "source closure preserves caller phase and actual moment");
  auto owner = take(view.complete_exact_time_bindings(), "original complete owner");
  check(owner && owner->reads().original_core, "actual original core");
  FIELD_OBSERVE("component.P10", selected_result_owners(view, *owner, coverage));
  auto group = take(NativeFieldGroupWorld::create(view, *owner->reads().original_core), "original group owner");
  auto &scope = group->field();
  const auto *x = scope.exact_contacts();
  const auto *relations = scope.diplomacy();
  const auto &abilities = scope.abilities();
  check(x && relations && abilities.original_rosters && abilities.exact_contacts == x,
        "original same-world entry owners");
  check_field_custody_source_components(scope.body_sources(), coverage.custody_sources);
  check_field_military_head_source_components(scope.body_sources(),
      *view.group_movement_executions, coverage.custody_sources);
  FIELD_OBSERVE("component.growth_quiet", selected_growth_quiet(view, *owner, *x, now, coverage));
  const auto &catalog = *view.catalog->field_battle;
  const auto &history = *abilities.original_rosters;
  const SourceRef missing{"military", "field_battle_contact", "component:missing:field-contact"};
  const SourceRef missing_motion{"travel", "movement_boundary_contact", "component:missing:motion-contact"};
  check(!view.common.state().facts.find(missing.id) &&
        !view.field_battle->boundary_contacts.contains(missing.id), "negative source really absent");
  if (!coverage.initial_checked) {
    geometry(coverage);
    rejected(initialize_field_reinforcement_queue_in_candidate(
        view.common, *view.field_battle, *x, *relations, catalog, history, missing,
        view.checkpoint_copy_memory), "FieldReinforcementActualContactRequired");
    rejected(install_field_boundary_contact_in_candidate(
        view.common, *view.field_battle, *x, *relations, catalog, missing_motion,
        view.checkpoint_copy_memory), "MovementBoundaryOriginalSourceRequired");
    rejected(prepare_next_field_reinforcement_in_candidate(*x, catalog, history, missing),
             "FieldReinforcementQueueSourceRequired");
    rejected(read_field_boundary_continuation_source(*x, *relations, catalog, missing),
             "FieldBoundaryContinuationContactRequired");
    // 两个候选均由同一个真实 bank 正规冷解，跨World不是空服务假对象。
    WorldBundle foreign(view.catalog, take(decode_world_checkpoint(bank, *view.catalog), "foreign original bank"));
    // 冷解副本按正式入口恢复本World派生索引，再保留完整来源校验。
    take(restore_native_transport_derived_values_in_candidate(foreign),
         "foreign original transport derived values");
    take(restore_native_complete_director_wake_cache_in_candidate(foreign),
         "foreign original director derived cache");
    take(foreign.validate(), "foreign full original validation");
    const auto foreign_before = take(encode_world_checkpoint(foreign), "foreign before");
    const auto quiet_body = take(owner->reads().character_conditions.sources(),
                                 "actual quiet source before cross-world probe");
    check(quiet_body.fixed_quiet && !quiet_body.fixed_quiet->bound_to(foreign.common),
          "quiet owner cannot bind a real distinct World decoded from the same bank");
    ++coverage.quiet_negative_cases;
    check(!make_native_field_military_result_read(foreign, *owner, *owner->reads().original_core).ok,
          "military P10 adapter rejects a real different World owner");
    check(!make_native_field_movement_result_read(foreign, *owner, *owner->reads().original_core).ok,
          "movement P10 adapter rejects a real different World owner");
    coverage.result_owner_negative_cases += 2;
    rejected(initialize_field_reinforcement_queue_in_candidate(
        foreign.common, *foreign.field_battle, *x, *relations, catalog, history, missing,
        foreign.checkpoint_copy_memory), "FieldReinforcementQueueSameCandidateRequired");
    rejected(install_field_boundary_contact_in_candidate(
        foreign.common, *foreign.field_battle, *x, *relations, catalog, missing_motion,
        foreign.checkpoint_copy_memory), "FieldBoundarySameCandidateRequired");
    check(foreign_before == take(encode_world_checkpoint(foreign), "foreign after"), "cross-world probes byte-inert on foreign view");
    coverage.negative_contract_cases += 6;
    coverage.initial_checked = true;
  }
  // 入场写口负例也借真正生成的战场几何，不给空 GeneratedBattleField 充前置。
  if (!coverage.entry_rejections_checked && !view.field_battle->contacts.empty()) {
    const auto &contact = view.field_battle->contacts.begin()->second;
    auto field = take(regenerate_field_contact_battle(*view.catalog->travel, catalog, contact.source, x), "original field geometry");
    rejected(admit_next_ai_field_reinforcement_in_candidate(
        view.common, *view.field_battle, *x, catalog, history, abilities, field,
        missing, view.checkpoint_copy_memory), "FieldReinforcementQueueSourceRequired");
    rejected(admit_next_player_field_reinforcement_in_candidate(
        view.common, *view.field_battle, *x, catalog, history, abilities, field,
        missing, view.checkpoint_copy_memory), "FieldReinforcementQueueSourceRequired");
    coverage.negative_contract_cases += 2;
    coverage.entry_rejections_checked = true;
  }
  for (const auto &[id, boundary] : view.field_battle->boundary_contacts) {
    const auto *motion = std::get_if<FieldExactBoundaryOrigin>(&boundary.motion);
    check(motion, "actual boundary exact origin");
    const auto original = take(read_field_boundary_original_source(*x, catalog, motion->actual_contact), "actual boundary producer/source");
    coverage.boundaries.insert(id);
    if (!boundary.reinforcement_queue) continue;
    const SourceRef contact{"military", "field_battle_contact", id};
    const auto *queue = take(read_field_original_reinforcement_queue(*x, catalog, history, contact), "actual queue and dual moments");
    check(queue == &*boundary.reinforcement_queue, "same real queue");
    if (!coverage.queues.contains(queue->initialized_source.id)) {
      const auto now = take(x->clock.read_current_moment(), "actual queue clock");
      check(now.half_day_tick < std::numeric_limits<Integer>::max(), "negative future representable");
      const WorldMoment future{now.half_day_tick + 1, {}};
      const auto denied = validate_field_reinforcement_application_time(
          *x, original, original.entry, future, catalog.parameters, false, {},
          &queue->initialized_source);
      check(!denied.ok && denied.error.original.code == "FieldReinforcementActualApplicationOrderRequired",
            "real queue source cannot adopt future application M");
      ++coverage.negative_time_cases;
    }
    coverage.queues.insert(queue->initialized_source.id);
    const auto &session = view.field_battle->sessions.at(boundary.battle_id);
    for (const auto &entry : queue->admitted) {
      const auto &formation = session.formations.at(entry.formation_id);
      check(take(read_field_reinforcement_admission_identity(view.common, *view.field_battle, session, formation), "actual admission identity") == &entry,
            "actual Common/Entered/queue source identity");
      if (!coverage.admissions.contains(entry.application_source.id)) {
        check(catalog.parameters.step_s > 1, "original fixture STEP allows non-grid counterexample");
        auto invalid_local = entry.at;
        ++invalid_local.t; // 原STEP为10；只篡改测试参数，不写实际队列/持久时刻。
        const auto denied = validate_field_reinforcement_application_time(
            *x, original, invalid_local, entry.applied_at, catalog.parameters, false, {},
            &entry.application_source);
        check(!denied.ok && denied.error.original.code == "FieldReinforcementActualApplicationOrderRequired",
              "real admission source cannot authenticate an off-STEP local entry");
        ++coverage.negative_time_cases;
      }
      coverage.admissions.insert(entry.application_source.id);
    }
    // 只在原门实际产生已采用的边界赢家时核完整冷链；绝不造终局/续程或改军令。
    if (boundary.entered_binding && boundary.exact_continuation_source &&
        !coverage.winner_continuations.contains(boundary.exact_continuation_source->id)) {
      const auto &actual = *boundary.exact_continuation_source;
      const auto resumed = take(read_movement_boundary_continuation_source(
          *x, *relations, catalog, actual), "actual boundary winner suffix");
      const auto cause = take(read_field_boundary_winner_resume_cause(
          *x, *relations, catalog, history, contact, {}, &actual), "actual boundary winner cause");
      check(cause.coverage == FieldBoundaryWinnerCoverage::fully_admitted &&
            cause.continuation == resumed.continuation && resumed.new_unit &&
            resumed.new_basis && !cause.active_admitted_members.empty(),
            "genuine adopted complete winner with original surviving members");
      auto missing_roster = *x;
      missing_roster.original_rosters = nullptr;
      rejected(read_movement_boundary_continuation_source(missing_roster, *relations, catalog, actual),
               "FieldBoundaryWinnerOriginalRosterOwnerRequired");
      auto missing_members = *x;
      missing_members.original_member_releases = nullptr;
      rejected(read_movement_boundary_continuation_source(missing_members, *relations, catalog, actual),
               "FieldBoundaryWinnerOriginalOwnersRequired");
      auto wrong_source = actual;
      wrong_source.domain = "military";
      rejected(read_movement_boundary_continuation_source(*x, *relations, catalog, wrong_source),
               "MovementBoundaryContinuationSourceRequired");
      coverage.negative_winner_contract_cases += 3;
      coverage.winner_continuations.insert(actual.id);
    }
  }
  // 只验证原运行真实产生的新父；不造ordinary名单、deadline或正写口。
  for (const auto &receipt : view.military.casualty_facts) {
    const auto *parent = view.common.state().facts.find(receipt.battle_id);
    if (!parent || parent->domain != "military" || parent->kind != "field_army_exit_step" ||
        coverage.ordinary_casualties.contains(receipt.id)) continue;
    const SourceRef casualty{"military","military_casualties",receipt.id};
    const auto original = take(read_field_army_exit_ordinary_casualty_source(
        view.common,view.military,*view.field_battle,casualty,x->clock), "real step casualty source");
    check(original.casualties == &receipt && original.step && original.plan,
          "ordinary source is exact bank row, not old terminal parent");
    check(scope.group_handoffs(), "ordinary same-world group owner");
    take(validate_field_army_exit_ordinary_loss_applications(*scope.group_handoffs()),
         "ordinary original S4/S1 reverse coverage");
    SourceRef wrong = casualty; wrong.domain = "travel";
    rejected(read_field_army_exit_ordinary_casualty_source(view.common,view.military,
        *view.field_battle,wrong,x->clock), "FieldArmyExitLossActualCasualtyRequired");
    wrong = casualty; wrong.id = "component:missing:ordinary-casualty";
    check(!view.common.state().facts.find(wrong.id), "negative casualty actually absent");
    rejected(read_field_army_exit_ordinary_casualty_source(view.common,view.military,
        *view.field_battle,wrong,x->clock), "FieldArmyExitLossActualCasualtyRequired");
    coverage.negative_ordinary_loss_cases += 2;
    if (!coverage.ordinary_cross_world_checked) {
      // 同真bank冷解另一World，不以空owner假对象模拟匿名批跨根反例。
      WorldBundle foreign(view.catalog, take(decode_world_checkpoint(bank,*view.catalog),
                                             "ordinary foreign original bank"));
      // 不借热World的cache；由这个冷候选的原源独立恢复派生值。
      take(restore_native_transport_derived_values_in_candidate(foreign),
           "ordinary foreign transport derived values");
      take(restore_native_complete_director_wake_cache_in_candidate(foreign),
           "ordinary foreign director derived cache");
      take(foreign.validate(), "ordinary foreign full validation");
      const auto foreign_now = take(foreign.actual_phase_moment(), "ordinary foreign clock");
      auto foreign_phase = take(foreign.enter_phase_read_context(
          "field_ordinary_foreign_component", "field_battle",
          {foreign_now, foreign_now}, foreign_now), "ordinary foreign read phase");
      auto foreign_owner = take(foreign.complete_exact_time_bindings(), "ordinary foreign core");
      auto foreign_group = take(NativeFieldGroupWorld::create(foreign,
          *foreign_owner->reads().original_core), "ordinary foreign group");
      auto crossed = *scope.group_handoffs();
      crossed.exact_contacts = foreign_group->field().exact_contacts();
      crossed.original_abilities = &foreign_group->field().abilities();
      crossed.original_body = &foreign_group->field().body_sources();
      const auto rejected_owner = validate_field_army_exit_ordinary_loss_applications(crossed);
      check(!rejected_owner.ok && rejected_owner.error.original.code ==
          "FieldArmyExitLossOriginalAbilitiesRequired", "anonymous full source cannot borrow foreign roots");
      ++coverage.negative_ordinary_loss_cases;
      coverage.ordinary_cross_world_checked = true;
    }
    const SourceRef step{"military","field_army_exit_step",original.step->fact_id};
    for (const auto &motion : original.step->formations) {
      for (const auto &actor : motion.deserted_member_ids) {
        const auto departure = take(read_field_army_exit_ordinary_departure_source(
            *x,catalog,abilities,step,actor), "actual ordinary departure source");
        check(departure.motion == &motion && departure.source.step == original.step,
              "actual ordinary motion point source");
        ++coverage.ordinary_roster_departures;
      }
      if (!motion.deserted_member_ids.empty()) {
        const auto &formation = original.session->formations.at(motion.formation_id);
        rejected(read_field_army_exit_ordinary_departure_source(*x,catalog,abilities,step,
            formation.commander_actor_id), "FieldArmyExitOrdinaryActorRequired");
        ++coverage.negative_ordinary_loss_cases;
      }
    }
    for (const auto &change : receipt.actor_changes) {
      auto absent = *scope.group_handoffs(); absent.original_abilities = nullptr;
      rejected(read_field_member_loss_group_origin(absent,casualty,change.actor_id),
               "FieldArmyExitLossOriginalAbilitiesRequired");
      ++coverage.negative_ordinary_loss_cases;
      ++coverage.ordinary_named_adoptions;
    }
    coverage.ordinary_casualties.insert(receipt.id);
  }
  // 真有限返程出现才核原held子集与先前退出的互斥终点，不制造新汇合或返程输入。
  for (const auto &[id, plan] : view.field_battle->army_exit_plans) {
    if (!plan.actual_continuation || coverage.finite_returns.contains(plan.actual_continuation->id)) continue;
    const auto &source = *plan.actual_continuation;
    const auto actual = take(read_field_army_exit_continuation_source(
        *x, catalog, abilities, source), "actual whole-army finite return");
    check(actual.ready.plan == &plan && actual.cancellation &&
          (actual.already_at_destination
              ? !actual.opening && !actual.journey &&
                    actual.cancellation == view.group_movement_executions->controls.find(source.id)
              : actual.opening && actual.journey &&
                    actual.opening == view.group_movement_executions->openings.find(source.id)),
          "finite return keeps original bank row identities");
    if (actual.already_at_destination) ++coverage.stable_return_sources;
    check(scope.placements(), "original Field engagement history owner");
    for (const auto &actor : actual.ready.named_participants) {
      const auto entries = take(scope.placements()->read_field_actor_engagement_history(actor),
                                "real finite return actor engagement");
      const FieldActorEngagementRead *selected = nullptr;
      for (const auto &entry : entries)
        if (entry.battle_id && *entry.battle_id == plan.battle_id && entry.journey_id &&
            *entry.journey_id == actual.ready.original_journey->journey_id) {
          check(!selected, "one original finite return field entry"); selected = &entry;
        }
      check(selected, "actual returning actor has original field entry");
      const bool held = std::count(actual.ready.held_named_participants.begin(),
                                    actual.ready.held_named_participants.end(), actor) == 1;
      if (held) {
        if (actual.already_at_destination) {
          const auto *stable = std::get_if<const GroupMovementControlSource *>(&selected->release);
          check(stable && *stable == actual.cancellation, "held member ends at exact typed stable cancel");
        } else {
          const auto *returned = std::get_if<const GroupMovementOpeningSource *>(&selected->release);
          check(returned && *returned == actual.opening,
                "held member ends at exact new opening, not forged member release");
        }
        ++coverage.finite_return_held_members;
      } else {
        const auto *prior = std::get_if<const GroupContactMemberReleaseSource *>(&selected->release);
        check(prior && *prior && std::find(actual.ready.prior_orderly_releases.begin(),
            actual.ready.prior_orderly_releases.end(), *prior) != actual.ready.prior_orderly_releases.end(),
            "previous orderly exit keeps its original endpoint");
        ++coverage.finite_return_prior_exits;
      }
      if (actual.already_at_destination) {
        const auto point = take(scope.placements()->read_field_army_exit_stable_placement({source, 1}, actor),
                                "actual stable ready physical point");
        check(point.army_id == &plan.army_id && point.actual.actor_id == actor &&
              point.actual.immutable_source_head.size() == 2 &&
              point.actual.immutable_source_head[0].source.id == source.id &&
              point.actual.immutable_source_head[1].source.id == plan.ready_source->id &&
              take(same_spatial_anchor(point.actual.actual_anchor, actual.ready.actual_anchor),
                   "stable actual point comparison"), "stable true Army and double source heads");
        auto absent = *scope.group_handoffs(); absent.original_abilities = nullptr;
        OriginalFieldBattleExitPlacementRead no_ready_owner(absent);
        rejected(no_ready_owner.read_field_army_exit_stable_placement({source, 1}, actor),
                 "FieldArmyExitStablePlacementOriginalOwnerRequired");
        ++coverage.negative_finite_return_cases;
      }
    }
    auto wrong = source; wrong.domain = "military";
    rejected(read_field_army_exit_continuation_source(*x, catalog, abilities, wrong),
             "FieldArmyExitReturnContinuationSourceRequired");
    auto no_original_returns = *x; no_original_returns.original_finite_returns = nullptr;
    rejected(read_field_army_exit_continuation_source(no_original_returns, catalog, abilities, source),
             "FieldArmyExitReturnContinuationSourceRequired");
    coverage.negative_finite_return_cases += 2;
    coverage.finite_returns.insert(source.id);
  }
  } // same-world source leases end before checkpoint comparison
  if (byte_probe)
    check(before == take(FIELD_OBSERVE("component.encode", encode_world_checkpoint(view)), "after component writers"),
          "all rejected writers preserve original bank bytes");
}
// 此报告不增加门内事件要求，亦不把零命中写成援军 producer 正例通过。
std::string field_reinforcement_component_report(const FieldReinforcementComponentCoverage &c) {
  // 按实际 kind 分列，某类已命中绝不冒充其他 Field/行军原 owner 已覆盖。
  std::string result_kinds;
  for (const auto &[kind, count] : c.result_owner_fact_kinds)
    result_kinds += "selected_result_header_kind[" + kind + "]=" + std::to_string(count) + "\n";
  for (const auto &[kind, count] : c.result_owner_moment_kinds)
    result_kinds += "selected_result_moment_kind[" + kind + "]=" + std::to_string(count) + "\n";
  return field_custody_source_component_report(c.custody_sources) + "\nquiet_current_cases=" + std::to_string(c.quiet_current_cases) +
         "\nquiet_frozen_head_cases=" + std::to_string(c.quiet_head_cases) +
         "\nquiet_negative_cases=" + std::to_string(c.quiet_negative_cases) +
         "\nquiet_actual_army_cases=" + std::to_string(c.quiet_army_cases) +
         "\n" + (c.quiet_army_cases == 0
             ? "genuine_quiet_army_coverage=NOT_EXERCISED\n"
             : "genuine_quiet_army_coverage=OBSERVED_ORIGINAL_BANK\n") +
         result_kinds + "actual_selected_result_headers=" + std::to_string(c.result_owner_facts.size()) +
         "\nactual_selected_result_moments=" + std::to_string(c.result_owner_moment_cases) +
         "\nresult_owner_negative_cases=" + std::to_string(c.result_owner_negative_cases) +
         "\n" + (c.result_owner_facts.empty()
             ? "genuine_selected_result_owner_coverage=NOT_EXERCISED\n"
             : "genuine_selected_result_owner_coverage=OBSERVED_LISTED_KINDS_ONLY\n") +
         "geometry_cases=" + std::to_string(c.geometry_cases) +
         "\nnegative_contract_cases=" + std::to_string(c.negative_contract_cases) +
         "\nnegative_time_cases=" + std::to_string(c.negative_time_cases) +
         "\ncontinuation_selection_cases=" + std::to_string(c.continuation_selection_cases) +
         "\ncontinuation_codec_cases=" + std::to_string(c.continuation_codec_cases) +
         "\nactual_continuation_codec_rows=" + std::to_string(c.continuation_codec_rows.size()) +
         "\n" + (c.continuation_codec_rows.empty()
             ? "genuine_continuation_codec_coverage=NOT_EXERCISED"
             : "genuine_continuation_codec_coverage=OBSERVED_ORIGINAL_BANK") +
         "\nactual_boundary_sources=" + std::to_string(c.boundaries.size()) +
         "\nactual_queue_sources=" + std::to_string(c.queues.size()) +
         "\nactual_admission_sources=" + std::to_string(c.admissions.size()) +
         "\nactual_boundary_winner_continuations=" + std::to_string(c.winner_continuations.size()) +
         "\nnegative_winner_contract_cases=" + std::to_string(c.negative_winner_contract_cases) +
         "\n" + (c.winner_continuations.empty()
             ? "genuine_boundary_winner_coverage=NOT_EXERCISED\n"
             : "genuine_boundary_winner_coverage=OBSERVED_ORIGINAL_BANK\n") +
         "actual_ordinary_casualties=" + std::to_string(c.ordinary_casualties.size()) +
         "\nordinary_roster_departures=" + std::to_string(c.ordinary_roster_departures) +
         "\nordinary_named_adoptions=" + std::to_string(c.ordinary_named_adoptions) +
         "\nnegative_ordinary_loss_cases=" + std::to_string(c.negative_ordinary_loss_cases) +
         "\n" + (c.ordinary_casualties.empty()
             ? "genuine_ordinary_loss_coverage=NOT_EXERCISED\n"
             : "genuine_ordinary_loss_coverage=OBSERVED_ORIGINAL_BANK\n") +
         (c.ordinary_named_adoptions == 0
             ? "genuine_ordinary_named_capture_coverage=NOT_EXERCISED\n"
             : "genuine_ordinary_named_capture_coverage=OBSERVED_ORIGINAL_BANK\n") +
         "actual_finite_return_sources=" + std::to_string(c.finite_returns.size()) +
         "\nstable_return_sources=" + std::to_string(c.stable_return_sources) +
         "\nfinite_return_held_members=" + std::to_string(c.finite_return_held_members) +
         "\nfinite_return_prior_exits=" + std::to_string(c.finite_return_prior_exits) +
         "\nnegative_finite_return_cases=" + std::to_string(c.negative_finite_return_cases) +
         "\n" + (c.finite_returns.empty()
             ? "genuine_finite_return_coverage=NOT_EXERCISED\n"
             : "genuine_finite_return_coverage=OBSERVED_ORIGINAL_BANK\n") +
         (c.stable_return_sources == 0
             ? "genuine_stable_return_coverage=NOT_EXERCISED\n"
             : "genuine_stable_return_coverage=OBSERVED_ORIGINAL_BANK\n") +
         "forced_positive_writer=NOT_RUN\n" +
         (c.admissions.empty() ? "genuine_admission_coverage=NOT_EXERCISED\n" : "genuine_admission_coverage=OBSERVED_ORIGINAL_BANK\n");
}
} // namespace hanmo::resident

#undef FIELD_OBSERVE_SOURCE
#undef FIELD_OBSERVE
