// P§15.3原使者谈判委任；只保存实际签发和有限条款，不代Task受领、获知或签署。
#pragma once
#include "treaty_revision_sources.hpp"
namespace hanmo::resident {
struct NegotiationMandate {
 std::vector<TreatyTermKind> allowed_term_kinds;ScopeRef scope;
 // 钱与物各自上限，不折价、不拿军务人数当item。其它专项量仍由原term权限核。
 std::vector<TreatyAidItemAmount> quantity_caps;std::optional<TreatyAidMoney> money_cap;
 PassageWindow window;bool may_sign=false;
};
struct TreatyMandateSource {
 StableId id,treaty_id;Integer original_terms_revision=0;SourceRef original_terms_source;
 ActingAuthority issuer;StableId envoy_actor_id;NegotiationMandate mandate;WorldMoment issued_at;
};
// 原作者选择、专项签发范围和历史人物资格是必需真实依赖；不把caller payload当授权。
class TreatyMandateOriginalRead {public:virtual ~TreatyMandateOriginalRead()=default;
 virtual Outcome<SingleGrantAuthorityRequest> read_negotiation_mandate_issue_authority(
  const TreatyRevisionView&,const ActingAuthority&,const StableId& envoy_actor,
  const NegotiationMandate&,const SourceRef* actual_issue_boundary)const=0;
 virtual Status validate_negotiation_mandate_issue_inputs(const TreatyRevisionView&,
  const ActingAuthority&,const StableId& envoy_actor,const NegotiationMandate&,
  const WorldMoment&,const SourceRef* actual_issue_boundary)const=0;
 // 沿真实原释放/死亡/撤权/连续性来源核当前或历史采用；不能只按今日Office反推。
 // 不在本口反读完整Mission/签署，防止消费来源回环。
 virtual Status validate_negotiation_mandate_use(const TreatyMandateSource&,
  const TreatyRevisionView& actual_revision,const WorldMoment& actual_at,
  const SourceRef* actual_use_boundary)const=0;
};
struct TreatyMandateContext {
 const TreatyRevisionContext& revisions;const OrderedTable<TreatyMandateSource>& mandates;
 const TreatyMandateOriginalRead& originals;
};
// 唯一Basis的原签发腿；只读已写真实行，不循环验证自己Basis。
class NativeTreatyMandateIssueRead final:public AuthorityIssueSourceRead {
 const TreatyMandateContext& x_;
public:explicit NativeTreatyMandateIssueRead(const TreatyMandateContext& x):x_(x){}
 Outcome<WorldMoment> read_organization_authority_now()const override;
 Outcome<AuthorityIssueSourceView> read_authority_issue_source(const SourceRef&)const override;
};
SourceRef treaty_mandate_source(const TreatyMandateSource&);
Status validate_negotiation_mandate_shape(const NegotiationMandate&);
Status validate_treaty_mandate_source_shape(const TreatyMandateSource&);
Outcome<const TreatyMandateSource*> read_treaty_mandate_source(const TreatyMandateContext&,const SourceRef&,Integer schema);
Status validate_treaty_mandate_sources(const TreatyMandateContext&);
// 仅核S6原授予范围；调用方还必须反读S3真实Task接受和本人实际同版知情。
// 数量/价格按原term实际责任量投影后比较，不能用may_sign代替其它专项约束。
Status validate_treaty_mandate_use(const TreatyMandateContext&,const SourceRef& mandate,
 Integer schema,const StableId& envoy,const SourceRef& actual_terms,Integer revision,
 const WorldMoment& actual_at,bool signing,const SourceRef* actual_use_boundary=nullptr);
struct IssueTreatyMandateCommand {
 ActingAuthority authority;StableId treaty_id;Integer expected_terms_revision=0;
 SourceRef expected_terms_source;StableId envoy_actor_id;NegotiationMandate mandate;
};
// 原命令request序号承担重放；本采用口只能在失败会撤销整个World候选的签发栈调用。
Outcome<SourceRef> issue_treaty_mandate_in_candidate(CommonStore&,OrderedTable<TreatyMandateSource>&,
 AuthorityBasisRegistry&,const TreatyMandateContext&,const IssueTreatyMandateCommand&);
Status visit_treaty_mandate_source(CodecVisitor&,TreatyMandateSource&);
Status emit_treaty_mandate_source_projection(ProjectionWriter&,std::string_view,const TreatyMandateSource&);
Outcome<BoundaryValue> encode_treaty_mandate_source(const TreatyMandateSource&);
Outcome<TreatyMandateSource> decode_treaty_mandate_source(const BoundaryValue&);
}
