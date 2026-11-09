// 原FamilyDeathState终点引用；消息、HP0、任职结果均不能充当生命证明。
#pragma once
#include "resident/model/common_model.hpp"
namespace hanmo::resident {
struct DeathRef {StableId domain="family_life",kind="family_death",id;};
}
