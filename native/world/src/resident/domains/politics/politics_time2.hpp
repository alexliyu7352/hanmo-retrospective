// 原政治事实/2唯一精确时刻；原Calendar期限保持不变。
#pragma once
#include "politics_organization_runtime.hpp"
#include "politics_model.hpp"
#include "politics_actor_qualification.hpp"
namespace hanmo::resident {
class NativePoliticsAuthorityRead final:public AuthorityIssueSourceRead {
public:NativePoliticsAuthorityRead(const CommonStore& c,const PoliticsState& s,const OrganizationAuthorityClock& clock):common_(c),state_(s),clock_(clock){}
 Outcome<WorldMoment> read_organization_authority_now()const override;Outcome<AuthorityIssueSourceView> read_authority_issue_source(const SourceRef&)const override;
private:const CommonStore& common_;const PoliticsState& state_;const OrganizationAuthorityClock& clock_;
};
Status validate_politics_time2_authorities(const CommonStore&,const PoliticsState&,const OrganizationState&,const OrganizationCatalog&,const OrganizationRuntime&,const OrganizationAuthorityClock&);
Outcome<WorldMoment> read_politics_source_moment(const CommonStore&,const PoliticsState&,const OrganizationRuntime&,const SourceRef&,Integer source_schema,const OrganizationAuthorityClock&);
Status validate_politics_time2(const CommonStore&,const PoliticsState&,const OrganizationRuntime&,const OrganizationAuthorityClock&);
Status visit_politics2(CodecVisitor&,PoliticsState&);
Status emit_politics2_projection(ProjectionWriter&,std::string_view,const PoliticsState&);
Outcome<BoundaryValue> encode_politics2(const PoliticsState&);
Outcome<PoliticsState> decode_politics2(const BoundaryValue&);
}
