// 原双方让职的明确source/2；实际接受只调用唯一Office writer。
#pragma once
#include "political_head_handover_rules.hpp"
namespace hanmo::resident {
Outcome<PoliticalSuccessionOfficeSource> read_political_head_handover_current_office(const PoliticalHeadHandoverContext&,const PoliticalSuccessionPolicy&);
Outcome<WorldMoment> political_head_handover_now(const PoliticalHeadHandoverContext&);
Outcome<WorldMoment> read_political_head_handover_moment2(const PoliticalHeadHandoverContext&,const SourceRef&);
Outcome<bool> political_head_handover_response_open(const PoliticalHeadHandoverContext&,const PoliticalHeadHandoverOffer&,const WorldMoment&);
Status validate_political_head_handover_exact_sources(const PoliticalHeadHandoverContext&);
Outcome<SingleGrantAuthorityRead> read_current_head_handover_authority2(
 const PoliticalHeadHandoverContext&,const PoliticalHeadHandoverPolicy&,const PoliticalSuccessionPolicy&,
 const ActingAuthority&,const StableId& command);
Status validate_head_handover_authority2(const PoliticalHeadHandoverContext&,
 const PoliticalHeadHandoverPolicy&,const PoliticalSuccessionPolicy&,const StableId& source);
Status validate_head_handover_office2(const PoliticalHeadHandoverContext&,
 const PoliticalSuccessionPolicy&,const StableId& actor,Integer epoch,const StableId& boundary);
Status validate_head_handover_person2(const PoliticalHeadHandoverContext&,const StableId& actor,const StableId& boundary);
class NativeHeadHandoverAuthorityRead final:public AuthorityIssueSourceRead {
 const CommonStore& common_;const PoliticalHeadHandoverState& state_;const OrganizationAuthorityClock& clock_;
public:
 NativeHeadHandoverAuthorityRead(const CommonStore& c,const PoliticalHeadHandoverState& s,const OrganizationAuthorityClock& k):common_(c),state_(s),clock_(k){}
 Outcome<WorldMoment> read_organization_authority_now()const override{return clock_.read_organization_authority_now();}
 Outcome<AuthorityIssueSourceView> read_authority_issue_source(const SourceRef&)const override;
};
class NativeHeadHandoverOfficeRead final:public OfficeTransitionSourceRead {
 const PoliticalHeadHandoverContext& context_;
public:
 explicit NativeHeadHandoverOfficeRead(const PoliticalHeadHandoverContext& x):context_(x){}
 Outcome<VerifiedOfficeTransition> read_office_transition(const SourceRef&,const ReadStamp&)const override;
};
// 只供原apply的候选尾；接受源已经真实写入，返回唯一Office结果后再补本答复引用。
Outcome<OfficeChangeResult> apply_head_handover_office2(CommonStore&,OrganizationState&,
 OrganizationRuntime&,PoliticalHeadHandoverState&,const PoliticalHeadHandoverContext&,const StableId& accepted_source);
Outcome<StableId> apply_political_head_handover2_in_candidate(CommonStore&,OrganizationState&,
 OrganizationRuntime&,PoliticalHeadHandoverState&,const PoliticalHeadHandoverContext&,const StableId&,
 const PoliticalHeadHandoverCommand&,const PoliticalHeadHandoverQuote&);
Status visit_political_head_handover2_checkpoint(CodecVisitor&,PoliticalHeadHandoverState&);
Outcome<BoundaryValue> encode_political_head_handover2_state(const PoliticalHeadHandoverState&);
Outcome<PoliticalHeadHandoverState> decode_political_head_handover2_state(const BoundaryValue&);
Status emit_political_head_handover2_projection(ProjectionWriter&,std::string_view,const PoliticalHeadHandoverState&);
}
