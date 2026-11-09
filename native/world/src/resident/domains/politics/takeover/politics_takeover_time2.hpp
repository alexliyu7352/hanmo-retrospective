// political_takeover/2只给原登记/批准保存真M；AuthorityBasis仍为原schema1。
#pragma once
#include "politics_takeover_rules.hpp"
#include <string_view>
#include "resident/domains/organization/organization_runtime.hpp"
namespace hanmo::resident {
class ProjectionWriter;
// 原签发源读口不调用整takeover validator，避免Basis历史反验自递归。
class NativePoliticalTakeoverAuthorityRead final:public AuthorityIssueSourceRead {
public:
 explicit NativePoliticalTakeoverAuthorityRead(const PoliticalTakeoverContext& c):context_(c){}
 Outcome<WorldMoment> read_organization_authority_now()const override;
 Outcome<AuthorityIssueSourceView> read_authority_issue_source(const SourceRef&)const override;
private:const PoliticalTakeoverContext& context_;
};
SingleGrantAuthorityRequest political_takeover_authority_request(const ActingAuthority&,const PoliticalTakeoverRegistration&,const PoliticalTakeoverRule&);
Outcome<SingleGrantAuthorityRead> read_current_political_takeover_authority(const PoliticalTakeoverContext&,const ActingAuthority&,const PoliticalTakeoverRegistration&,const PoliticalTakeoverRule&);
Status validate_political_takeover_approval_authority2(const PoliticalTakeoverContext&,const PoliticalTakeoverApprovalRecord&);
Outcome<WorldMoment> read_political_takeover_source_moment2(const PoliticalTakeoverContext&,const SourceRef&,Integer source_schema);
Status validate_political_takeover_moments2(const PoliticalTakeoverContext&);
// 显式后继入口，不按payload字段存在与否选择版本。
Status visit_political_takeover_checkpoint2(CodecVisitor&,PoliticalTakeoverState&);
Status emit_political_takeover_state2_projection(ProjectionWriter&,std::string_view,const PoliticalTakeoverState&);
Outcome<BoundaryValue> encode_political_takeover_state2(const PoliticalTakeoverState&);
Outcome<PoliticalTakeoverState> decode_political_takeover_state2(const BoundaryValue&);
Outcome<PoliticalTakeoverCatalog> decode_political_takeover_catalog2(const BoundaryValue&);
Outcome<BoundaryValue> encode_political_takeover_catalog2(const PoliticalTakeoverCatalog&);
}
