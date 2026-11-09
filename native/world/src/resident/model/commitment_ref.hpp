// CK1 addressing only. Terms identity does not create or replace any owner's contract state.
#pragma once
#include "resident/model/dynamic_contract.hpp"
#include "resident/kernel/source_ref.hpp"
namespace hanmo::resident {
struct SourceTermsKey {StableId key;};
struct SourceFactTerms {SourceRef source;};
struct Revision {Integer revision=0;};
using TermsRevisionRef=std::variant<SourceTermsKey,SourceFactTerms,Revision>;
struct CommitmentRef {StableId domain,kind,id;TermsRevisionRef terms_revision;};
bool same_terms_revision(const TermsRevisionRef&,const TermsRevisionRef&);
bool same_commitment_ref(const CommitmentRef&,const CommitmentRef&);
Status validate_terms_revision(const TermsRevisionRef&);
Status validate_commitment_ref(const CommitmentRef&);
struct BoundaryValue;
struct CodecVisitor;
Outcome<TermsRevisionRef> decode_terms_revision(const BoundaryValue &);
Outcome<BoundaryValue> encode_terms_revision(const TermsRevisionRef&);
Status visit_terms_revision_checkpoint(CodecVisitor&,TermsRevisionRef&);
Outcome<CommitmentRef> decode_commitment_ref(const BoundaryValue&);
Outcome<BoundaryValue> encode_commitment_ref(const CommitmentRef&);
Status visit_commitment_ref_checkpoint(CodecVisitor&,CommitmentRef&);
}
