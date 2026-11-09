#pragma once
#include "training_exact_model.hpp"
#include "military_action_conditions.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/kernel/source_retention.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
// Explicit /1 physical adjunct: schema, formation_acceptances, return_acceptances.
// Attach to the original decoded ActionTime rows, never another durable table.
Status visit_military_action_conditions(CodecVisitor&,MilitaryActionTimeState&,Integer selected_version);
Status decode_military_action_conditions(const BoundaryValue&,MilitaryActionTimeState&,Integer selected_version);
Status emit_military_action_conditions_fields(ProjectionWriter&,const MilitaryActionTimeState&,Integer selected_version);
Status emit_military_action_conditions(ProjectionWriter&,std::string_view,const MilitaryActionTimeState&,Integer selected_version);
Outcome<BoundaryValue> encode_military_action_conditions(const MilitaryActionTimeState&,Integer selected_version);
// Physical bootstrap only: selected0 absence, selected1 actual owner/shape coverage.
Status validate_military_action_condition_layout(const MilitaryActionTimeState&,Integer selected_version);
// Source-local prefix proof. Full original C07 participant and all eligibility
// joins remain with its owning reader, not this physical adjunct bootstrap.
Status validate_military_action_condition_sources(const MilitaryActionConditionContext&,Integer selected_version);
Status collect_military_action_condition_pins(const MilitaryActionConditionContext&,SourcePinSink&,Integer selected_version=1);
Status validate_military_action_condition_pin(const MilitaryActionConditionContext&,const SourcePin&,Integer selected_version=1);
std::vector<RequiredSourceConsumerSlot> military_action_condition_consumer_slots();
Outcome<bool> military_action_condition_source_may_release(const MilitaryActionConditionContext&,const SourceRef&,Integer source_schema,Integer selected_version=1);
}
