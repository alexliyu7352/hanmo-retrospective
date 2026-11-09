// M-TREATY original termination, with finite P15.3 causes. Actual obligations,
// safe-exit and return/settlement remain the original owners' responsibilities.
#pragma once
#include "treaty_activation_sources.hpp"
#include "treaty_aid_schedule.hpp"
namespace hanmo::resident {
struct TreatyExpiryTermination {};
struct TreatyMutualReleaseTermination {std::vector<SourceRef> release_acceptance_refs;};
struct TreatyBreachAfterNoticeTermination {
 SourceRef incident_ref;std::vector<SourceRef> notice_refs;SourceRef actual_decision_ref;
};
using TreatyTerminationCause=std::variant<TreatyExpiryTermination,TreatyMutualReleaseTermination,TreatyBreachAfterNoticeTermination>;
struct TreatyTerminationSource {
 StableId id,treaty_id;Integer terms_revision=0;SourceRef activation_ref;
 TreatyTerminationCause cause;WorldMoment terminated_at;
};
struct TreatyTerminationView {const TreatyTerminationSource* source=nullptr;TreatyActivationView activation;};
class TreatyTerminationOriginalRead {public:virtual ~TreatyTerminationOriginalRead()=default;
 // Re-read the actual same-version release consent, or incident, delivered
 // notice/elapsed original notice window and lawful original decision. Never
 // derive authority/consent from this termination payload or today's holder.
 virtual Status validate_original_treaty_termination_cause(const TreatyActivationView&,
  const TreatyTerminationCause&,const WorldMoment&,const SourceRef* actual_boundary)const=0;
 // Every selected original term owns its actual future release/return/retained
 // old debt disposition. This read must not recurse into full termination.
 virtual Status validate_applied_treaty_termination(const TreatyTerminationView&)const=0;
};
class TreatyTerminationOriginalWrite {public:virtual ~TreatyTerminationOriginalWrite()=default;
 virtual Status apply_treaty_termination_in_candidate(CommonStore&,const TreatyTerminationView&)=0;
};
struct TreatyTerminationContext {
 const TreatyActivationContext& activations;const OrderedTable<TreatyTerminationSource>& terminations;
 const TreatyTerminationOriginalRead& originals;
};
SourceRef treaty_termination_source(const TreatyTerminationSource&);
Status validate_treaty_termination_shape(const TreatyTerminationSource&);
Outcome<TreatyTerminationView> read_original_treaty_termination(const TreatyTerminationContext&,const SourceRef&,Integer schema);
Outcome<TreatyTerminationView> read_verified_treaty_termination(const TreatyTerminationContext&,const SourceRef&,Integer schema);
Status validate_treaty_termination_sources(const TreatyTerminationContext&);
Outcome<SourceRef> terminate_treaty_terms_in_candidate(CommonStore&,OrderedTable<TreatyTerminationSource>&,
 const TreatyTerminationContext&,TreatyTerminationOriginalWrite&,const SourceRef& actual_activation,
 const TreatyTerminationCause& actual_cause);
// Exact original M-TREATY source projection for the one M-PERFORMANCE schedule.
// Uses raw verified original source legs, avoiding a performance -> terms ->
// performance validation cycle. Full World validation separately covers both.
class NativeTreatyAidTermsRead final:public TreatyAidTermsOriginalRead {
 const TreatyTerminationContext& context_;
public:explicit NativeTreatyAidTermsRead(const TreatyTerminationContext& c):context_(c){}
 Outcome<ActivatedTreatyAidTermsRead> read_activated_material_aid_terms(
  const StableId&,Integer,const StableId&)const override;
};
Status visit_treaty_termination_source(CodecVisitor&,TreatyTerminationSource&);
Status emit_treaty_termination_source_projection(ProjectionWriter&,std::string_view,const TreatyTerminationSource&);
Outcome<BoundaryValue> encode_treaty_termination_source(const TreatyTerminationSource&);
Outcome<TreatyTerminationSource> decode_treaty_termination_source(const BoundaryValue&);
}
