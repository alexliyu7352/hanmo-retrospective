#pragma once
#include "military_spatial_handoff.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
Status visit_military_spatial_chart2(CodecVisitor&,MilitarySpatialChart2&);
Status visit_military_spatial_handoff2(CodecVisitor&,MilitarySpatialHandoff2&);
Status visit_military_entry_connector_definition(CodecVisitor&,MilitaryEntryConnectorDefinition&);
Outcome<MilitarySpatialChart2> decode_military_spatial_chart2(const BoundaryValue&);
Outcome<MilitarySpatialHandoff2> decode_military_spatial_handoff2(const BoundaryValue&);
Outcome<MilitarySpatialGeometryDefinition2> decode_military_spatial_definition2(const BoundaryValue&);
Outcome<MilitaryEntryConnectorDefinition> decode_military_entry_connector_definition(const BoundaryValue&);
Outcome<BoundaryValue> encode_military_spatial_chart2(const MilitarySpatialChart2&);
Outcome<BoundaryValue> encode_military_spatial_handoff2(const MilitarySpatialHandoff2&);
Outcome<BoundaryValue> encode_military_spatial_definition2(const MilitarySpatialGeometryDefinition2&);
Outcome<BoundaryValue> encode_military_entry_connector_definition(const MilitaryEntryConnectorDefinition&);
Status emit_military_spatial_chart2(ProjectionWriter&,std::string_view,const MilitarySpatialChart2&);
Status emit_military_spatial_handoff2(ProjectionWriter&,std::string_view,const MilitarySpatialHandoff2&);
Status emit_military_spatial_definition2(ProjectionWriter&,std::string_view,const MilitarySpatialGeometryDefinition2&);
Status emit_military_entry_connector_definition(ProjectionWriter&,std::string_view,const MilitaryEntryConnectorDefinition&);
Status emit_military_spatial_chart2_fields(ProjectionWriter&,const MilitarySpatialChart2&);
Status emit_military_spatial_handoff2_fields(ProjectionWriter&,const MilitarySpatialHandoff2&);
Status emit_military_spatial_definition2_fields(ProjectionWriter&,const MilitarySpatialGeometryDefinition2&);
Status emit_military_entry_connector_definition_fields(ProjectionWriter&,const MilitaryEntryConnectorDefinition&);
}
