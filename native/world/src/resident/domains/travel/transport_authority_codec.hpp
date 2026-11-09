#pragma once
#include "group_journey_funding.hpp"
#include "../../checkpoint/codec_visitor.hpp"
#include "../../checkpoint/projection_writer.hpp"
namespace hanmo::resident {
class ScopedCheckpointJsonDecode;
Status visit_transport_expense_authority(CodecVisitor&,TransportExpenseAuthority&);
Status visit_transport_expense_authority(CodecVisitor&,std::string_view name,TransportExpenseAuthority&);
Outcome<TransportExpenseAuthority> decode_transport_expense_authority(const BoundaryValue&);
Outcome<TransportExpenseAuthority> decode_transport_expense_authority(const BoundaryValue&,ScopedCheckpointJsonDecode&);
Status emit_transport_expense_authority_fields(ProjectionWriter&,const TransportExpenseAuthority&);
Status emit_transport_expense_authority_projection(ProjectionWriter&,std::string_view,const TransportExpenseAuthority&);
Outcome<BoundaryValue> export_transport_expense_authority(const TransportExpenseAuthority&);
bool same_transport_expense_authority(const TransportExpenseAuthority&,const TransportExpenseAuthority&);
}
