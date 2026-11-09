// 正规Installed原actors投影的窄门；不构造当前Common冒初态。
#include "resident/domains/growth/growth_personal_mastery_history.hpp"
#include "resident/storage/common_codec.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
Integer count = 0;
void check(bool v, const char *s) {
  ++count;
  if (!v)
    throw std::runtime_error(s);
}
} // namespace
int main() {
  try {
    CommonCatalog c;
    c.actor_ids = {"student", "teacher"};
    c.personal_skill_kinds.insert("sword", "strike");
    c.personal_skill_kinds.insert("heart", "heart");
    auto parsed = parse_boundary_json(
        R"({"actors":[{"actor_id":"student","combat":{}},{"actor_id":"teacher","combat":{"weapon_id":"blade","learned":[{"skill_id":"sword","mastery":80},{"skill_id":"heart","mastery":0}]}}]})");
    check(parsed.ok, "real boundary parse");
    const auto before = render_boundary_json(parsed.value).value;
    auto good = make_growth_personal_mastery_initial_catalog(parsed.value, c,
                                                             "scenario");
    check(good.ok && good.value.scenario_id == "scenario",
          "exact scenario projected");
    check(good.value.actors.find("student")->empty(),
          "missing optional learned means original empty");
    check(*good.value.actors.find("teacher")->find("sword") == 80 &&
              *good.value.actors.find("teacher")->find("heart") == 0,
          "original mastery including learned zero");
    check(render_boundary_json(parsed.value).value == before,
          "projection does not mutate original input");
    // 原personal_skills的真实类别是strike/guard/heart，不是成长卡片分类technique。
    for (const auto *kind : {"strike", "guard", "heart"}) {
      c.personal_skill_kinds.set("sword", kind);
      check(make_growth_personal_mastery_initial_catalog(parsed.value, c,
                                                         "scenario")
                .ok,
            "actual personal skill kinds are admitted");
    }
    c.personal_skill_kinds.set("sword", "unknown");
    check(!make_growth_personal_mastery_initial_catalog(parsed.value, c,
                                                        "scenario")
               .ok,
          "unknown personal skill kind rejected");
    c.personal_skill_kinds.set("sword", "strike");
    check(!make_growth_personal_mastery_initial_catalog(parsed.value, c, {}).ok,
          "missing selected scenario rejected");
    auto missing = parsed.value;
    missing.find("actors")->as<BoundaryValue::Array>()->pop_back();
    check(!make_growth_personal_mastery_initial_catalog(missing, c, "scenario")
               .ok,
          "missing person cannot imply empty learned");
    auto duplicate = parsed.value;
    auto &rows = *duplicate.find("actors")->as<BoundaryValue::Array>();
    rows.push_back(rows.front());
    check(
        !make_growth_personal_mastery_initial_catalog(duplicate, c, "scenario")
             .ok,
        "duplicate actor rejected");
    auto forged = parsed.value;
    auto &entry = (*forged.find("actors")->as<BoundaryValue::Array>())[1];
    entry.find("combat")
        ->find("learned")
        ->as<BoundaryValue::Array>()
        ->front()
        .find("mastery")
        ->data = Integer{101};
    check(
        !make_growth_personal_mastery_initial_catalog(forged, c, "scenario").ok,
        "invalid mastery rejected");
    c.personal_skill_kinds.erase("sword");
    check(!make_growth_personal_mastery_initial_catalog(parsed.value, c,
                                                        "scenario")
               .ok,
          "unknown skill rejected");
    c.personal_skill_kinds.insert("sword", "strike");
    auto duplicated = parsed.value;
    auto &ls = *(*duplicated.find("actors")->as<BoundaryValue::Array>())[1]
                    .find("combat")
                    ->find("learned")
                    ->as<BoundaryValue::Array>();
    ls.push_back(ls.front());
    check(
        !make_growth_personal_mastery_initial_catalog(duplicated, c, "scenario")
             .ok,
        "duplicate mastery source rejected");
    std::cout << "GROWTH_PERSONAL_MASTERY_INITIAL " << count
              << " checks 0 failures\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "GROWTH_PERSONAL_MASTERY_INITIAL " << count
              << " checks 1 failure: " << e.what() << '\n';
    return 1;
  }
}
