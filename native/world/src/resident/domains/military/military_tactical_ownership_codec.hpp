#pragma once
#include "military_tactical_ownership_model.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
struct MilitaryExactState;
// Only the two original children, in formations -> missions field order.
// The complete /3 root selector owns their placement before existing actions.
Status visit_military_tactical_ownership3(CodecVisitor&,MilitaryExactState&);
Status emit_military_tactical_ownership3(ProjectionWriter&,const MilitaryExactState&);
Status visit_military_tactical_formation3(CodecVisitor&,MilitaryTacticalFormation3&);
Status visit_military_tactical_mission3(CodecVisitor&,MilitaryTacticalMissionRecord3&);
Status visit_direct_force_intent3(CodecVisitor&,DirectForceIntent3&);
Status visit_direct_force_intent_receipt3(CodecVisitor&,DirectForceIntentReceipt3&);
Outcome<MilitaryTacticalFormation3> decode_military_tactical_formation3(const BoundaryValue&);
Outcome<MilitaryTacticalMissionRecord3> decode_military_tactical_mission3(const BoundaryValue&);
Outcome<DirectForceIntent3> decode_direct_force_intent3(const BoundaryValue&);
Outcome<DirectForceIntentReceipt3> decode_direct_force_intent_receipt3(const BoundaryValue&);
Outcome<BoundaryValue> encode_military_tactical_formation3(const MilitaryTacticalFormation3&);
Outcome<BoundaryValue> encode_military_tactical_mission3(const MilitaryTacticalMissionRecord3&);
Outcome<BoundaryValue> encode_direct_force_intent3(const DirectForceIntent3&);
Outcome<BoundaryValue> encode_direct_force_intent_receipt3(const DirectForceIntentReceipt3&);
Status emit_military_tactical_formation3(ProjectionWriter&,std::string_view,const MilitaryTacticalFormation3&);
Status emit_military_tactical_mission3(ProjectionWriter&,std::string_view,const MilitaryTacticalMissionRecord3&);
Status emit_direct_force_intent3(ProjectionWriter&,std::string_view,const DirectForceIntent3&);
Status emit_direct_force_intent_receipt3(ProjectionWriter&,std::string_view,const DirectForceIntentReceipt3&);
Status emit_military_tactical_formation3_fields(ProjectionWriter&,const MilitaryTacticalFormation3&);
Status emit_military_tactical_mission3_fields(ProjectionWriter&,const MilitaryTacticalMissionRecord3&);
Status emit_direct_force_intent3_fields(ProjectionWriter&,const DirectForceIntent3&);
Status emit_direct_force_intent_receipt3_fields(ProjectionWriter&,const DirectForceIntentReceipt3&);
}
