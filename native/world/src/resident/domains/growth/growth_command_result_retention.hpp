// Original persistent Growth consumers of the Common result cache.
#pragma once
#include "resident/domains/growth/growth_refs.hpp"
#include "growth_tactic_lessons.hpp"
#include "growth_recovery_model.hpp"
#include <string_view>
namespace hanmo::resident {
enum class GrowthCommandResultSlot {fixed_command,study_adjustment,study_reservation_release,tactic_start,tactic_closure,recovery_closure};
struct GrowthCommandResultReference {
 GrowthCommandResultSlot slot;
 std::string_view consumer_id,actor_id,command_kind,result_id;
 Integer request_seq=0;
 // Actual embedded array ordinal, never a fabricated source instance.
 Integer consumer_ordinal=0;
 // The original embedded value, where present; FSL has no second revision field.
 std::optional<Integer> committed_revision;
};
class GrowthCommandResultSink {public:virtual ~GrowthCommandResultSink()=default;virtual Status actual_reference(const GrowthCommandResultReference&)=0;};
// Streams only original embedded result references, including stopped command
// causes. No inferred result from a fact ID, tick, decision sequence or current XP.
Status visit_growth_command_result_references(const GrowthState&,const GrowthTacticLessonState*,const GrowthRecoveryState*,GrowthCommandResultSink&);
// G+96 source cuts use actual Common insertion positions. Every original head
// in the saved prefix is retained while that immutable adjustment needs the cut.
class GrowthAdjustmentFactPrefixSink {
public:virtual ~GrowthAdjustmentFactPrefixSink()=default;
 virtual Status actual_prefix_fact(std::string_view study_action,Integer lesson_ordinal,std::string_view fact_id)=0;
};
Status visit_growth_adjustment_fact_prefix_references(const CommonStore&,const GrowthState&,const GrowthCatalog&,GrowthAdjustmentFactPrefixSink&);
}
