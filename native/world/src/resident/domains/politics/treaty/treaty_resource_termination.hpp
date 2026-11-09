#pragma once
#include "treaty_termination_sources.hpp"
#include "treaty_resource_activation.hpp"
namespace hanmo::resident {
struct TreatyResourceTerminationContext {
 const TreatyResourceActivationContext& activations;
 const OrderedTable<TreatyTerminationSource>& terminations;
 const TreatyTerminationOriginalRead& originals;
};
Outcome<TreatyTerminationView> read_original_treaty_resource_termination(const TreatyResourceTerminationContext&,const SourceRef&,Integer schema);
Outcome<TreatyTerminationView> read_verified_treaty_resource_termination(const TreatyResourceTerminationContext&,const SourceRef&,Integer schema);
Status validate_treaty_resource_termination_sources(const TreatyResourceTerminationContext&);
Outcome<SourceRef> terminate_treaty_resource_terms_in_candidate(CommonStore&,OrderedTable<TreatyTerminationSource>&,const TreatyResourceTerminationContext&,TreatyTerminationOriginalWrite&,const SourceRef&,const TreatyTerminationCause&);
// Actual /2 activation and termination originals, no performance/financial
// downstream callback in this source leg. It is not a /1-row interpretation.
class NativeTreatyResourceAidTermsRead final:public TreatyAidTermsOriginalRead {
 const TreatyResourceTerminationContext& context_;
public:explicit NativeTreatyResourceAidTermsRead(const TreatyResourceTerminationContext&x):context_(x){}
 Outcome<ActivatedTreatyAidTermsRead> read_activated_material_aid_terms(const StableId&,Integer,const StableId&)const override;
};
Status visit_treaty_resource_termination2(CodecVisitor&,TreatyTerminationSource&);
Status emit_treaty_resource_termination2_projection(ProjectionWriter&,std::string_view,const TreatyTerminationSource&);
Outcome<BoundaryValue> encode_treaty_resource_termination2(const TreatyTerminationSource&);
Outcome<TreatyTerminationSource> decode_treaty_resource_termination2(const BoundaryValue&);
}
