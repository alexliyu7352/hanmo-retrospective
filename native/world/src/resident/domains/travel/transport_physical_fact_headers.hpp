#pragma once
#include "transport_section_codec.hpp"
namespace hanmo::resident {
// These are only the Common headers actually issued by the original producers
// whose payloads are owned by the final V sections. Other travel source kinds,
// authored/owned-field addresses and the old Travel facts remain with their
// original owner. A true answer does not itself validate a fact or its business.
bool owns_transport_physical_fact_kind(std::string_view domain,std::string_view kind)noexcept;
Status validate_transport_physical_fact_header(const CommonStore&,
 const TransportPhysicalCodecRoots&,const FactRecord&);
// Both directions: every original payload has exactly its actual Common head,
// and every owned Common head resolves to exactly one retained original row.
// Original semantic/source validators still run after physical restoration.
Status validate_transport_physical_fact_headers(const CommonStore&,const TransportPhysicalCodecRoots&);
}
