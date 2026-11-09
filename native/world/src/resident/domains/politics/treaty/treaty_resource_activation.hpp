#pragma once
#include "treaty_activation_sources.hpp"
#include "treaty_resource_signature_read.hpp"
namespace hanmo::resident {
// Selected activation/2 keeps the original activation fields, but its signature
// refs resolve only in the actual signature/2 owner. It is not a /1 fallback.
struct TreatyResourceActivationContext {
 const TreatyResourceSignatureReadContext& signatures;
 const OrderedTable<TreatyActivationSource>& activations;
 const TreatyActivationOriginalRead& originals;
};
Outcome<std::optional<TreatyActivationReadyView>> read_current_treaty_resource_activation_ready(const TreatyResourceActivationContext&,const StableId&);
Outcome<TreatyActivationView> read_original_treaty_resource_activation(const TreatyResourceActivationContext&,const SourceRef&,Integer schema);
Outcome<TreatyActivationView> read_verified_treaty_resource_activation(const TreatyResourceActivationContext&,const SourceRef&,Integer schema);
Status validate_treaty_resource_activation_sources(const TreatyResourceActivationContext&);
Outcome<SourceRef> activate_treaty_resource_terms_in_candidate(CommonStore&,OrderedTable<TreatyActivationSource>&,const TreatyResourceActivationContext&,TreatyActivationOriginalWrite&,const StableId&);
Status visit_treaty_resource_activation2(CodecVisitor&,TreatyActivationSource&);
Status emit_treaty_resource_activation2_projection(ProjectionWriter&,std::string_view,const TreatyActivationSource&);
Outcome<BoundaryValue> encode_treaty_resource_activation2(const TreatyActivationSource&);
Outcome<TreatyActivationSource> decode_treaty_resource_activation2(const BoundaryValue&);
}
