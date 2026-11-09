// 原StudySegment.adjustment的显式Time2嵌值后继候选；没有第二XP或调整表。
#pragma once
#include "growth_capability_application.hpp"
#include "resident/kernel/world_timeline.hpp"
namespace hanmo::resident {
struct StudyAdjustmentFactPrefix {Integer original_fact_prefix_size=0;std::optional<StableId> last_fact_id;};
struct StudyAdjustmentExactBinding {
 WorldMoment settled_at;
 StableId result_id,actor_id,command_kind;
 Integer request_seq=0,revision=0,application_ordinal=0;
 // G+96 only. The original zero-time commit tail captures the real Common append cut.
 std::optional<StudyAdjustmentFactPrefix> original_fact_prefix;
 std::optional<GrowthCapabilityApplication> capability_application;
};
// 受信WorldBundle实际phase clock；不得由Godot请求或当前XP构造发生时点。
struct StudyAdjustmentWriteContext {WorldMoment actual_moment;};
}
