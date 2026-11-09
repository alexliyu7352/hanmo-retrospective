// 原财政/2：同一政策行尾真Moment、唯一原AuthorityBasis；不改变税档或下一Calendar界。
#pragma once
#include "politics_fiscal.hpp"
#include <string_view>
#include "resident/domains/organization/organization_runtime.hpp"
#include "resident/domains/family/life_age.hpp"
namespace hanmo::resident {
struct PoliticalTakeoverContext;
struct PoliticalFiscalTime2Context {
 const CommonStore& common;
 const CommonCatalog& common_catalog;
 const PoliticsState& politics;
 const PoliticalFiscalState& state;
 const PoliticalFiscalCatalog& catalog;
 const OrganizationState& organizations;
 const OrganizationCatalog& organization_catalog;
 const OrganizationRuntime& runtime;
 const OrganizationAuthorityClock& clock;
 const LifeAgeSources& ages;
 const FiscalPolicyOccupationRead* occupation=nullptr;
 // 选接管来源时必须同批借入原context，保旧税政不得在接管后重新签发之门。
 const PoliticalTakeoverContext* takeover=nullptr;
};
class NativePoliticalFiscalAuthorityRead final:public AuthorityIssueSourceRead {
 const PoliticalFiscalTime2Context& context_;
public:
 explicit NativePoliticalFiscalAuthorityRead(const PoliticalFiscalTime2Context& context):context_(context){}
 Outcome<WorldMoment> read_organization_authority_now()const override;
 Outcome<AuthorityIssueSourceView> read_authority_issue_source(const SourceRef&)const override;
};
Outcome<PoliticalFiscalState> initialize_political_fiscal2(const PoliticalFiscalCatalog&);
Status preview_political_set_tax2(const PoliticalFiscalTime2Context&,const ActivityRead&,
 const StableId& actor,const PoliticalSetTaxCommand&);
// 只供外层唯一完整World候选使用。Common、state、runtime必须与context同一对象；
// 任一错误丢弃全部候选，不能只回滚财政行而遗留Common头/AuthorityBasis。
Outcome<PoliticalFiscalEffects> apply_political_set_tax2_in_candidate(
 CommonStore&,PoliticalFiscalState&,OrganizationRuntime&,const PoliticalFiscalTime2Context&,
 const ActivityRead&,const StableId& actor,const PoliticalSetTaxCommand&);
Outcome<WorldMoment> read_political_fiscal_source_moment2(const PoliticalFiscalTime2Context&,
 const SourceRef&,Integer source_schema);
Status validate_political_fiscal_occupation_boundary2(const PoliticalFiscalTime2Context&,const FiscalPolicyOccupationBoundary&);
Status validate_political_fiscal_record2(const PoliticalFiscalTime2Context&,const FiscalPolicyRecord&);
Status validate_political_fiscal2(const PoliticalFiscalTime2Context&);
// /2显式物理入口；/1绝不猜迁移。Catalog的作者字段仍仅enabled，由选择器锁version2。
Status visit_political_fiscal_checkpoint2(CodecVisitor&,PoliticalFiscalState&);
Status emit_political_fiscal_state2_projection(ProjectionWriter&,std::string_view,const PoliticalFiscalState&);
Outcome<BoundaryValue> encode_political_fiscal_state2(const PoliticalFiscalState&);
Outcome<PoliticalFiscalState> decode_political_fiscal_state2(const BoundaryValue&);
Outcome<PoliticalFiscalCatalog> decode_political_fiscal_catalog2(const BoundaryValue&);
Status emit_political_fiscal_catalog2_projection(ProjectionWriter&,std::string_view,const PoliticalFiscalCatalog&);
Outcome<BoundaryValue> encode_political_fiscal_catalog2(const PoliticalFiscalCatalog&);
}
