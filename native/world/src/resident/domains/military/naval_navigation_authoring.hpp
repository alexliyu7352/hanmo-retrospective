// 航损与远航门仅由本情景显式政策开启，不用合包能力并集推断启用。
#pragma once
#include "naval_navigation_model.hpp"
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident {Outcome<NavalNavigationPolicy> decode_naval_navigation_policy(const BoundaryValue&);}
