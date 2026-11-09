// 07§5.2公有营业和用途的唯一S6批准历史；不保存市场当前容量、劳力或重复AuthorityBasis。
#pragma once
#include "resident/domains/organization/organization_runtime.hpp"
#include "resident/domains/local_economy/public_market_authority_read.hpp"
namespace hanmo::resident {
inline constexpr const char* public_market_authority_module="public_market_authority";
enum class PublicMarketAuthorityDecision {issued,revised,ended};
// 所有权限和可接受时间窗必须来自锁内作者输入；没有默认政策或隐式全员营业许可。
struct PublicMarketAuthorityPolicy {
 ActorFoundationRuleRef policy_ref,market_definition_ref,opening_rule_ref,usage_rule_ref;
 StableId organization_id,asset_id;
 SourceRef original_asset_initialization_ref,original_asset_right_ref,original_usage_right_ref;
 std::vector<MomentSpan> allowed_opening_intervals,allowed_usage_intervals;
};
struct PublicMarketAuthorityCatalog {CatalogIdentity identity;OrderedTable<PublicMarketAuthorityPolicy> policies;};
struct PublicMarketAuthorityRecord {
 StableId id;ActorFoundationRuleRef policy_ref,market_definition_ref;
 Integer revision=0;std::optional<SourceRef> previous_source_ref;
 PublicMarketAuthorityDecision decision=PublicMarketAuthorityDecision::issued;
 ActingAuthority issuer;
 SourceRef original_asset_initialization_ref,original_asset_right_ref,original_usage_right_ref;
 ActorFoundationRuleRef opening_rule_ref,usage_rule_ref;
 std::vector<MomentSpan> committed_opening_intervals,valid_usage_intervals;
 AuthorityBasisRef authority_basis_ref;
 WorldMoment issued_at;
};
// 唯一根严格schema_version,authorizations；当前行只沿真实原事实追加顺序派生。
struct PublicMarketAuthorityState {OrderedTable<PublicMarketAuthorityRecord> authorizations;};
class PublicMarketAuthorityOriginalRead {public:virtual ~PublicMarketAuthorityOriginalRead()=default;
 virtual Status validate_public_market_policy(const PublicMarketAuthorityPolicy&,const MarketServiceDefinition&,const CatalogIdentity&)const=0;
 // historical_before为空只供当前签发真检；冷读必须反读当时原asset/right，而非今日产权。
 virtual Status validate_public_market_original_rights(const PublicMarketAuthorityPolicy&,const WorldMoment&,const std::optional<SourceRef>& historical_before)const=0;
 // 非本段的实际staffing/condition变化必须由其原owner回核，不能只凭Common头猜时刻。
 virtual Outcome<WorldMoment> read_public_market_external_adoption_moment(const SourceRef&)const=0;
};
struct PublicMarketAuthorityContext {
 const CommonStore& common;const OrganizationState& organization;const OrganizationCatalog& organization_catalog;const OrganizationRuntime& organization_runtime;
 const PublicMarketAuthorityState& state;const PublicMarketAuthorityCatalog& catalog;
 const std::vector<MarketServiceDefinition>& definitions;
 const PublicMarketAuthorityOriginalRead& originals;const OrganizationAuthorityClock& clock;
};
class NativePublicMarketAuthorityRead final:public PublicMarketAuthorityRead,public AuthorityIssueSourceRead {
public:explicit NativePublicMarketAuthorityRead(PublicMarketAuthorityContext x):context_(x){}
 Outcome<MarketServiceChangeCause> read_public_market_service_change(const MarketServiceDefinition&,const FacilitySourceRef&)const override;
 Outcome<PublicMarketAuthorityView> read_public_market_authority_at_adoption(const MarketServiceDefinition&,const SourceRef&,const WorldMoment&)const override;
 Outcome<WorldMoment> read_public_market_authority_change_moment(const SourceRef&)const override;
 Status validate_public_market_authority_history(const MarketServiceDefinition&,const PublicMarketAuthorityView&,const SourceRef&,const WorldMoment&)const override;
 Outcome<WorldMoment> read_organization_authority_now()const override;
 Outcome<AuthorityIssueSourceView> read_authority_issue_source(const SourceRef&)const override;
private:PublicMarketAuthorityContext context_;
};
struct PublicMarketAuthorityCommand {
 StableId policy_id;Integer expected_revision=0;
 PublicMarketAuthorityDecision decision=PublicMarketAuthorityDecision::issued;
 ActingAuthority authority;
 std::vector<MomentSpan> committed_opening_intervals,valid_usage_intervals;
};
// 新批准真实改变营业/用途后，S5必须同候选处理原市场容量及所有受影响claims；技术失败全候选撤回。
class PublicMarketAuthorityEffectsWrite {public:virtual ~PublicMarketAuthorityEffectsWrite()=default;virtual Status apply_public_market_authority_change_in_candidate(const SourceRef&,const WorldMoment&)=0;};
SourceRef public_market_authority_source(const PublicMarketAuthorityRecord&);
Status validate_public_market_authority_catalog(const PublicMarketAuthorityContext&);
Status validate_public_market_authority(const PublicMarketAuthorityContext&);
Outcome<SourceRef> apply_public_market_authority_in_candidate(CommonStore&,OrganizationRuntime&,PublicMarketAuthorityState&,const PublicMarketAuthorityContext&,PublicMarketAuthorityEffectsWrite&,const PublicMarketAuthorityCommand&);
Status visit_public_market_authority(CodecVisitor&,PublicMarketAuthorityState&);
Status emit_public_market_authority_projection(ProjectionWriter&,std::string_view,const PublicMarketAuthorityState&);
Outcome<BoundaryValue> encode_public_market_authority(const PublicMarketAuthorityState&);
Outcome<PublicMarketAuthorityState> decode_public_market_authority(const BoundaryValue&);
Outcome<BoundaryValue> encode_public_market_authority_catalog(const PublicMarketAuthorityCatalog&);
Outcome<PublicMarketAuthorityCatalog> decode_public_market_authority_catalog(const BoundaryValue&,const CatalogIdentity& admitted_identity);
}
