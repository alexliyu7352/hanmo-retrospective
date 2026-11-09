// 原生日的成年门数学回归；不冒完整 Family、命令门或世界存读验收。
#include "resident/domains/family/child_maturation_exact.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace hanmo::resident;

namespace {

// 数学门失败时给出具体断言，不能静默改成另一历法或延后成年。
void check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}

// 实际历法来自原固定 admit，而不是伪造可以成功的 Calendar reader。
OriginalCalendarRead calendar() {
  CatalogIdentity identity;
  identity.ruleset_id = "original_maturation_calendar_regression";
  identity.content_digest = "original_maturation_calendar_regression_digest";
  auto admitted = OriginalCalendarRead::admit(
      identity, {original_calendar_rule_id, 1, identity.content_digest});
  check(admitted.ok, "original calendar admission failed");
  return std::move(admitted.value);
}

} // namespace

// 非年首出生须在本人的实际生日成年，不能等下一全世界年界。
// 未选择历法、负生日与整数溢出必须失败，不制造当前时刻结果。
int main() {
  try {
    const auto original = calendar();
    const auto near_end = normalize_fraction("999", "1000");
    check(near_end.ok, "exact fractional test moment was rejected");
    for (const Integer birth :
         {Integer{0}, Integer{1}, Integer{560}, Integer{719}, Integer{720}}) {
      const auto due = original_child_maturation_boundary(original, birth);
      check(due.ok, "valid original birthday was rejected");
      check(due.value.half_day_tick == birth + 18 * 720,
            "maturation rounded to a global year boundary");
      const auto fraction =
          compare_fractions(due.value.sub_half_day, CanonicalFraction{});
      check(fraction.ok && fraction.value == 0,
            "original birthday acquired a fractional offset");
      const auto before =
          original.exact_birth_age_at(birth, {due.value.half_day_tick - 1, {}});
      const auto after = original.exact_birth_age_at(birth, due.value);
      check(before.ok && before.value == 17 && after.ok && after.value == 18,
            "maturation is not the first real adult birthday");
      const auto immediately_before = original.exact_birth_age_at(
          birth, {due.value.half_day_tick - 1, near_end.value});
      check(immediately_before.ok && immediately_before.value == 17,
            "fractional moment before the birthday matured early");
    }
    check(!original_child_maturation_boundary(original, -1).ok,
          "negative Birth4 birthday accepted");
    check(!original_child_maturation_boundary(
               original, std::numeric_limits<Integer>::max() - 18 * 720 + 1)
               .ok,
          "maturation overflow accepted");
    const auto last_representable = original_child_maturation_boundary(
        original, std::numeric_limits<Integer>::max() - 18 * 720);
    check(last_representable.ok && last_representable.value.half_day_tick ==
                                       std::numeric_limits<Integer>::max(),
          "last representable original birthday was rejected");
    check(!original_child_maturation_boundary(OriginalCalendarRead{}, 0).ok,
          "unadmitted original calendar accepted");
    std::cout << "child_maturation_exact_boundary_test: PASS\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "child_maturation_exact_boundary_test: " << error.what()
              << '\n';
    return 1;
  }
}
