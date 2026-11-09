// M-TREATY original activation row. Term obligations and adopted grants remain
// with their original owners; this row is not a second performance ledger.
#pragma once
#include "treaty_signature_sources.hpp"
namespace hanmo::resident {
struct TreatyActivationSource {
 StableId id,treaty_id;Integer terms_revision=0;SourceRef terms_source;
 // Exactly the immutable terms' party order, each a real original signature.
 std::vector<SourceRef> signature_refs;WorldMoment activated_at;
};
struct TreatyActivationReadyView {
 TreatyRevisionView revision;std::vector<SourceRef> signature_refs;WorldMoment activation_at;
};
struct TreatyActivationView {
 const TreatyActivationSource* source=nullptr;TreatyRevisionView revision;
};
// Implemented by the selected original term owners. These are mandatory actual
// adoption/history consumers, not optional success callbacks. The historical
// read must not call the full activation validator back recursively.
class TreatyActivationOriginalRead {public:virtual ~TreatyActivationOriginalRead()=default;
 virtual Status validate_original_activation_inputs(const TreatyActivationReadyView&,
  const SourceRef* actual_activation_boundary)const=0;
 virtual Status validate_applied_treaty_activation(const TreatyActivationView&)const=0;
};
class TreatyActivationOriginalWrite {public:virtual ~TreatyActivationOriginalWrite()=default;
 // Runs AFTER the genuine source is appended. Aid holds and a first occurrence
 // due now, and submission's actual original Grant changes, are same-candidate
 // consequences. Any failure discards the entire candidate including signatures.
 virtual Status apply_treaty_activation_in_candidate(CommonStore&,const TreatyActivationView&)=0;
};
struct TreatyActivationContext {
 const TreatySignatureContext& signatures;
 const OrderedTable<TreatyActivationSource>& activations;
 const TreatyActivationOriginalRead& originals;
};
SourceRef treaty_activation_source(const TreatyActivationSource&);
Status validate_treaty_activation_shape(const TreatyActivationSource&);
// Pure original-owner prediction. Missing recognition returns null; a later
// clock never repairs a missed source. Superseded revisions are not predicted.
Outcome<std::optional<TreatyActivationReadyView>> read_current_treaty_activation_ready(
 const TreatyActivationContext&,const StableId& treaty_id);
// Raw source leg for original term/performance readers. It validates every
// actual signature and exact M/cut but does not re-enter downstream adoption.
Outcome<TreatyActivationView> read_original_treaty_activation(
 const TreatyActivationContext&,const SourceRef&,Integer schema);
Outcome<TreatyActivationView> read_verified_treaty_activation(
 const TreatyActivationContext&,const SourceRef&,Integer schema);
Status validate_treaty_activation_sources(const TreatyActivationContext&);
Outcome<SourceRef> activate_treaty_terms_in_candidate(CommonStore&,
 OrderedTable<TreatyActivationSource>&,const TreatyActivationContext&,
 TreatyActivationOriginalWrite&,const StableId& treaty_id);
struct RatifyAndActivateTreatyResult {SourceRef signature;std::optional<SourceRef> activation;};
// Mandatory original command wrapper for the direct-signing path. If this
// signature completes recognition and from has arrived, activation is adopted
// now in this same candidate. A future from is handled by the original due read.
Outcome<RatifyAndActivateTreatyResult> ratify_and_activate_treaty_in_candidate(
 CommonStore&,OrderedTable<TreatySignatureSource>&,AuthorityBasisRegistry&,
 OrderedTable<TreatyActivationSource>&,const TreatyActivationContext&,
 TreatyActivationOriginalWrite&,const RatifyTreatyTermsCommand&);
Status visit_treaty_activation_source(CodecVisitor&,TreatyActivationSource&);
Status emit_treaty_activation_source_projection(ProjectionWriter&,std::string_view,const TreatyActivationSource&);
Outcome<BoundaryValue> encode_treaty_activation_source(const TreatyActivationSource&);
Outcome<TreatyActivationSource> decode_treaty_activation_source(const BoundaryValue&);
}
