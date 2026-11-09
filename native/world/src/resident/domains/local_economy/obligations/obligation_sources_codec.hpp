#pragma once
#include <string_view>
#include "obligation_sources_model.hpp"
namespace hanmo::resident {
class CodecVisitor;class ProjectionWriter;
Status initialize_economic_obligation_sources(EconomicObligationSourcesState&,Integer selected_version);
Outcome<EconomicObligationSourcesState>decode_economic_obligation_sources(const BoundaryValue&);
Outcome<BoundaryValue>encode_economic_obligation_sources(const EconomicObligationSourcesState&);
Status visit_economic_obligation_sources(CodecVisitor&,EconomicObligationSourcesState&);
Status emit_economic_obligation_sources(ProjectionWriter&,std::string_view,const EconomicObligationSourcesState&);
Status emit_economic_obligation_sources_fields(ProjectionWriter&,const EconomicObligationSourcesState&);
}
