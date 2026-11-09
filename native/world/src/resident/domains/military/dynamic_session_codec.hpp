#pragma once
#include <string_view>
#include "dynamic_session.hpp"
namespace hanmo::resident {
class ProjectionWriter;
Status visit_dynamic_military_session_current(CodecVisitor&,DynamicMilitarySessionCurrent2&);
Status visit_dynamic_military_session_source(CodecVisitor&,DynamicSessionStateSource2&);
Status visit_dynamic_military_random_slot(CodecVisitor&,DynamicMilitaryRandomSlot&);
// Exactly these two arrays inside the original dynamic_military/2 section.
// This is not a second section, and not the complete /2 section writer.
Status visit_dynamic_military_session_payloads(CodecVisitor&,MilitaryExactState&);
Outcome<DynamicMilitarySessionCurrent2> decode_dynamic_military_session_current(const BoundaryValue&);
Outcome<BoundaryValue> encode_dynamic_military_session_current(const DynamicMilitarySessionCurrent2&);
Status emit_dynamic_military_session_current(ProjectionWriter&,std::string_view name,const DynamicMilitarySessionCurrent2&);
Outcome<DynamicSessionStateSource2> decode_dynamic_military_session_source(const BoundaryValue&);
Outcome<BoundaryValue> encode_dynamic_military_session_source(const DynamicSessionStateSource2&);
Status emit_dynamic_military_session_source(ProjectionWriter&,std::string_view name,const DynamicSessionStateSource2&);
Outcome<DynamicMilitaryRandomSlot> decode_dynamic_military_random_slot(const BoundaryValue&);
Outcome<BoundaryValue> encode_dynamic_military_random_slot(const DynamicMilitaryRandomSlot&);
Status emit_dynamic_military_random_slot(ProjectionWriter&,std::string_view name,const DynamicMilitaryRandomSlot&);
}
