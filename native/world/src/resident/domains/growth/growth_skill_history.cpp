// 来源只沿原StudyFact/逐期净增益，受限未知保持明确；无写操作、无新事实或原课程重算。
#include "growth_skill_history.hpp"
#include "growth_internal.hpp"
#include "growth_skill_origin.hpp"
#include "growth_tactic_lesson_qualification.hpp"
#include "growth_military_training_qualification.hpp"
#include <algorithm>
#include <charconv>
#include <set>
namespace hanmo::resident {
namespace {
// 原开店与新战法报名分别限定真实业务头；不开放任意事实或未来时刻的通用资格读取。
enum class QualificationBoundary {
  private_shop,
  tactic_lesson,
  military_training
};
// 实际实例号与真实头成对，数值只核分配身份；事实先后始终读Common原序。
bool allocated(const CommonStore &common, const StableId &id,
               std::string_view prefix) {
  if (id.size() <= prefix.size() + 1 ||
      std::string_view(id).substr(0, prefix.size()) != prefix ||
      id[prefix.size()] != ':')
    return false;
  const auto digits = std::string_view(id).substr(prefix.size() + 1);
  Integer value = 0;
  const auto parse =
      std::from_chars(digits.data(), digits.data() + digits.size(), value);
  return parse.ec == std::errc{} &&
         parse.ptr == digits.data() + digits.size() && value > 0 &&
         value < common.state().next_instance_id && digits.front() != '0';
}
struct QualificationView {
  WorldIdentity source_identity;
  std::string_view actor_id, skill_id,
      source_kind = "effective_initial_and_permanent_study", boundary_fact_id;
  Integer observed_tick = 0, initial_xp = 0, minimum_xp = 0, minimum_level = 0;
  CheckpointVector<std::string_view> study_source_ids;
  std::string_view origin_kind = "effective_authored_actor",
                   initial_prototype_id;
  Integer initial_created_tick = 0;
  CheckpointVector<std::string_view> initial_source_ids;
};
// 后来完成的同一课程仍可读取早期已入账分段；未结旧末付课不冒成已获得XP。
Outcome<QualificationView> read_view(
    const CommonStore &common, const CommonCatalog &catalog,
    const FamilyCatalog &family, const GrowthState &growth_state,
    const StableId &actor, Integer tick, const StableId &boundary,
    const GrowthSkillOriginContext *identities = nullptr,
    const StableId &skill = "commerce",
    QualificationBoundary business = QualificationBoundary::private_shop) {
  auto fail = [&](const StableId &code) -> Outcome<QualificationView> {
    return {false, {}, common_failure(code, actor).error};
  };
  if (tick < 0 || tick > common.tick())
    return fail("GrowthSkillHistoryBoundaryUnavailable");
  const auto &order = common.state().facts.order();
  auto boundary_at = order.end();
  if (!boundary.empty()) {
    const auto *head = common.state().facts.find(boundary);
    boundary_at = std::find(order.begin(), order.end(), boundary);
    const bool exact =
        head && (business == QualificationBoundary::private_shop
                     ? allocated(common, boundary, "private_shop") &&
                           head->kind == "private_shop_opened" &&
                           head->domain == "private_shops"
                 : business == QualificationBoundary::tactic_lesson
                     ? allocated(common, boundary, "tactic_lesson") &&
                           head->kind == "tactic_lesson_started" &&
                           head->domain == "growth_tactic_lessons"
                     : allocated(common, boundary, "action") &&
                           head->kind == "military_training" &&
                           head->domain == "military");
    if (!exact || head->id != boundary || boundary_at == order.end())
      return fail("GrowthSkillHistoryBoundaryUnavailable");
  }
  const auto *current = common.state().actors.find(actor);
  if (!current)
    return fail("GrowthSkillOriginUnavailable");
  GrowthSkillOriginBorrowed origin;
  if (identities) {
    auto source = boundary.empty()
                      ? read_growth_skill_origin_borrowed(common, *identities,
                                                          actor, skill)
                      : read_growth_skill_origin_before_fact_borrowed(
                            common, *identities, actor, skill, tick, boundary);
    if (!source.ok)
      return {false, {}, source.error};
    origin = std::move(source.value);
  } else {
    const auto *initial = family.prototypes.find(actor);
    if (!initial || !catalog.actor_initial_birth.find(actor) ||
        std::find(catalog.actor_ids.begin(), catalog.actor_ids.end(), actor) ==
            catalog.actor_ids.end() ||
        (current->prototype_id && *current->prototype_id != actor) ||
        (current->created_tick && *current->created_tick != 0))
      return fail("GrowthSkillOriginUnavailable");
    origin.initial_xp = growth_skill_xp(initial->skills, skill);
    origin.origin_kind = "effective_authored_actor";
    origin.prototype_actor_id = actor;
  }
  const auto original = origin.initial_xp,
             today = growth_skill_xp(current->skills, skill);
  if (original < 0 || original > 1200 || today < original || today > 1200)
    return fail("GrowthSkillMonotonicityUnavailable");
  QualificationView result;
  result.source_identity = common.state().identity;
  result.actor_id = actor;
  result.skill_id = skill;
  result.boundary_fact_id = boundary;
  result.observed_tick = tick;
  result.initial_xp = original;
  result.minimum_xp = original;
  result.origin_kind = origin.origin_kind;
  result.initial_prototype_id = origin.prototype_actor_id;
  result.initial_created_tick = origin.created_tick;
  result.initial_source_ids = origin.source_fact_ids;
  auto last_origin = order.end();
  if (!origin.source_fact_ids.empty())
    last_origin =
        std::find(order.begin(), order.end(), origin.source_fact_ids.back());
  Integer all_known = original;
  std::set<std::string_view, std::less<>, CheckpointAllocator<std::string_view>>
      seen;
  for (const auto &row : growth_state.facts.rows()) {
    const auto *study = std::get_if<StudyFact>(&row.second);
    if (!study || study->terms.student_id != actor ||
        study->terms.skill_id != skill)
      continue;
    const auto *head = common.state().facts.find(row.first);
    auto at = std::find(order.begin(), order.end(), row.first);
    if (!seen.insert(row.first).second ||
        !allocated(common, row.first, "action") || !head ||
        head->id != row.first || head->kind != "skill_study" ||
        head->domain != "cultivation" || at == order.end() ||
        study->actor_id != actor || study->start_tick < origin.created_tick ||
        study->start_tick > common.tick() ||
        (study->phase != "started" && study->phase != "completed"))
      return fail("GrowthSkillStudySourceInvalid");
    if (last_origin != order.end() && at <= last_origin)
      return fail("GrowthSkillStudyPredatesIdentity");
    const bool before = boundary.empty() || at < boundary_at;
    Integer counted = 0;
    auto contribution = [&](Integer settled, Integer gained) -> Status {
      if (settled < study->start_tick || settled > common.tick() ||
          gained < 0 || gained > 1200 || all_known > 1200 - gained)
        return common_failure("GrowthSkillContributionInvalid", row.first);
      all_known += gained;
      if (settled < tick) {
        if (!before)
          return common_failure("GrowthSkillHistoryOrderUnavailable",
                                row.first);
        if (result.minimum_xp > 1200 - gained)
          return common_failure("GrowthSkillContributionInvalid", row.first);
        result.minimum_xp += gained;
        counted += gained;
      }
      return common_success();
    };
    if (growth::study_segmented(*study)) {
      for (const auto &lesson : study->lessons) {
        if (!lesson.segment)
          return fail("GrowthSkillStudySourceInvalid");
        const auto &segment = *lesson.segment;
        auto valid = contribution(segment.settled_tick, segment.xp_gained);
        if (!valid.ok)
          return {false, {}, valid.error};
        if (segment.adjustment) {
          valid = contribution(segment.adjustment->settled_tick,
                               segment.adjustment->xp_gained);
          if (!valid.ok)
            return {false, {}, valid.error};
        }
      }
    } else if (study->phase == "completed") {
      // 原末次结算函数同时处理取消已有课时、表现及上限；不复制本域XP数学。
      auto valid =
          contribution(study->completed_tick, growth::study_xp_gained(*study));
      if (!valid.ok)
        return {false, {}, valid.error};
    }
    if (counted)
      result.study_source_ids.push_back(row.first);
  }
  if (all_known > today || result.minimum_xp > today)
    return fail("GrowthSkillMonotonicityUnavailable");
  result.minimum_level = growth_skill_level(result.minimum_xp);
  return {true, std::move(result), {}};
}
GrowthSkillQualification legacy_copy(const QualificationView &v) {
  GrowthSkillQualification r;
  r.source_identity = v.source_identity;
  r.actor_id = v.actor_id;
  r.skill_id = v.skill_id;
  r.source_kind = v.source_kind;
  r.boundary_fact_id = v.boundary_fact_id;
  r.observed_tick = v.observed_tick;
  r.initial_xp = v.initial_xp;
  r.minimum_xp = v.minimum_xp;
  r.minimum_level = v.minimum_level;
  for (auto id : v.study_source_ids)
    r.study_source_ids.emplace_back(id);
  r.origin_kind = v.origin_kind;
  r.initial_prototype_id = v.initial_prototype_id;
  r.initial_created_tick = v.initial_created_tick;
  for (auto id : v.initial_source_ids)
    r.initial_source_ids.emplace_back(id);
  return r;
}
Outcome<GrowthSkillQualification>
read(const CommonStore &c, const CommonCatalog &catalog,
     const FamilyCatalog &family, const GrowthState &s, const StableId &a,
     Integer tick, const StableId &fact,
     const GrowthSkillOriginContext *x = nullptr,
     const StableId &skill = "commerce",
     QualificationBoundary business = QualificationBoundary::private_shop) {
  auto result =
      read_view(c, catalog, family, s, a, tick, fact, x, skill, business);
  if (!result.ok)
    return {false, {}, std::move(result.error)};
  return {true, legacy_copy(result.value), {}};
}
OwnedGrowthSkillQualification own(const QualificationView &v) {


  auto memory = CheckpointMemoryCharge(0);
  GrowthSkillQualification out{
      v.source_identity,
      checkpoint_fresh_text_copy(v.actor_id),
      checkpoint_fresh_text_copy(v.skill_id),
      checkpoint_fresh_text_copy(v.source_kind),
      checkpoint_fresh_text_copy(v.boundary_fact_id),
      v.observed_tick,
      v.initial_xp,
      v.minimum_xp,
      v.minimum_level,
      {},
      checkpoint_fresh_text_copy(v.origin_kind),
      checkpoint_fresh_text_copy(v.initial_prototype_id),
      v.initial_created_tick,
      {}};
  out.study_source_ids.reserve(v.study_source_ids.size());
  out.initial_source_ids.reserve(v.initial_source_ids.size());
  for (auto id : v.study_source_ids)
    out.study_source_ids.push_back(checkpoint_fresh_text_copy(id));
  for (auto id : v.initial_source_ids)
    out.initial_source_ids.push_back(checkpoint_fresh_text_copy(id));
  return OwnedGrowthSkillQualification(std::move(memory), std::move(out));
}
Outcome<OwnedGrowthSkillQualification>
read_owned(const CommonStore &c, const CommonCatalog &catalog,
           const FamilyCatalog &family, const GrowthState &s, const StableId &a,
           Integer tick, const StableId &fact,
           const GrowthSkillOriginContext *x = nullptr) {
  try {
    const StableId skill = "commerce"; // 借视图完成拥有化前保持此SSO输入活着。
    auto result = read_view(c, catalog, family, s, a, tick, fact, x, skill);
    if (!result.ok)
      return {false, {}, std::move(result.error)};
    return {true, own(result.value), {}};
  } catch (const std::bad_alloc &) {
    return {false,
            {},
            common_failure("GrowthSkillQualificationMemoryExhausted", a,
                           ErrorKind::internal)
                .error};
  }
}
} // namespace
Outcome<GrowthSkillQualification> read_commerce_study_qualification(
    const CommonStore &c, const CommonCatalog &common,
    const FamilyCatalog &family, const GrowthState &state,
    const StableId &actor) {
  return read(c, common, family, state, actor, c.tick(), {});
}
Outcome<GrowthSkillQualification>
read_commerce_study_qualification_before_opening(
    const CommonStore &c, const CommonCatalog &common,
    const FamilyCatalog &family, const GrowthState &state,
    const StableId &actor, Integer tick, const StableId &fact) {
  if (fact.empty())
    return {false,
            {},
            common_failure("GrowthSkillHistoryBoundaryUnavailable").error};
  return read(c, common, family, state, actor, tick, fact);
}
Outcome<GrowthSkillQualification> read_commerce_study_qualification(
    const CommonStore &c, const GrowthSkillOriginContext &x,
    const GrowthState &state, const StableId &actor) {
  return read(c, x.common, x.family_catalog, state, actor, c.tick(), {}, &x);
}
Outcome<GrowthSkillQualification>
read_commerce_study_qualification_before_opening(
    const CommonStore &c, const GrowthSkillOriginContext &x,
    const GrowthState &state, const StableId &actor, Integer tick,
    const StableId &fact) {
  if (fact.empty())
    return {false,
            {},
            common_failure("GrowthSkillHistoryBoundaryUnavailable").error};
  return read(c, x.common, x.family_catalog, state, actor, tick, fact, &x);
}

namespace {
// 这里只是基础门槛的可证下界；战法掌握、教师许可与实际教学由各自原领域另验。
Outcome<GrowthSkillQualification> tactic_qualification(
    const CommonStore &c, const GrowthSkillOriginContext &x,
    const GrowthState &s, const StableId &actor, const StableId &skill,
    Integer tick, const StableId &fact,
    QualificationBoundary business = QualificationBoundary::tactic_lesson) {
  if (skill != "tactics" && skill != "infantry" && skill != "archery" &&
      skill != "cavalry" && skill != "transport")
    return {false,
            {},
            common_failure("TacticLessonPrerequisiteUnsupported", skill).error};
  auto result = read(c, x.common, x.family_catalog, s, actor, tick, fact, &x,
                     skill, business);
  if (!result.ok)
    return result;
  const auto &order = c.state().facts.order();
  std::sort(result.value.study_source_ids.begin(),
            result.value.study_source_ids.end(),
            [&](const StableId &a, const StableId &b) {
              return std::find(order.begin(), order.end(), a) <
                     std::find(order.begin(), order.end(), b);
            });
  return result;
}
} // namespace
Outcome<GrowthSkillQualification> read_growth_tactic_lesson_qualification(
    const CommonStore &c, const GrowthSkillOriginContext &x,
    const GrowthState &s, const StableId &actor, const StableId &skill) {
  return tactic_qualification(c, x, s, actor, skill, c.tick(), {});
}
Outcome<GrowthSkillQualification>
read_growth_tactic_lesson_qualification_before_start(
    const CommonStore &c, const GrowthSkillOriginContext &x,
    const GrowthState &s, const StableId &actor, const StableId &skill,
    Integer tick, const StableId &fact) {
  if (fact.empty())
    return {
        false, {}, common_failure("TacticLessonStartBoundaryRequired").error};
  return tactic_qualification(c, x, s, actor, skill, tick, fact);
}

Outcome<GrowthSkillQualification>
read_growth_military_training_qualification_before_start(
    const CommonStore &c, const GrowthSkillOriginContext &x,
    const GrowthState &s, const StableId &actor, const StableId &skill,
    Integer tick, const StableId &fact) {
  if (fact.empty())
    return {false,
            {},
            common_failure("MilitaryTrainingStartBoundaryRequired").error};
  return tactic_qualification(c, x, s, actor, skill, tick, fact,
                              QualificationBoundary::military_training);
}

} // namespace hanmo::resident

