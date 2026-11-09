// 精确成年只记录实际原生日边界；不重建儿童，不补历史，不借成熟授予其他权利。
#include "child_maturation_exact.hpp"
#include "resident/kernel/world_bundle.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <string_view>

namespace hanmo::resident {
namespace {

// 本叶错误保留原原因；写入失败交原 World 候选整体撤回。
struct MaturationFailure {
  RuleError error;
};

// 来源损坏与未选择能力分开，不能用空结果掩盖缺失 owner。
[[noreturn]] void fail(const char *code, const StableId &id = {},
                       ErrorKind kind = ErrorKind::internal) {
  throw MaturationFailure{common_failure(code, id, kind).error};
}

// 传播下游原来源的完整错误，不重写成一般未到期。
template <class T> T take(Outcome<T> result) {
  if (!result.ok)
    throw MaturationFailure{std::move(result.error)};
  return std::move(result.value);
}

// 原 Status 检查和 typed Outcome 采取相同失败边界。
void check(Status result) {
  if (!result.ok)
    throw MaturationFailure{std::move(result.error)};
}

// 时刻必须规范且准确相同，不允许以 floor tick 代替分数时刻。
bool same(const WorldMoment &first, const WorldMoment &second) {
  return take(compare_world_moments(first, second)) == 0;
}

// 成熟沿原 FamilyBirth 分配器，不建立第二业务事实域。
SourceRef source(const StableId &id) {
  return {"family_birth", "family_child_matured", id};
}

// 原 Common 分配序号的严格形状；手写同名头不能冒充真实分配。
bool allocated(const CommonStore &common, const StableId &id) {
  constexpr std::string_view prefix = "family_birth:";
  if (!std::string_view(id).starts_with(prefix))
    return false;
  const auto digits = std::string_view(id).substr(prefix.size());
  Integer serial = 0;
  const auto parsed =
      std::from_chars(digits.data(), digits.data() + digits.size(), serial);
  return parsed.ec == std::errc{} &&
         parsed.ptr == digits.data() + digits.size() && serial > 0 &&
         serial < common.state().next_instance_id &&
         digits == std::to_string(serial);
}

// 当前世界、原 FamilyBirth 选择与两个时钟必须来自同一借用链。
WorldMoment selected(const ChildMaturationExactSources &sources) {
  const auto &birth = sources.births;
  if (!birth.catalog.enabled || birth.catalog.selected_version != 4 ||
      !birth.access.people || !birth.access.people->bound_to(birth.common) ||
      !birth.access.life ||
      !birth.access.people->bound_to(birth.common, *birth.access.life))
    fail("ChildMaturationBirth4SourcesRequired", {},
         ErrorKind::unsupported_capability);
  const auto now = take(sources.clock.read_current_moment());
  if (!same(now, take(birth.access.people->read_current_moment())) ||
      now.half_day_tick < birth.common.tick())
    fail("ChildMaturationOriginalClockMismatch");
  if (sources.calendar.profile_ref().catalog_digest !=
      birth.common.state().metadata.content_digest)
    fail("ChildMaturationCalendarCatalogMismatch");
  take(sources.calendar.year_ordinal(now.half_day_tick));
  return now;
}

// 历史先后只采用原 Common append 顺序，ID 字典序与当前表顺序都不作证。
void before(const CommonStore &common, const StableId &earlier,
            const StableId &later) {
  const auto &order = common.state().facts.order();
  const auto first = std::find(order.begin(), order.end(), earlier);
  const auto last = std::find(order.begin(), order.end(), later);
  if (first == order.end() || last == order.end() || first >= last ||
      std::count(order.begin(), order.end(), earlier) != 1 ||
      std::count(order.begin(), order.end(), later) != 1)
    fail("ChildMaturationOriginalSourceOrderMismatch", later);
}

// 保留原成年唯一键；缺失或重复头在来源读口处拒绝。
const StableId *existing_for(const FamilyBirthState &births,
                             const StableId &actor) {
  const StableId *result = nullptr;
  for (const auto &row : births.maturations.rows()) {
    if (row.second.actor_id != actor)
      continue;
    if (result)
      fail("DuplicateChildMaturation", actor);
    result = &row.first;
  }
  return result;
}

// 只由实际 Life4 终点判断生日之前是否已死亡；今日死亡不能遮住更早漏掉的成年门。
// 同 M 的原死亡阶段先于 guardianship，本候选不为该终点再生成成熟事实。
bool died_by(const ChildMaturationExactSources &sources, const StableId &actor,
             const WorldMoment &boundary) {
  const auto death =
      take(sources.births.access.people->life().read_actor_death(actor));
  if (!death) {
    if (!take(sources.births.access.people->read_current_presence(actor)).alive)
      fail("ChildMaturationActualLifeSourceRequired", actor);
    return false;
  }
  if (death->effective_at.kind != ActorFoundationAt::Kind::exact_moment)
    fail("ChildMaturationActualLifeMomentRequired", actor);
  return take(compare_world_moments(death->effective_at.exact_moment,
                                    boundary)) <= 0;
}

} // namespace

Outcome<WorldMoment>
original_child_maturation_boundary(const OriginalCalendarRead &calendar,
                                   Integer birth_tick) {
  try {
    // 720 半日及 18 岁取自已固定原历法、成年规则；不是作者可调的新参数。
    constexpr Integer maturity_offset = 18 * 720;
    if (birth_tick < 0 ||
        birth_tick > std::numeric_limits<Integer>::max() - maturity_offset)
      fail("ChildMaturationBirthTickOutOfRange");
    const WorldMoment due{birth_tick + maturity_offset, {}};
    if (take(calendar.exact_birth_age_at(birth_tick, due)) != 18 ||
        take(calendar.exact_birth_age_at(
            birth_tick, WorldMoment{due.half_day_tick - 1, {}})) != 17)
      fail("ChildMaturationOriginalCalendarMismatch");
    return {true, due, {}};
  } catch (MaturationFailure &error) {
    return {false, {}, std::move(error.error)};
  }
}

Outcome<ChildMaturationExactView>
read_child_maturation_exact_source(const ChildMaturationExactSources &sources,
                                   const SourceRef &ref) {
  try {
    const auto now = selected(sources);
    const auto &birth = sources.births;
    const auto &common = birth.common;
    const auto *record = birth.births.maturations.find(ref.id);
    const auto *header = common.state().facts.find(ref.id);
    if (ref.domain != "family_birth" || ref.kind != "family_child_matured" ||
        !allocated(common, ref.id) || !record || !header ||
        header->id != ref.id || header->domain != ref.domain ||
        header->kind != ref.kind || record->kind != ref.kind ||
        record->availability_enabled)
      fail("ChildMaturationExactSourceRequired", ref.id);
    const auto original =
        take(read_born_character_source(birth, {record->birth_fact_id}));
    if (!original.birth || original.birth->actor_id != record->actor_id ||
        !original.original_fact ||
        record->revision < original.birth->revision ||
        record->revision > common.revision())
      fail("ChildMaturationOriginalBirthMismatch", ref.id);
    const auto due = take(original_child_maturation_boundary(
        sources.calendar, original.birth->tick));
    if (record->tick != due.half_day_tick ||
        take(compare_world_moments(due, now)) > 0)
      fail("ChildMaturationExactBoundaryMismatch", ref.id);
    before(common, record->birth_fact_id, ref.id);
    before(common, record->actor_id, ref.id);
    const auto *existing = existing_for(birth.births, record->actor_id);
    if (!existing || *existing != ref.id)
      fail("ChildMaturationOriginalOnceMismatch", ref.id);
    const auto age = take(birth.access.people->read_age_before_original_fact(
        record->actor_id, record->tick, ref));
    const auto *age_birth = std::get_if<BirthFactRef>(&age.ref);
    if (!age_birth || age_birth->birth_fact_id != record->birth_fact_id ||
        age.age_years != 18)
      fail("ChildMaturationOriginalAgeMismatch", ref.id);
    check(birth.access.people->life().require_no_death_before_original_fact(
        record->actor_id, record->tick, ref));
    return {true, {record, original.birth, header, due}, {}};
  } catch (MaturationFailure &error) {
    return {false, {}, std::move(error.error)};
  }
}

Status validate_child_maturation_exact_sources(
    const ChildMaturationExactSources &sources) {
  try {
    selected(sources);
    for (const auto &row : sources.births.births.maturations.rows())
      take(read_child_maturation_exact_source(sources, source(row.first)));
    for (const auto &row : sources.births.common.state().facts.rows()) {
      if (row.second.domain == "family_birth" &&
          row.second.kind == "family_child_matured" &&
          !sources.births.births.maturations.find(row.first))
        fail("ChildMaturationPayloadMissing", row.first);
    }
    return common_success();
  } catch (MaturationFailure &error) {
    return {false, {}, std::move(error.error)};
  }
}

Status validate_child_maturation_completed_through(
    const ChildMaturationExactSources &sources) {
  try {
    const auto now = selected(sources);
    check(validate_child_maturation_exact_sources(sources));
    for (const auto &entry : sources.births.births.births.rows()) {
      const auto original =
          take(read_born_character_source(sources.births, {entry.first}));
      const auto due = take(original_child_maturation_boundary(
          sources.calendar, original.birth->tick));
      if (take(compare_world_moments(due, now)) > 0 ||
          existing_for(sources.births.births, original.birth->actor_id) ||
          died_by(sources, original.birth->actor_id, due))
        continue;
      fail("UnresolvedChildMaturationAtStableBoundary", entry.first);
    }
    return common_success();
  } catch (MaturationFailure &error) {
    return {false, {}, std::move(error.error)};
  }
}

Outcome<std::optional<WorldMoment>>
next_child_maturation_boundary(const ChildMaturationExactSources &sources) {
  try {
    const auto now = selected(sources);
    check(validate_child_maturation_exact_sources(sources));
    std::optional<WorldMoment> next;
    for (const auto &id : sources.births.common.state().facts.order()) {
      const auto *birth = sources.births.births.births.find(id);
      if (!birth)
        continue;
      const auto original =
          take(read_born_character_source(sources.births, {id}));
      if (existing_for(sources.births.births, birth->actor_id))
        continue;
      const auto due = take(original_child_maturation_boundary(
          sources.calendar, original.birth->tick));
      if (died_by(sources, original.birth->actor_id, due))
        continue;
      if (take(compare_world_moments(due, now)) < 0)
        fail("MissedChildMaturationCalendarBoundary", id);
      if (!next || take(compare_world_moments(due, *next)) < 0)
        next = due;
    }
    return {true, next, {}};
  } catch (MaturationFailure &error) {
    return {false, {}, std::move(error.error)};
  }
}

Outcome<SourceRef> record_child_maturation_at_calendar_in_candidate(
    CommonStore &common, FamilyBirthState &births,
    const ChildMaturationExactSources &sources, const BirthFactRef &ref,
    const ExactPeriodContext &period, const ReadStamp &stamp,
    const ReadStampAuthority &stamps, const DomainReadVersionSource &versions) {
  try {
    if (&common != &sources.births.common || &births != &sources.births.births)
      fail("ChildMaturationSameCandidateRequired");
    const auto now = selected(sources);
    check(validate_child_maturation_exact_sources(sources));
    const auto original = take(read_born_character_source(sources.births, ref));
    const auto due = take(original_child_maturation_boundary(
        sources.calendar, original.birth->tick));
    if (const auto *prior = existing_for(births, original.birth->actor_id)) {
      const auto result = source(*prior);
      const auto actual =
          take(read_child_maturation_exact_source(sources, result));
      if (actual.maturation->birth_fact_id != ref.birth_fact_id)
        fail("ChildMaturationOriginalBirthMismatch", *prior);
      return {true, result, {}};
    }
    if (period.period_id.empty() || period.opening_at.half_day_tick < 0 ||
        period.opening_at.half_day_tick ==
            std::numeric_limits<Integer>::max() ||
        period.through_tick != period.opening_at.half_day_tick + 1 ||
        !same(period.opening_at,
              WorldMoment{period.opening_at.half_day_tick, {}}) ||
        !same(now, WorldMoment{period.through_tick, {}}) || !same(now, due))
      fail("ChildMaturationActualCalendarBoundaryRequired", ref.birth_fact_id);
    check(stamps.validate(stamp, common.state().identity,
                          ReadMode::phase_current, common, versions));
    bool actor_read = false;
    for (const auto &row : stamp.resources()) {
      if (row.resource.kind == ResourceKey::Kind::actor &&
          row.resource.id == original.birth->actor_id)
        actor_read = true;
    }
    if (stamp.phase() != "guardianship_monitor" ||
        !same(stamp.span().to, now) || !actor_read ||
        std::find(stamp.source_ids().begin(), stamp.source_ids().end(),
                  ref.birth_fact_id) == stamp.source_ids().end())
      fail("ChildMaturationActualReadStampRequired", ref.birth_fact_id);
    const auto presence =
        take(sources.births.access.people->read_current_presence(
            original.birth->actor_id));
    if (!presence.alive)
      fail("ChildMaturationLivingSubjectRequired", original.birth->actor_id,
           ErrorKind::rule);
    const auto id = take(common.next_id("family_birth"));
    ChildMaturationRecord record;
    record.actor_id = original.birth->actor_id;
    record.birth_fact_id = ref.birth_fact_id;
    record.tick = now.half_day_tick;
    record.revision = common.revision();
    if (!births.maturations.insert(id, std::move(record)))
      fail("DuplicateChildMaturation", id);
    check(common.append_fact({id, "family_child_matured", "family_birth"}));
    const auto result = source(id);
    take(read_child_maturation_exact_source(sources, result));
    return {true, result, {}};
  } catch (MaturationFailure &error) {
    return {false, {}, std::move(error.error)};
  }
}

Status apply_due_child_maturations_in_candidate(
    WorldBundle &world, const ChildMaturationExactSources &sources,
    const CurrentReadStampRead &current, const ReadStampAuthority &stamps,
    const DomainReadVersionSource &versions, const ExactPeriodContext &period,
    ExactPhaseWriteContext &context) {
  try {
    const auto at = selected(sources);
    if (period.period_id.empty() || period.opening_at.half_day_tick < 0 ||
        period.opening_at.half_day_tick ==
            std::numeric_limits<Integer>::max() ||
        period.through_tick != period.opening_at.half_day_tick + 1 ||
        !same(period.opening_at,
              WorldMoment{period.opening_at.half_day_tick, {}}) ||
        &world.common != &sources.births.common ||
        &world.family_birth != &sources.births.births ||
        world.read_stamps.get() != &stamps ||
        world.active_exact_write_context() != &context ||
        !world.common.actor_condition_capture_identity() ||
        !same(at, take(world.actual_phase_moment())) ||
        !same(at, take(current.read_current_moment())) ||
        !same(at, WorldMoment{period.through_tick, {}}))
      fail("ChildMaturationOriginalWorldCandidateRequired");
    check(validate_child_maturation_exact_sources(sources));
    // 本轮只借进入时已经存在的事实。新成熟头可以追加，不能扰动出生顺序。
    const auto original_count = world.common.state().facts.order().size();
    for (std::size_t index = 0; index < original_count; ++index) {
      const auto &id = world.common.state().facts.order()[index];
      const auto *born = world.family_birth.births.find(id);
      if (!born || existing_for(world.family_birth, born->actor_id))
        continue;
      const auto actual =
          take(read_born_character_source(sources.births, {id}));
      const auto due = take(original_child_maturation_boundary(
          sources.calendar, actual.birth->tick));
      if (died_by(sources, actual.birth->actor_id, due))
        continue;
      const auto comparison = take(compare_world_moments(due, at));
      if (comparison < 0)
        fail("MissedChildMaturationCalendarBoundary", id);
      if (comparison > 0)
        continue;
      // 先持值原 ID，再写 Common；不得在追加后使用可能失效的 fact-order 借用。
      const BirthFactRef birth_ref{id};
      const StableId actor_id = actual.birth->actor_id;
      auto stamp = take(
          current.capture({ResourceKey{ResourceKey::Kind::actor, actor_id}}, {},
                          {birth_ref.birth_fact_id}));
      if (!stamp)
        fail("ChildMaturationActualReadStampRequired", birth_ref.birth_fact_id);
      const auto applied = record_child_maturation_at_calendar_in_candidate(
          world.common, world.family_birth, sources, birth_ref, period, *stamp,
          stamps, versions);
      world.note_write_boundary();
      take(applied);
    }
    check(validate_child_maturation_completed_through(sources));
    return common_success();
  } catch (MaturationFailure &error) {
    return {false, {}, std::move(error.error)};
  }
}

} // namespace hanmo::resident
