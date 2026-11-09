#include "resident/kernel/catalog_section_observation.hpp"
// 实际 Inspect
// 才追加取得源；冷热两边均反读原供货快照，绝不更新旧报告的观察日期。
#include "public_supply_inspection.hpp"
#include "interaction_model.hpp"
#include "resident/domains/local_economy/local_economy_model.hpp"
#include "resident/domains/social/social_knowledge_cut.hpp"
#include "resident/views/scene_presentation.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
template <class T>
Outcome<T> fail(const char *code, const StableId &id = {},
                ErrorKind kind = ErrorKind::rule) {
  return {false, {}, common_failure(code, id, kind).error};
}
bool same_source(const SourceRef &a, const SourceRef &b) {
  return a.domain == b.domain && a.kind == b.kind && a.id == b.id;
}
const PropDefinition *prop(const InteractionCatalog &catalog,
                           const StableId &id) {
  for (const auto &row : catalog.props)
    if (row.id == id)
      return &row;
  return nullptr;
}
bool same_supply(const SupplyObservation &a, const SupplyObservation &b) {
  return a.source_key == b.source_key &&
         a.organization_id == b.organization_id && a.site_id == b.site_id &&
         a.inventory_id == b.inventory_id && a.item_id == b.item_id &&
         a.source_fact_id == b.source_fact_id &&
         a.target_amount == b.target_amount &&
         a.physical_amount == b.physical_amount &&
         a.source_revision == b.source_revision;
}
// 实际 writer 的临时深值先收费，原表再按自己的复制边界接管各行。
struct InspectionWriteValues {
  CheckpointMemoryCharge memory;
  PublicSupplyInspection source;
  SourceRef ref;
  KnownReport known;
  SocialSourceTimeBinding learned;
  static CheckpointMemoryCharge reserve(const StableId &id,
                                        const SubjectRef &receiver,
                                        const StableId &object,
                                        const Report &report,
                                        const SettlementLocation &location) {


    // ref、精确时间来源、取得来源、稍后由原 Common 尾捕获的因果切点。
    (void)0;
    // 原 KnownReport 的双 subject 与唯一 SocialKnowledgeKey。
    (void)0;


    return CheckpointMemoryCharge(0);
  }
  InspectionWriteValues(const StableId &id, const SubjectRef &receiver,
                        const StableId &object, const Report &report,
                        const SettlementLocation &location,
                        const WorldMoment &at)
      : memory(reserve(id, receiver, object, report, location)),
        source{
            id,
            receiver.id,
            object,
            report.id,
            {{"local_economy", "local_supply_observation", report.fact_id}, 1},
            report.observation.supply->source_key,
            location.settlement_id,
            location.place_id,
            at},
        ref{"interaction", public_supply_inspection_source_kind, id},
        known{receiver, report.id, {}, at.half_day_tick, receiver},
        learned{SocialKnowledgeKey{receiver, report.id},
                SocialExactSource{{ref, 1}}, ref} {}
};
// 公开公告必须确属该既有场所的同一注册货源；不从商品相同推断可读库存。
Status notice(const PublicSupplyInspectionReadContext &x, const StableId &id,
              const StableId *&source, const PropDefinition *&object) {
  source = x.catalog.notice_sources.find(id);
  object = prop(x.interactions, id);
  if (x.catalog.version != 1 || x.state.version != 1)
    return common_failure("UnsupportedPublicSupplyInspectionVersion", id,
                          ErrorKind::unsupported_capability);
  if (!source || !object)
    return common_failure("PublicSupplyNoticeRequired", id);
  const auto *definition = x.economy_catalog.supply_sources.find(*source);
  if (!definition || definition->source_key != *source ||
      !definition->public_report || definition->site_id != object->place_id)
    return common_failure("InvalidPublicSupplyNotice", id);
  return common_success();
}
// 原经济观察和原报告完全一致才可作为公告内容；冷读不重新观察现在的库存。
Status original_report(const PublicSupplyInspectionReadContext &x,
                       const Report &r, const StableId &key,
                       const SourceRef &original) {
  const auto *h = x.common.state().facts.find(original.id);
  const auto *o = x.economy.supply_observations.find(original.id);
  if (original.domain != "local_economy" ||
      original.kind != "local_supply_observation" || !h ||
      h->domain != original.domain || h->kind != original.kind || !o ||
      o->id != original.id || !o->publicly_visible ||
      o->observation.source_key != key ||
      o->observation.source_fact_id != original.id ||
      r.fact_id != original.id || r.payload_kind != original.kind ||
      r.observation.id != original.id || r.observation.kind != original.kind ||
      r.observation.tick != o->tick || r.observation_tick != o->tick ||
      r.visibility_scope != ReportVisibility::local_public ||
      !r.observation.publicly_visible || !r.observation.supply ||
      !same_supply(*r.observation.supply, o->observation))
    return common_failure("PublicSupplyOriginalReportMismatch", r.id,
                          ErrorKind::internal);
  return common_success();
}
// 本次只读票须确属同一世界版本与实际时刻，不接受另一份快照的报告指针。
Status actual_read(const PublicSupplyInspectionReadContext &x,
                   const ExactResultTimeRead &results,
                   const ExactResultReadStamp &stamp, const WorldMoment &at) {
  auto valid = results.validate_history_stamp(stamp);
  if (!valid.ok)
    return valid;
  const auto &a = x.common.state().identity;
  const auto &b = results.source_identity();
  if (a.world_id != b.world_id || a.execution_epoch != b.execution_epoch ||
      a.revision != b.revision || a.tick != b.tick)
    return common_failure("PublicSupplyInspectionWorldMismatch", {},
                          ErrorKind::internal);
  auto same = compare_world_moments(results.source_moment(), at);
  if (!same.ok)
    return {false, {}, same.error};
  return same.value ? common_failure("PublicSupplyInspectionMomentMismatch", {},
                                     ErrorKind::internal)
                    : common_success();
}
} // namespace
Status validate_public_supply_inspection_catalog(
    const PublicSupplyInspectionCatalog &catalog,
    const InteractionCatalog &interaction, const LocalEconomyCatalog &economy) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 58, "PublicSupplyInspectionCatalog", "validate_public_supply_inspection_catalog", &catalog);

  if (catalog.version != 1)
    return common_failure("UnsupportedPublicSupplyInspectionVersion", {},
                          ErrorKind::unsupported_capability);
  for (const auto &row : catalog.notice_sources.rows()) {
    const auto *object = prop(interaction, row.first);
    const auto *source = economy.supply_sources.find(row.second);
    if (row.first.empty() || row.second.empty() || !object || !source ||
        source->source_key != row.second || !source->public_report ||
        source->site_id != object->place_id || !interaction.scene ||
        !interaction.scene->places.find(object->place_id))
      return common_failure("InvalidPublicSupplyNotice", row.first);
  }
  return common_success();
}
Outcome<const Report *> preview_public_supply_inspection(
    const PublicSupplyInspectionReadContext &x,
    const ExactResultTimeRead &results, const ExactResultReadStamp &stamp,
    const StableId &actor, const StableId &object_id) {
  const StableId *key = nullptr;
  const PropDefinition *object = nullptr;
  auto valid = notice(x, object_id, key, object);
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto *person = x.common.actor(actor);
  if (!person || !person->alive || !person->available)
    return fail<const Report *>("PersonAbsent", actor);
  const auto *location = std::get_if<SettlementLocation>(&person->location);
  if (!location || location->place_id != object->place_id)
    return fail<const Report *>("NotAtPlace", object_id);
  const auto *site = x.interactions.scene
                         ? x.interactions.scene->places.find(object->place_id)
                         : nullptr;
  if (!site || site->settlement_id != location->settlement_id)
    return fail<const Report *>("PublicSupplyNoticeLocationMismatch",
                                object_id);
  if (!x.social.dynamic || !x.social.dynamic->time2 ||
      !social_knowledge_cut_version(x.social.dynamic->version))
    return fail<const Report *>("PublicSupplyKnowledgeOwnerRequired", actor,
                                ErrorKind::unsupported_capability);
  auto at = x.clock.read_current_moment();
  if (!at.ok)
    return {false, {}, at.error};
  valid = actual_read(x, results, stamp, at.value);
  if (!valid.ok)
    return {false, {}, valid.error};
  const Report *selected = nullptr;
  const DynamicFactViewV2 *selected_fact = nullptr;
  for (const auto &row : x.social.dynamic->reports.rows()) {
    const auto &r = row.second;
    if (!r.observation.supply || r.observation.supply->source_key != *key)
      continue;
    {


      auto original_memory = CheckpointMemoryCharge(0);
      valid = original_report(
          x, r, *key, {"local_economy", "local_supply_observation", r.fact_id});
    }
    if (!valid.ok)
      return {false, {}, valid.error};
    valid = validate_exact_report_source(x.common, r, results, stamp);
    if (!valid.ok)
      return {false, {}, valid.error};
    auto fact = results.fact_v2_borrowed(r.fact_id, stamp);
    if (!fact.ok)
      return {false, {}, fact.error};
    if (!fact.value)
      return fail<const Report *>("PublicSupplyObservationMissing", r.fact_id,
                                  ErrorKind::internal);
    auto order = compare_world_moments(fact.value->occurred_at, at.value);
    if (!order.ok)
      return {false, {}, order.error};
    if (order.value > 0)
      continue;
    if (r.valid_until_tick && *r.valid_until_tick < at.value.half_day_tick)
      continue;
    if (selected) {
      order = compare_world_moments(fact.value->occurred_at,
                                    selected_fact->occurred_at);
      if (!order.ok)
        return {false, {}, order.error};
      if (order.value < 0 ||
          (order.value == 0 &&
           r.observation.supply->source_revision <=
               selected->observation.supply->source_revision))
        continue;
    }
    selected = &r;
    selected_fact = fact.value;
  }
  if (!selected)
    return fail<const Report *>("PublicSupplyReportUnavailable", object_id);
  return {true, selected, {}};
}
// 记录只在本次真实操作产生；重复看已经知道的同一原报告不分新
// ID、不刷新获知时间。
Status inspect_public_supply_in_candidate(
    const PublicSupplyInspectionReadContext &x, CommonStore &common,
    SocialState &social, PublicSupplyInspectionState &state,
    const ExactResultTimeRead &results, const ExactResultReadStamp &stamp,
    const StableId &actor, const StableId &object) {
  if (&common != &x.common || &social != &x.social || &state != &x.state)
    return common_failure("PublicSupplyInspectionWriteOwnerMismatch", {},
                          ErrorKind::internal);
  auto selected =
      preview_public_supply_inspection(x, results, stamp, actor, object);
  if (!selected.ok)
    return {false, {}, selected.error};
  const auto &r = *selected.value;


  auto receiver_memory = CheckpointMemoryCharge(0);
  const SubjectRef receiver{SubjectKind::actor, actor};
  auto &dynamic = *social.dynamic;
  if (find_social_knowledge(dynamic, receiver, r.id))
    return common_success();
  auto at = x.clock.read_current_moment();
  if (!at.ok)
    return {false, {}, at.error};
  auto valid = actual_read(x, results, stamp, at.value);
  if (!valid.ok)
    return valid;
  const auto *location =
      std::get_if<SettlementLocation>(&common.actor(actor)->location);
  if (!location)
    return common_failure("NotAtPlace", object);
  auto id = common.next_id("result");
  if (!id.ok)
    return {false, {}, id.error};
  InspectionWriteValues values(id.value, receiver, object, r, *location,
                               at.value);
  const auto &ref = values.ref;
  {


    auto header_memory = CheckpointMemoryCharge(0);
    valid = common.append_fact({ref.id, ref.kind, ref.domain});
  }
  if (!valid.ok)
    return valid;
  if (!state.results.insert(values.source.id, values.source))
    return common_failure("DuplicatePublicSupplyInspection", ref.id,
                          ErrorKind::internal);
  const OwnedSocialIndexKey key(receiver);
  if (!dynamic.known_reports.find(key.value()))
    dynamic.known_reports.insert(key.value(), {});
  if (!dynamic.known_reports.find(key.value())->insert(r.id, values.known))
    return common_failure("PublicSupplyKnowledgeConflict", r.id,
                          ErrorKind::internal);
  valid = capture_social_knowledge_cut(common, dynamic.version, values.learned);
  if (!valid.ok)
    return valid;
  const OwnedSocialIndexKey learned_key(values.learned.key);
  if (!dynamic.time2->source_time_bindings.insert(learned_key.value(),
                                                  values.learned))
    return common_failure("PublicSupplyKnowledgeBindingConflict", r.id,
                          ErrorKind::internal);
  return common_success();
}
bool is_public_supply_inspection_source(const SourceRef &source) {
  return source.domain == "interaction" &&
         source.kind == public_supply_inspection_source_kind;
}
Outcome<const PublicSupplyInspection *>
read_public_supply_inspection_source(const PublicSupplyInspectionReadContext &x,
                                     const SourceRef &ref, Integer schema) {
  if (schema != 1 || !is_public_supply_inspection_source(ref))
    return fail<const PublicSupplyInspection *>(
        "UnsupportedPublicSupplyInspectionSource", ref.id,
        ErrorKind::unsupported_capability);
  const auto *r = x.state.results.find(ref.id);
  const auto *h = x.common.state().facts.find(ref.id);
  if (!r || !h || r->id != ref.id || h->domain != ref.domain ||
      h->kind != ref.kind || !x.common.actor(r->actor_id) || !x.social.dynamic)
    return fail<const PublicSupplyInspection *>(
        "PublicSupplyInspectionSourceMissing", ref.id, ErrorKind::internal);
  const StableId *key = nullptr;
  const PropDefinition *object = nullptr;
  auto valid = notice(x, r->prop_id, key, object);
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto *site = x.interactions.scene
                         ? x.interactions.scene->places.find(r->place_id)
                         : nullptr;
  const auto *report = x.social.dynamic->reports.find(r->report_id);
  if (*key != r->source_key || object->place_id != r->place_id || !site ||
      site->settlement_id != r->settlement_id ||
      r->original_observation.source_schema != 1 || !report)
    return fail<const PublicSupplyInspection *>(
        "PublicSupplyInspectionBindingMismatch", ref.id, ErrorKind::internal);
  valid = original_report(x, *report, *key, r->original_observation.source_ref);
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto *observation = x.economy.supply_observations.find(report->fact_id);
  auto before = compare_world_moments(WorldMoment{observation->tick, {}},
                                      r->inspected_at);
  if (!before.ok)
    return {false, {}, before.error};
  auto now = x.clock.read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  auto future = compare_world_moments(r->inspected_at, now.value);
  if (!future.ok)
    return {false, {}, future.error};
  if (before.value > 0 || future.value > 0 ||
      (report->valid_until_tick &&
       r->inspected_at.half_day_tick > *report->valid_until_tick))
    return fail<const PublicSupplyInspection *>(
        "PublicSupplyInspectionTimeMismatch", ref.id, ErrorKind::internal);
  const auto &order = x.common.state().facts.order();
  auto a = std::find(order.begin(), order.end(), report->fact_id),
       b = std::find(order.begin(), order.end(), ref.id);
  if (a == order.end() || b == order.end() || a >= b)
    return fail<const PublicSupplyInspection *>(
        "PublicSupplyInspectionSourceOrderMismatch", ref.id,
        ErrorKind::internal);
  return {true, r, {}};
}
Outcome<ExactOwnerFactRead> project_public_supply_inspection_source(
    const PublicSupplyInspectionReadContext &x, const SourceRef &ref,
    Integer schema) {
  auto read = read_public_supply_inspection_source(x, ref, schema);
  if (!read.ok)
    return {false, {}, read.error};
  const auto &r = *read.value;
  // 深值构造前预留精确普通字段；同一租约随原 P10 Row 接管，不能在返回时退账。


  ExactOwnerFactRead out;
  out.payload_memory = CheckpointCopyLease::reserve();
  DynamicFactViewV2 fact;
  fact.id = checkpoint_fresh_string_copy(r.id);
  fact.kind = StableId{public_supply_inspection_source_kind};
  fact.rule_version = StableId{"public_supply_inspection_v1"};
  fact.occurred_at = r.inspected_at;
  fact.time_basis = OwnerFactMoment{ref, 1};
  fact.cause_fact_ids.reserve(1);
  fact.cause_fact_ids.push_back(r.original_observation.source_ref.id);
  fact.participants.reserve(1);
  fact.participants.push_back({{SubjectKind::actor, r.actor_id}, "reader"});
  fact.subject_id = checkpoint_fresh_string_copy(r.prop_id);
  fact.outcome = "read";
  out.fact = std::move(fact);
  out.reportable = false;
  return {true, std::move(out), {}};
}
Status validate_public_supply_inspection_sources(
    const PublicSupplyInspectionReadContext &x) {
  if (x.state.version != 1)
    return common_failure("UnsupportedPublicSupplyInspectionVersion", {},
                          ErrorKind::unsupported_capability);
  auto valid = validate_public_supply_inspection_catalog(
      x.catalog, x.interactions, x.economy_catalog);
  if (!valid.ok)
    return valid;
  for (const auto &row : x.state.results.rows()) {


    auto scope_memory = CheckpointMemoryCharge(0);
    const SourceRef ref{"interaction", public_supply_inspection_source_kind,
                        row.first};
    auto source = read_public_supply_inspection_source(x, ref);
    if (!source.ok)
      return {false, {}, source.error};
    const auto &r = *source.value;
    if (!x.social.dynamic || !x.social.dynamic->time2)
      return common_failure("PublicSupplyKnowledgeOwnerRequired", r.id);
    const SubjectRef actor{SubjectKind::actor, r.actor_id};
    const auto *k =
        find_social_knowledge(*x.social.dynamic, actor, r.report_id);
    const SocialTimeKey knowledge{SocialKnowledgeKey{actor, r.report_id}};
    const OwnedSocialIndexKey knowledge_key(knowledge);
    const auto *b = x.social.dynamic->time2->source_time_bindings.find(
        knowledge_key.value());
    const auto *time =
        b ? std::get_if<SocialExactSource>(&b->time_source) : nullptr;
    if (!k || !b || !time || k->source_message_id ||
        !same_social_subject(k->source_subject, actor) ||
        !same_source(b->actual_source_ref, ref) ||
        !same_source(time->original_timed_source_ref.source_ref, ref) ||
        time->original_timed_source_ref.source_schema != 1 ||
        k->learned_tick != r.inspected_at.half_day_tick)
      return common_failure("PublicSupplyInspectionKnowledgeMismatch", r.id,
                            ErrorKind::internal);
    valid = validate_social_knowledge_cut(x.common, *x.social.dynamic, *b);
    if (!valid.ok)
      return valid;
  }
  for (const auto &row : x.common.state().facts.rows())
    if (row.second.domain == "interaction" &&
        row.second.kind == public_supply_inspection_source_kind &&
        !x.state.results.find(row.first))
      return common_failure("OrphanPublicSupplyInspection", row.first,
                            ErrorKind::internal);
  return common_success();
}
Outcome<const PublicSupplyInspection *>
read_public_supply_inspection_command_effect(
    const PublicSupplyInspectionState &before,
    const PublicSupplyInspectionReadContext &after, const StableId &actor,
    const StableId &object, const WorldMoment &actual) {
  if (&before == &after.state || before.version != 1 ||
      after.state.version != 1)
    return fail<const PublicSupplyInspection *>(
        "PublicSupplyInspectionOriginalDeltaRequired", object,
        ErrorKind::internal);
  const auto &original = before.results.rows();
  const auto &applied = after.state.results.rows();
  if (original.size() == applied.size())
    return fail<const PublicSupplyInspection *>(
        "PublicSupplyInspectionNoNewSource", object);
  if (original.size() == SIZE_MAX || applied.size() != original.size() + 1 ||
      before.results.order().size() != original.size() ||
      after.state.results.order().size() != applied.size())
    return fail<const PublicSupplyInspection *>(
        "PublicSupplyInspectionDeltaMismatch", object, ErrorKind::internal);
  // 精确字段直接比较，旧表的任何删除、改写、换序均不成为一次新 Inspect。
  for (std::size_t i = 0; i != original.size(); ++i) {
    const auto &left = original[i];
    const auto &right = applied[i];
    const auto &a = left.second;
    const auto &b = right.second;
    if (left.first != right.first || left.first != a.id ||
        right.first != b.id || before.results.order()[i] != left.first ||
        after.state.results.order()[i] != right.first ||
        before.results.find(left.first) != &a ||
        after.state.results.find(right.first) != &b ||
        a.actor_id != b.actor_id || a.prop_id != b.prop_id ||
        a.report_id != b.report_id ||
        !same_source(a.original_observation.source_ref,
                     b.original_observation.source_ref) ||
        a.original_observation.source_schema !=
            b.original_observation.source_schema ||
        a.source_key != b.source_key || a.settlement_id != b.settlement_id ||
        a.place_id != b.place_id)
      return fail<const PublicSupplyInspection *>(
          "PublicSupplyInspectionOriginalPrefixChanged", object,
          ErrorKind::internal);
    auto same = compare_world_moments(a.inspected_at, b.inspected_at);
    if (!same.ok)
      return {false, {}, same.error};
    if (same.value)
      return fail<const PublicSupplyInspection *>(
          "PublicSupplyInspectionOriginalPrefixChanged", object,
          ErrorKind::internal);
  }
  const auto &row = applied.back();
  const auto &source = row.second;
  const auto *person = after.common.actor(actor);
  const auto *location =
      person ? std::get_if<SettlementLocation>(&person->location) : nullptr;
  if (row.first != source.id || source.id.empty() ||
      after.state.results.order().back() != source.id ||
      before.results.find(source.id) || source.actor_id != actor ||
      source.prop_id != object || !person || !person->alive ||
      !person->available || !location ||
      location->settlement_id != source.settlement_id ||
      location->place_id != source.place_id)
    return fail<const PublicSupplyInspection *>(
        "PublicSupplyInspectionActualCommandMismatch", object,
        ErrorKind::internal);
  auto now = after.clock.read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  const WorldMoment *times[] = {&source.inspected_at, &now.value};
  for (const auto *at : times) {
    auto same = compare_world_moments(*at, actual);
    if (!same.ok)
      return {false, {}, same.error};
    if (same.value)
      return fail<const PublicSupplyInspection *>(
          "PublicSupplyInspectionActualMomentMismatch", source.id,
          ErrorKind::internal);
  }
  auto valid = validate_public_supply_inspection_sources(after);
  if (!valid.ok)
    return {false, {}, valid.error};
  if (after.state.results.find(source.id) != &source)
    return fail<const PublicSupplyInspection *>(
        "PublicSupplyInspectionActualSourceBorrowRequired", source.id,
        ErrorKind::internal);
  return {true, &source, {}};
}
Status validate_public_supply_inspection_knowledge_source(
    const CommonStore &common, const Report &report,
    const KnownReport &knowledge, const SocialSourceTimeBinding &binding,
    const ExactResultTimeRead &results, const ExactResultReadStamp &stamp) {
  const auto &source = binding.actual_source_ref;
  const auto *time = std::get_if<SocialExactSource>(&binding.time_source);
  const auto *key = std::get_if<SocialKnowledgeKey>(&binding.key);
  if (!is_public_supply_inspection_source(source) || !time ||
      time->original_timed_source_ref.source_schema != 1 ||
      !same_source(time->original_timed_source_ref.source_ref, source) ||
      !key || key->report_id != report.id || knowledge.report_id != report.id ||
      !same_social_subject(key->receiver, knowledge.receiver) ||
      knowledge.receiver.kind != SubjectKind::actor ||
      knowledge.source_message_id ||
      !same_social_subject(knowledge.receiver, knowledge.source_subject))
    return common_failure("InvalidPublicSupplyKnowledgeSource", report.id,
                          ErrorKind::internal);
  auto read = results.fact_v2_borrowed(source.id, stamp);
  if (!read.ok)
    return {false, {}, read.error};
  const auto *f = read.value;
  const auto *owner =
      f ? std::get_if<OwnerFactMoment>(&f->time_basis) : nullptr;
  if (!f || f->kind != public_supply_inspection_source_kind ||
      f->rule_version != "public_supply_inspection_v1" || !owner ||
      owner->source_schema != 1 || !same_source(owner->source, source) ||
      (f->cause_fact_ids.size() != 1 ||
       f->cause_fact_ids.front() != report.fact_id) ||
      f->participants.size() != 1 ||
      !same_social_subject(f->participants.front().subject,
                           knowledge.receiver) ||
      f->participants.front().role != "reader" ||
      f->occurred_at.half_day_tick != knowledge.learned_tick ||
      f->publicly_visible || f->supply || f->obligation)
    return common_failure("PublicSupplyKnowledgeOriginalMismatch", report.id,
                          ErrorKind::internal);
  auto reportable = results.reportable_fact(source.id, stamp);
  if (!reportable.ok)
    return {false, {}, reportable.error};
  if (reportable.value)
    return common_failure("PublicSupplyInspectionCannotCreateReport", source.id,
                          ErrorKind::internal);
  auto valid = validate_social_knowledge_cut_source(common, binding, source);
  if (!valid.ok)
    return valid;
  return common_success();
}

} // namespace hanmo::resident
