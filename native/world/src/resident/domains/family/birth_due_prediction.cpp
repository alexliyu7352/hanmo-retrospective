// 到期来自原接受的绝对日历边界；预测不是成果，不伪造 source revision。
#include "birth_due_prediction.hpp"

namespace hanmo::resident {
namespace {
// 将原专业来源错误原样交给外层，不把技术缺源变成“没有到期”。
struct BirthPredictionFailure {
  RuleError error;
};

// 坏来源与不支持的选型明确失败，拒绝仅声明一个空 provider。
[[noreturn]] void fail(const char *code, const StableId &id = {},
                       ErrorKind kind = ErrorKind::internal) {
  throw BirthPredictionFailure{common_failure(code, id, kind).error};
}

// 只读取当前真实源的 Outcome，不能用默认值代替原 validator 拒绝。
template <class T> T take(Outcome<T> result) {
  if (!result.ok)
    throw BirthPredictionFailure{std::move(result.error)};
  return std::move(result.value);
}

// 完整 SourceRef 身份比较，不只比较可能跨域相同的 ID。
bool same(const SourceRef &first, const SourceRef &second) {
  return first.domain == second.domain && first.kind == second.kind &&
         first.id == second.id;
}

// 本叶仅接原 Birth4 和同一候选的真实 People/Life/Lineage/目录绑定。
// 下游整链仍由原 validate_birth_arrangements 验证，不在这里放宽旧来源门。
void selected(const BornCharacterSourceContext &sources) {
  if (!sources.catalog.enabled || sources.catalog.selected_version != 4 ||
      !sources.lineage.enabled ||
      sources.interactions.common.get() != &sources.common_catalog ||
      sources.access.lineage != &sources.lineage || !sources.access.people ||
      !sources.access.life ||
      !sources.access.people->bound_to(sources.common, *sources.access.life))
    fail("BirthPredictionOriginal4SourcesRequired", {},
         ErrorKind::unsupported_capability);
}

// 只持一条最早真实预定的临时值；原表/终态拥有权始终不变。
std::optional<ScheduledBirthSource>
first_due(const BornCharacterSourceContext &sources, const WorldMoment &current,
          const WorldMoment &permit_to) {
  selected(sources);
  if (take(compare_world_moments(
          take(sources.access.people->read_current_moment()), current)) != 0)
    fail("BirthPredictionActualClockMismatch");
  if (take(compare_world_moments(permit_to, current)) < 0)
    fail("BirthPredictionInvalidWindow");
  const auto history = take(validate_birth_arrangements(
      sources.common, sources.interactions, sources.family_catalog,
      sources.catalog, sources.births, sources.access));
  std::optional<ScheduledBirthSource> earliest;
  // validator 已核全部 accepted 与反向覆盖；遍历真正 Common 源顺序，不按
  // map 排序或新的请求顺序改变同刻出生的原抽样次序。
  for (const auto &id : sources.common.state().facts.order()) {
    const auto *accepted = sources.births.replies.find(id);
    if (!accepted || accepted->decision != "accepted" ||
        history.closed_acceptances.count(id) ||
        history.born_acceptances.count(id))
      continue;
    auto scheduled = take(read_scheduled_birth_source(
        sources.common, sources.interactions, sources.family_catalog,
        sources.catalog, sources.births, sources.access, id));
    if (take(compare_world_moments(scheduled.occurrence, current)) < 0)
      fail("MissedBirthDueBoundary", id);
    if (take(compare_world_moments(scheduled.occurrence, permit_to)) > 0)
      continue;
    if (!earliest || take(compare_world_moments(scheduled.occurrence,
                                                earliest->occurrence)) < 0)
      earliest = std::move(scheduled);
  }
  return earliest;
}
} // namespace

// 验证过的空结果仅表示此窗口确实没有未终结安排，绝不意味着完整运行就绪。
Outcome<std::vector<ExactEventPrediction>>
predict_original_birth_due(const BornCharacterSourceContext &sources,
                           const WorldMoment &current,
                           const WorldMoment &permit_to) {
  try {
    auto scheduled = first_due(sources, current, permit_to);
    std::vector<ExactEventPrediction> result;
    if (scheduled)
      result.push_back({scheduled->occurrence, ExactStage::birth_due,
                        std::move(scheduled->source_ref),
                        sources.catalog.selected_version,
                        sources.common.revision()});
    return {true, std::move(result), {}};
  } catch (BirthPredictionFailure &error) {
    return {false, {}, std::move(error.error)};
  }
}

// 同刻前驱可能刚产生真实 Death4；这里仅重新选安排，不缓存孕育者 alive。
// 后续采用必须再由原真实死因/出生 writer 分派，合法出生不因资源短缺重抽。
Outcome<ScheduledBirthSource> read_original_birth_due_prediction_source(
    const BornCharacterSourceContext &sources,
    const ExactEventPrediction &prediction) {
  try {
    selected(sources);
    const auto now = take(sources.access.people->read_current_moment());
    if (prediction.stage != ExactStage::birth_due ||
        prediction.source_schema != sources.catalog.selected_version ||
        prediction.dependency_revision != sources.common.revision() ||
        take(compare_world_moments(prediction.at, now)) != 0)
      fail("BirthDuePredictionMismatch", prediction.actual_source_basis.id);
    auto scheduled = first_due(sources, now, now);
    if (!scheduled ||
        !same(scheduled->source_ref, prediction.actual_source_basis) ||
        take(compare_world_moments(scheduled->occurrence, prediction.at)) != 0)
      fail("BirthDuePredictionMismatch", prediction.actual_source_basis.id);
    return {true, std::move(*scheduled), {}};
  } catch (BirthPredictionFailure &error) {
    return {false, {}, std::move(error.error)};
  }
}
} // namespace hanmo::resident