namespace hanmo::resident {
Outcome<OwnedGrowthSkillQualification> read_commerce_study_qualification_owned(
    const CommonStore &c, const CommonCatalog &catalog,
    const FamilyCatalog &family, const GrowthState &s, const StableId &a) {
  return read_owned(c, catalog, family, s, a, c.tick(), {});
}
Outcome<OwnedGrowthSkillQualification>
read_commerce_study_qualification_before_opening_owned(
    const CommonStore &c, const CommonCatalog &catalog,
    const FamilyCatalog &family, const GrowthState &s, const StableId &a,
    Integer tick, const StableId &fact) {
  if (fact.empty())
    return {false,
            {},
            common_failure("GrowthSkillHistoryBoundaryUnavailable").error};
  return read_owned(c, catalog, family, s, a, tick, fact);
}
Outcome<OwnedGrowthSkillQualification> read_commerce_study_qualification_owned(
    const CommonStore &c, const GrowthSkillOriginContext &x,
    const GrowthState &s, const StableId &a) {
  return read_owned(c, x.common, x.family_catalog, s, a, c.tick(), {}, &x);
}
Outcome<OwnedGrowthSkillQualification>
read_commerce_study_qualification_before_opening_owned(
    const CommonStore &c, const GrowthSkillOriginContext &x,
    const GrowthState &s, const StableId &a, Integer tick,
    const StableId &fact) {
  if (fact.empty())
    return {false,
            {},
            common_failure("GrowthSkillHistoryBoundaryUnavailable").error};
  return read_owned(c, x.common, x.family_catalog, s, a, tick, fact, &x);
}
} // namespace hanmo::resident
