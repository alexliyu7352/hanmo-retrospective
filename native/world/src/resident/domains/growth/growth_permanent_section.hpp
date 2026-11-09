// Complete fixed five-root section. The live World keeps its two original
// owners; only a private decoder result temporarily owns both before installation.
#pragma once
#include "growth_permanent_contributions.hpp"
#include "card_acquisition_contract.hpp"
namespace hanmo::resident {
enum class GrowthPermanentSectionVersion { original=1,capability_application_chain=2,trade_grants=3 };
struct DecodedGrowthPermanentSection {
 GrowthPermanentState permanent;CardAcquisitionState cards;
};
Status visit_growth_permanent_section(CodecVisitor&,GrowthPermanentState&,CardAcquisitionState&,GrowthPermanentSectionVersion);
Status emit_growth_permanent_section_projection(ProjectionWriter&,std::string_view,const GrowthPermanentState&,const CardAcquisitionState&,GrowthPermanentSectionVersion);
Outcome<BoundaryValue> encode_growth_permanent_section(const GrowthPermanentState&,const CardAcquisitionState&,GrowthPermanentSectionVersion);
Outcome<DecodedGrowthPermanentSection> decode_growth_permanent_section(const BoundaryValue&,GrowthPermanentSectionVersion);
}
