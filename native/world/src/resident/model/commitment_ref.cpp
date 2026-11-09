#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "commitment_ref.hpp"
#include "commitment_ref_codec.hpp"
#include "resident/domains/tasks/task_dynamic_codec.hpp"
namespace hanmo::resident {
bool same_terms_revision(const TermsRevisionRef&a,const TermsRevisionRef&b){if(a.index()!=b.index())return false;return std::visit([&](const auto&x){using T=std::decay_t<decltype(x)>;const auto&y=std::get<T>(b);if constexpr(std::is_same_v<T,SourceTermsKey>)return x.key==y.key;else if constexpr(std::is_same_v<T,Revision>)return x.revision==y.revision;else return x.source.domain==y.source.domain&&x.source.kind==y.source.kind&&x.source.id==y.source.id;},a);}
bool same_commitment_ref(const CommitmentRef&a,const CommitmentRef&b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id&&same_terms_revision(a.terms_revision,b.terms_revision);}
Status validate_terms_revision(const TermsRevisionRef&v){const bool valid=std::visit([](const auto&x){using T=std::decay_t<decltype(x)>;if constexpr(std::is_same_v<T,SourceTermsKey>)return !x.key.empty();else if constexpr(std::is_same_v<T,Revision>)return x.revision>=1;else return !x.source.domain.empty()&&!x.source.kind.empty()&&!x.source.id.empty();},v);return valid?common_success():common_failure("InvalidTermsRevisionRef");}
Status validate_commitment_ref(const CommitmentRef&v){if(v.domain.empty()||v.kind.empty()||v.id.empty())return common_failure("InvalidCommitmentRef");return validate_terms_revision(v.terms_revision);}
using namespace commitment_codec;
Outcome<TermsRevisionRef> decode_terms_revision(const BoundaryValue&v){TermsRevisionRef out;dynamic_codec::JsonArchive a(v);auto s=terms(a,out);if(s.ok)s=a.finish();return s.ok?Outcome<TermsRevisionRef>{true,std::move(out),{}}:Outcome<TermsRevisionRef>{false,{},s.error};}
Outcome<BoundaryValue> encode_terms_revision(const TermsRevisionRef&v){auto&borrowed=const_cast<TermsRevisionRef&>(v);dynamic_codec::JsonArchive a;auto s=terms(a,borrowed);if(s.ok)s=a.finish();return s.ok?Outcome<BoundaryValue>{true,a.value(),{}}:Outcome<BoundaryValue>{false,{},s.error};}
Status visit_terms_revision_checkpoint(CodecVisitor&a,TermsRevisionRef&v){return terms(a,v);}
Outcome<CommitmentRef> decode_commitment_ref(const BoundaryValue&v){CommitmentRef out;dynamic_codec::JsonArchive a(v);auto s=commitment(a,out);if(s.ok)s=a.finish();return s.ok?Outcome<CommitmentRef>{true,std::move(out),{}}:Outcome<CommitmentRef>{false,{},s.error};}
Outcome<BoundaryValue> encode_commitment_ref(const CommitmentRef&v){auto&borrowed=const_cast<CommitmentRef&>(v);dynamic_codec::JsonArchive a;auto s=commitment(a,borrowed);if(s.ok)s=a.finish();return s.ok?Outcome<BoundaryValue>{true,a.value(),{}}:Outcome<BoundaryValue>{false,{},s.error};}
Status visit_commitment_ref_checkpoint(CodecVisitor&a,CommitmentRef&v){return commitment(a,v);}
}
