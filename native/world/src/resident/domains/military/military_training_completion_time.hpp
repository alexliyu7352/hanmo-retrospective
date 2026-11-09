// Original C06 release occurrence. The immutable terminal cause stays separate.
#pragma once
#include "training_exact.hpp"
#include "resident/kernel/source_retention.hpp"
namespace hanmo::resident {
struct MilitaryTrainingCompletionContext {
 const CommonStore& common;const MilitaryState& military;
 const MilitaryActionTimeState& actions;const ActionElapsedStore& elapsed;
 const TimeScaleRef& scale;const CurrentMomentRead& clock;ExactArithmeticBudget budget{};
};
Status validate_military_training_completion_time_layout(const MilitaryActionTimeState&,Integer selected_version);
// Bootstrap identity only. Does not recursively invoke acceptance or S0 closure.
Status validate_military_training_completion_header_source(const FactRecord&,const MilitaryState&,const MilitaryActionTimeState&,Integer selected_version);
class ProjectionWriter;
Status emit_military_training_completion_header_payload_fields(ProjectionWriter&,const FactRecord&,const MilitaryState&,const MilitaryActionTimeState&,Integer selected_version);
// Actual Common order of start, original cause (if Common-owned), and release.
Status validate_exact_training_completion_header(const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,const DynamicMilitaryTrainingFact2&);
// Source-local existing occupation/history proof, never whole training admission.
Outcome<const DynamicMilitaryTrainingFact2*> read_exact_training_completion_application(const MilitaryTrainingCompletionContext&,const SourceRef&,Integer source_schema);
Status validate_military_training_completion_time_sources(const MilitaryTrainingCompletionContext&,Integer selected_version);
Status collect_military_training_completion_time_pins(const MilitaryTrainingCompletionContext&,SourcePinSink&);
Status validate_military_training_completion_time_pin(const MilitaryTrainingCompletionContext&,const SourcePin&);
std::vector<RequiredSourceConsumerSlot> military_training_completion_time_consumer_slots();
Outcome<bool> military_training_completion_time_source_may_release(const MilitaryTrainingCompletionContext&,const SourceRef&,Integer source_schema);
}
