// 寿终桥只读绑定的组件门；不初始化世界、不抽寿终随机数、不冒充 LIFE-5 业务门。
#include "resident/domains/actor_foundation/character_condition_read.hpp"
#include "resident/domains/family/life_age_exact_phase.hpp"
#include "resident/domains/military/military_model.hpp"
#include <iostream>
#include <stdexcept>

using namespace hanmo::resident;
namespace {
// 组件断言保留具名首错；不把任意失败当作预期拒绝。
void require(bool value, const char *name) {
  if (!value)
    throw std::runtime_error(name);
}

// 只供绑定组件观察时钟，不发行 stamp，不执行年龄或死亡来源。
struct BindingClock final : CurrentMomentRead {
  WorldMoment now{720, {}};
  Outcome<WorldMoment> read_current_moment() const override {
    return {true, now, {}};
  }
};

// 独立真实根用于核地址身份；这里没有作者人物、死亡事实或可用业务来源。
struct BindingFixture {
  std::shared_ptr<CommonCatalog> shared = std::make_shared<CommonCatalog>();
  CommonStore common{CommonState{}, shared};
  ActorFoundationCatalog foundation_catalog;
  AuthoredActorFoundationState foundation;
  FamilyState family;
  FamilyBirthState births;
  FamilyCatalog family_catalog;
  FamilyDeathState deaths;
  LifeAgeState age;
  LifeAgeCatalog age_catalog;
  OriginalCalendarRead calendar;
  MilitaryState military;
  MilitaryExactState military_exact;
  BindingClock clock;
  ProfileControlEligibilitySources control{
      common,  family,      family_catalog,      *shared,
      nullptr, &foundation, &foundation_catalog, &clock};
  NativeLifeAgeExactRead original{{common, foundation, foundation_catalog,
                                   family, births, control, calendar},
                                  age_catalog,
                                  age,
                                  deaths,
                                  clock,
                                  foundation_catalog.identity,
                                  military,
                                  &military_exact,
                                  nullptr};

  // 返回原 reader 的同根借用；不将这个值宣称为通过完整来源验证的 body。
  CharacterConditionSources body() const {
    CharacterConditionSources out{common, foundation, foundation_catalog,
                                  clock.now};
    out.deaths = &deaths;
    out.life_sources = &original.bindings().life_sources;
    out.military = &military;
    out.military_exact = &military_exact;
    return out;
  }
};

// 每个拒绝同时核错误码；正例仅表示原根绑定一致，不授予死亡资格。
void rejected(const LifeAgeExactReadBinding &original,
              const CharacterConditionSources *body, const WorldMoment &at,
              const char *code, const char *name) {
  const auto result =
      validate_life_age_death_condition_sources(original, body, at);
  require(!result.ok && result.error.original.code == code, name);
}

// 缺少同候选 body 或实际 Life4 原来源必须硬拒，不能回退空来源。
void missing_original_sources() {
  BindingFixture f;
  const auto original = f.original.bindings();
  rejected(original, nullptr, f.clock.now,
           "LifeAgeActualConditionSourcesRequired", "missing_original_body");
  auto body = f.body();
  body.life_sources = nullptr;
  rejected(original, &body, f.clock.now, "LifeAgeActualLifeSourcesRequired",
           "missing_original_life_sources");
}

// 内容相同的另一个根也不是当前候选；不以值相等掩盖错世界借用。
void wrong_candidate_roots() {
  BindingFixture f, other;
  const auto original = f.original.bindings();
  auto foreign_body = other.body();
  rejected(original, &foreign_body, f.clock.now,
           "LifeAgeConditionCandidateMismatch", "foreign_candidate_body");
  auto body = f.body();
  body.deaths = &other.deaths;
  rejected(original, &body, f.clock.now, "LifeAgeConditionCandidateMismatch",
           "foreign_death_root");
  CharacterConditionSources catalog_copy{f.common, f.foundation,
                                         other.foundation_catalog, f.clock.now};
  catalog_copy.deaths = &f.deaths;
  catalog_copy.life_sources = &original.life_sources;
  rejected(original, &catalog_copy, f.clock.now,
           "LifeAgeConditionCandidateMismatch", "foreign_catalog_root");
}

// Life4 包装对象可以不同，但底层原 reader/军事目录和根必须逐项同一。
void original_source_identity() {
  BindingFixture f, other;
  const auto original = f.original.bindings();
  auto body = f.body();
  auto same_sources = original.life_sources;
  body.life_sources = &same_sources;
  require(
      validate_life_age_death_condition_sources(original, &body, f.clock.now)
          .ok,
      "same_original_roots_allow_distinct_borrow_wrapper");
  same_sources.age = &other.original;
  rejected(original, &body, f.clock.now,
           "LifeAgeConditionOriginalSourcesMismatch",
           "foreign_age_source_reader");
  auto foreign_catalog = original.life_sources;
  foreign_catalog.catalog_selection = &other.foundation_catalog.identity;
  body.life_sources = &foreign_catalog;
  rejected(original, &body, f.clock.now,
           "LifeAgeConditionOriginalSourcesMismatch",
           "foreign_selected_catalog");
  auto foreign_military = original.life_sources;
  foreign_military.military_exact = &other.military_exact;
  body.life_sources = &foreign_military;
  rejected(original, &body, f.clock.now,
           "LifeAgeConditionOriginalSourcesMismatch",
           "foreign_military_exact_root");
  body.life_sources = &original.life_sources;
  body.military = &other.military;
  rejected(original, &body, f.clock.now,
           "LifeAgeConditionOriginalSourcesMismatch",
           "foreign_body_military_root");
}

// body 与实际阶段时刻必须完全相等；整数 tick 相同也不能吞掉分数偏移。
void actual_moment_identity() {
  BindingFixture f;
  const auto original = f.original.bindings();
  auto body = f.body();
  body.actual_now.half_day_tick += 1;
  rejected(original, &body, f.clock.now, "LifeAgeExactClockMismatch",
           "stale_body_clock");
  body.actual_now = f.clock.now;
  auto fraction = normalize_fraction("1", "2");
  require(fraction.ok, "component_fraction_setup");
  body.actual_now.sub_half_day = fraction.value;
  rejected(original, &body, f.clock.now, "LifeAgeExactClockMismatch",
           "same_tick_wrong_fraction");
  body.actual_now = f.clock.now;
  f.clock.now.half_day_tick += 1;
  rejected(original, &body, body.actual_now, "LifeAgeExactClockMismatch",
           "stale_original_reader_clock");
}
} // namespace

// 仅运行窄桥组件，不调用 A4 或完整业务；任何预期拒绝漂移都非零退出。
int main() {
  try {
    missing_original_sources();
    wrong_candidate_roots();
    original_source_identity();
    actual_moment_identity();
    std::cout << "LIFE_5_CONDITION_SOURCE_BINDING component checks passed; "
                 "LIFE-5 business NOT_RUN\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "LIFE_5_CONDITION_SOURCE_BINDING failure: " << error.what()
              << '\n';
    return 1;
  }
}
