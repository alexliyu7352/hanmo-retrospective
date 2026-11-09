// 唯一世界候选的Social期间接头；纯订阅不是Director注册或决策能力。
#pragma once
#include "resident/domains/social/social_time2.hpp"
#include "resident/domains/social/social_period_sources.hpp"
namespace hanmo::resident {
struct WorldBundle;
Status bootstrap_exact_social_in_candidate(WorldBundle&);
Status collect_exact_social_in_candidate(WorldBundle&);
Status validate_exact_social_world(const WorldBundle&);
// 原严格空scope只按这一个选中组合开放静态供给源，所有其它宏观写者仍拒绝。
Status validate_exact_social_source_scope(const WorldBundle&);
}
