// Original M-TREATY presentations and replies. Actual receipt, an expressed
// answer, ratification and objective activation remain distinct source kinds.
#pragma once
#include "treaty_official_content.hpp"
namespace hanmo::resident {
struct TreatyPresentationSource {
 StableId id,treaty_id;Integer terms_revision=0;SourceRef terms_source;
 ActingAuthority recipient;MomentOwnerSource actual_content_receipt;WorldMoment presented_at;
};
enum class TreatyReplyDecision {accept,refuse};
struct TreatyReplySource {
 StableId id,treaty_id;Integer terms_revision=0;SourceRef terms_source,presentation_source;
 ActingAuthority respondent;TreatyReplyDecision decision=TreatyReplyDecision::refuse;WorldMoment replied_at;
};
// 同一replies根显式三种原操作；初次认可不解释为解除同意。
struct TreatyReleaseRequestSource {
 StableId id,treaty_id;Integer terms_revision=0;SourceRef activation_source,terms_source;
 ActingAuthority requester;WorldMoment requested_at;
};
struct TreatyReleaseAnswerSource {
 StableId id,treaty_id;Integer terms_revision=0;SourceRef activation_source,request_source;
 ActingAuthority respondent;TreatyReplyDecision decision=TreatyReplyDecision::refuse;
 MomentOwnerSource actual_request_receipt;WorldMoment answered_at;
};
using TreatyReplyRecord=std::variant<TreatyReplySource,TreatyReleaseRequestSource,TreatyReleaseAnswerSource>;
class TreatyRecognitionOriginalRead {public:virtual ~TreatyRecognitionOriginalRead()=default;
 // Actual original policy/control defines the narrow receiving/responding
 // scope. A caller cannot submit an arbitrary permission list or quote.
 virtual Outcome<SingleGrantAuthorityRequest> read_original_treaty_recognition_authority(
  const TreatyRevisionView&,const ActingAuthority&,bool reply,
  const SourceRef* actual_boundary)const=0;
 // Reads original negotiation disposition and authentic presented/replied
 // source, without calling the AuthorityBasis reader recursively.
 virtual Status validate_original_treaty_reply(const TreatyRevisionView&,
  const TreatyPresentationSource&,const ActingAuthority&,TreatyReplyDecision,
  const WorldMoment&,const SourceRef* actual_boundary)const=0;
};
struct TreatyRecognitionContext {
 const TreatyRevisionContext& revisions;
 const OrderedTable<TreatyPresentationSource>& presentations;const OrderedTable<TreatyReplyRecord>& replies;
 const TreatyRecognitionOriginalRead& originals;
 const SocialState& social;const CommunicationLinksState& communications;
 const CommunicationSourceRead& communication_sources;const ExactResultTimeRead& times;
 const ExactResultReadStamp& result_stamp;
};
class NativeTreatyRecognitionIssueRead final:public AuthorityIssueSourceRead {
 const TreatyRecognitionContext& context_;
public:explicit NativeTreatyRecognitionIssueRead(const TreatyRecognitionContext& c):context_(c){}
 Outcome<WorldMoment> read_organization_authority_now()const override;
 Outcome<AuthorityIssueSourceView> read_authority_issue_source(const SourceRef&)const override;
};
SourceRef treaty_presentation_source(const TreatyPresentationSource&);
SourceRef treaty_reply_source(const TreatyReplySource&);
Status validate_treaty_presentation_shape(const TreatyPresentationSource&);
Status validate_treaty_reply_shape(const TreatyReplySource&);
Outcome<const TreatyPresentationSource*> read_treaty_presentation_source(const TreatyRecognitionContext&,const SourceRef&,Integer schema);
Outcome<const TreatyReplySource*> read_treaty_reply_source(const TreatyRecognitionContext&,const SourceRef&,Integer schema);
Status validate_treaty_recognition_sources(const TreatyRecognitionContext&);
struct PresentTreatyTermsCommand {ActingAuthority recipient;StableId treaty_id;Integer terms_revision=0;SourceRef terms_source;MomentOwnerSource actual_content_receipt;};
struct ReplyTreatyTermsCommand {ActingAuthority authority;SourceRef presentation_source;TreatyReplyDecision decision=TreatyReplyDecision::refuse;};
// Called in the same actual official-disclosure candidate, after real receipt.
// A later person/Office cannot retroactively adopt someone else's disclosure.
Outcome<SourceRef> present_received_treaty_terms_in_candidate(CommonStore&,OrderedTable<TreatyPresentationSource>&,
 AuthorityBasisRegistry&,const TreatyRecognitionContext&,const PresentTreatyTermsCommand&);
Outcome<SourceRef> reply_treaty_terms_in_candidate(CommonStore&,OrderedTable<TreatyReplyRecord>&,
 AuthorityBasisRegistry&,const TreatyRecognitionContext&,const ReplyTreatyTermsCommand&);
Status visit_treaty_presentation_source(CodecVisitor&,TreatyPresentationSource&);
Status visit_treaty_reply_source(CodecVisitor&,TreatyReplySource&);
Status emit_treaty_presentation_source_projection(ProjectionWriter&,std::string_view,const TreatyPresentationSource&);
Status emit_treaty_reply_source_projection(ProjectionWriter&,std::string_view,const TreatyReplySource&);
Outcome<BoundaryValue> encode_treaty_presentation_source(const TreatyPresentationSource&);
Outcome<TreatyPresentationSource> decode_treaty_presentation_source(const BoundaryValue&);
Outcome<BoundaryValue> encode_treaty_reply_source(const TreatyReplySource&);
Outcome<TreatyReplySource> decode_treaty_reply_source(const BoundaryValue&);
}
