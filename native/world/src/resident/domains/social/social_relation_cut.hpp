// Borrowed adoption inputs, no extra persistent cut or caller-selected revision.
#pragma once
#include "resident/kernel/common_fact_prefix.hpp"
#include "hanmo/resident/world_moment.hpp"
namespace hanmo::resident {
struct OriginalSocialRelationCut {
 const WorldMoment& adopted_at;const CommonActualFactPrefix& original_prefix;
};
}
