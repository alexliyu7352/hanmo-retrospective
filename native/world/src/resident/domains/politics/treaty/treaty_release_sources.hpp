// 原Request/Answer解除来源；本人收到请求、明确答复与实际解除各有独立原源。
#pragma once
#include "treaty_recognition_sources.hpp"
#include "treaty_termination_sources.hpp"
namespace hanmo::resident {
class TreatyReleaseOriginalRead {public:virtual ~TreatyReleaseOriginalRead()=default;
 // 同原已激活条款与签发切点核具体权限；不能由解除payload自己批准或借今日Grant倒证。
 virtual Outcome<SingleGrantAuthorityRequest> read_treaty_release_authority(
  const TreatyActivationView&,const ActingAuthority&,bool answer,const SourceRef* actual_boundary)const=0;
 virtual Status validate_treaty_release_request_inputs(const TreatyActivationView&,
  const ActingAuthority&,const WorldMoment&,const SourceRef* actual_boundary)const=0;
 virtual Status validate_treaty_release_answer_inputs(const TreatyActivationView&,
  const TreatyReleaseRequestSource&,const ActingAuthority&,TreatyReplyDecision,
  const WorldMoment&,const SourceRef* actual_boundary)const=0;
};
struct TreatyReleaseContext {
 const TreatyTerminationContext& treaties;const OrderedTable<TreatyReplyRecord>& replies;
 const TreatyReleaseOriginalRead& originals;
};
class NativeTreatyReleaseIssueRead final:public AuthorityIssueSourceRead {
 const TreatyReleaseContext& x_;
public:explicit NativeTreatyReleaseIssueRead(const TreatyReleaseContext& x):x_(x){}
 Outcome<WorldMoment> read_organization_authority_now()const override;
 Outcome<AuthorityIssueSourceView> read_authority_issue_source(const SourceRef&)const override;
};
SourceRef treaty_release_request_source(const TreatyReleaseRequestSource&);
SourceRef treaty_release_answer_source(const TreatyReleaseAnswerSource&);
Status validate_treaty_release_request_shape(const TreatyReleaseRequestSource&);
Status validate_treaty_release_answer_shape(const TreatyReleaseAnswerSource&);
Outcome<const TreatyReleaseRequestSource*> read_treaty_release_request(const TreatyReleaseContext&,const SourceRef&,Integer schema);
Outcome<const TreatyReleaseAnswerSource*> read_treaty_release_answer(const TreatyReleaseContext&,const SourceRef&,Integer schema);
Status validate_treaty_release_sources(const TreatyReleaseContext&);
// 正文身份只来自真实request和其原activation/完整条款，不复制另一份Terms。
struct TreatyReleaseRequestBody {const TreatyReleaseRequestSource* request=nullptr;TreatyActivationView activation;};
Outcome<OfficialContentRef> read_treaty_release_request_official_content(const TreatyReleaseContext&,const SourceRef&,Integer schema);
Outcome<TreatyReleaseRequestBody> read_treaty_release_request_official_body(const TreatyReleaseContext&,const OfficialContentRef&);
struct RequestTreatyReleaseCommand {ActingAuthority authority;SourceRef activation_source;Integer terms_revision=0;};
struct AnswerTreatyReleaseCommand {ActingAuthority authority;SourceRef request_source;TreatyReplyDecision decision=TreatyReplyDecision::refuse;MomentOwnerSource actual_request_receipt;};
// 只在原命令整体World候选内调用；request序号在原命令层去重，不另增请求账。
Outcome<SourceRef> request_treaty_release_in_candidate(CommonStore&,OrderedTable<TreatyReplyRecord>&,
 AuthorityBasisRegistry&,const TreatyReleaseContext&,const RequestTreatyReleaseCommand&);
Outcome<SourceRef> answer_treaty_release_in_candidate(CommonStore&,OrderedTable<TreatyReplyRecord>&,
 AuthorityBasisRegistry&,const TreatyReleaseContext&,const AnswerTreatyReleaseCommand&);
struct TreatyMutualReleaseReady {SourceRef activation_source;TreatyMutualReleaseTermination cause;WorldMoment agreed_at;};
// 请求方的真实Request是其解除意愿；其余原party须同一请求的accept，拒绝不终止。
Outcome<std::optional<TreatyMutualReleaseReady>> read_treaty_mutual_release_ready(const TreatyReleaseContext&,const SourceRef& actual_request);
// 原termination reader可直接消费；必须逐party/source核，不反入完整termination形成循环。
Status validate_treaty_mutual_release_consent(const TreatyReleaseContext&,const TreatyActivationView&,
 const TreatyMutualReleaseTermination&,const WorldMoment&,const SourceRef* actual_termination_boundary);
struct AnswerAndReleaseTreatyResult {SourceRef answer;std::optional<SourceRef> termination;};
// 最后真实accept与原各term释放/结算同候选；失败撤回整份答复及后果，不留半个终态。
Outcome<AnswerAndReleaseTreatyResult> answer_and_release_treaty_in_candidate(CommonStore&,
 OrderedTable<TreatyReplyRecord>&,AuthorityBasisRegistry&,OrderedTable<TreatyTerminationSource>&,
 const TreatyReleaseContext&,TreatyTerminationOriginalWrite&,const AnswerTreatyReleaseCommand&);
Status visit_treaty_reply_record(CodecVisitor&,TreatyReplyRecord&);
Status emit_treaty_reply_record_projection(ProjectionWriter&,std::string_view,const TreatyReplyRecord&);
Outcome<BoundaryValue> encode_treaty_reply_record(const TreatyReplyRecord&);
Outcome<TreatyReplyRecord> decode_treaty_reply_record(const BoundaryValue&);
}
