#pragma once
#include "transport_authority_codec.hpp"
namespace hanmo::resident {
// Only the explicitly selected group_transport successor uses this outer tag.
// Legacy versions continue to call the original four-branch authority visitor.
Status visit_group_journey_funding(CodecVisitor&,GroupJourneyFunding&);
Outcome<GroupJourneyFunding> decode_group_journey_funding(const BoundaryValue&);
Status emit_group_journey_funding_fields(ProjectionWriter&,const GroupJourneyFunding&);
Status emit_group_journey_funding(ProjectionWriter&,std::string_view,const GroupJourneyFunding&);
Outcome<BoundaryValue> encode_group_journey_funding(const GroupJourneyFunding&);
}
