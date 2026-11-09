// 财政政策本域保存历史决定；实际税粮/欠期/民用留粮仍由经济域结算。
#pragma once
#include "politics_model.hpp"
#include "politics_fiscal_contract.hpp"
namespace hanmo::resident {
struct PoliticsViewIndex;
// 显式能力开关不带默认税政，启用后也须先有真实有权者的政策变更事实。
struct PoliticalFiscalCatalog { bool enabled=false; Integer version=1; };
struct FiscalPolicyRecord {
    StableId id,issuer_actor_id,issuer_office_id,grant_id;
    Integer authority_epoch=0;
    FiscalPolicySnapshot snapshot;
    // /2唯一真签发时刻；/1不编码且必须为空。税期仍以原Calendar整界选择。
    std::optional<WorldMoment> issued_at;
};
// 全部记录不可变；查询按责任due_tick选择当时政策，不覆盖旧欠期依据。
struct PoliticalFiscalState { OrderedTable<FiscalPolicyRecord> policies; Integer version=1; };
struct PoliticalFiscalTime2Context;
struct PoliticalSetTaxCommand {
    ActingAuthority authority;
    StableId jurisdiction_id,public_inventory_id;
    Integer expected_jurisdiction_revision=0,expected_policy_revision=0;
    FiscalTaxMode mode=FiscalTaxMode::normal;
};
struct PoliticalFiscalEffects { StableId policy_fact_id; Integer policy_revision=0,effective_from_tick=0; };
// 报价冻结同一世界身份与完整税政选择；不把政策倍率显示成已收收入。
struct PoliticalFiscalQuote { WorldIdentity identity; StableId actor_id; PoliticalSetTaxCommand command; Integer duration=0; std::vector<ReadVersion> resource_versions; };
// 该实现引用同一政治辖区和共同事实，不从配置金额生成一份可花收入。
class FiscalPolicyAccess final:public FiscalPolicyRead {
public:
    FiscalPolicyAccess(const CommonStore&,const CommonCatalog&,const PoliticsState&,const PoliticalFiscalState&,const PoliticalFiscalCatalog&,const FiscalPolicyOccupationRead* occupation=nullptr);
    // 世界单步内Common.tick仍为from，财政可显式求值至经核准的to；普通查询不获得未来读权。
    FiscalPolicyAccess(const CommonStore&,const CommonCatalog&,const PoliticsState&,const PoliticalFiscalState&,const PoliticalFiscalCatalog&,const DynamicStepInput&,const FiscalPolicyOccupationRead* occupation=nullptr);
    // 完整/2由同候选借用真实历史源；context与本reader同寿命。
    explicit FiscalPolicyAccess(const PoliticalFiscalTime2Context&);
    Outcome<std::optional<FiscalPolicySnapshot>> fiscal_policy_for_due_tick(const StableId&,Integer)const override;
    Outcome<std::optional<FiscalPolicySnapshot>> fiscal_policy_for_assessment_source(const StableId&,Integer,const StableId&)const override;
private:
    const CommonStore& common_; const CommonCatalog& common_catalog_; const PoliticsState& politics_;
    const PoliticalFiscalState& state_; const PoliticalFiscalCatalog& catalog_;
    std::optional<std::pair<Integer,Integer>> phase_boundary_;
    const FiscalPolicyOccupationRead* occupation_=nullptr;
    const PoliticalFiscalTime2Context* exact_=nullptr;
    Outcome<std::optional<FiscalPolicySnapshot>> select_policy(const StableId&,Integer,const StableId* historical_source)const;
};
Status preview_political_set_tax(const CommonStore&,const PoliticsState&,const PoliticalFiscalState&,const PoliticalFiscalCatalog&,const OrganizationState&,const OrganizationCatalog&,const ActivityRead&,const StableId& actor,const PoliticalSetTaxCommand&);
Outcome<PoliticalFiscalEffects> apply_political_set_tax(CommonStore&,const PoliticsState&,PoliticalFiscalState&,const PoliticalFiscalCatalog&,const OrganizationState&,const OrganizationCatalog&,const ActivityRead&,const StableId& actor,const PoliticalSetTaxCommand&);
// 调用方持唯一候选/子事务并负责失败回滚，内层不得开启第二savepoint。
Outcome<PoliticalFiscalEffects> apply_political_set_tax_in_transaction(CommonStore&,const PoliticsState&,PoliticalFiscalState&,const PoliticalFiscalCatalog&,const OrganizationState&,const OrganizationCatalog&,const ActivityRead&,const StableId& actor,const PoliticalSetTaxCommand&);
Status validate_political_fiscal(const CommonStore&,const CommonCatalog&,const PoliticsState&,const PoliticalFiscalState&,const PoliticalFiscalCatalog&,const OrganizationState&,const OrganizationCatalog&,const FiscalPolicyOccupationRead* occupation=nullptr);
std::optional<DynamicFactView> political_fiscal_dynamic_fact(const PoliticalFiscalState&,const StableId&);
BoundaryValue encode_political_fiscal_state(const PoliticalFiscalState&);
Outcome<PoliticalFiscalState> decode_political_fiscal_state(const BoundaryValue&);
Status visit_political_fiscal_checkpoint(CodecVisitor&,PoliticalFiscalState&);
BoundaryValue encode_political_fiscal_catalog(const PoliticalFiscalCatalog&);
Outcome<PoliticalFiscalCatalog> decode_political_fiscal_catalog(const BoundaryValue&);
BoundaryValue encode_political_set_tax(const PoliticalSetTaxCommand&);
Outcome<PoliticalSetTaxCommand> decode_political_set_tax(const BoundaryValue&);
// 同原字段writer，只发当前object字段，不构造中间树。
class ProjectionWriter;
Status emit_political_fiscal_effects_fields(ProjectionWriter&,const PoliticalFiscalEffects&);
BoundaryValue encode_political_fiscal_effects(const PoliticalFiscalEffects&);
Outcome<PoliticalFiscalEffects> decode_political_fiscal_effects(const BoundaryValue&);
Status visit_political_fiscal_effects(CodecVisitor&,PoliticalFiscalEffects&);
BoundaryValue encode_political_fiscal_quote(const PoliticalFiscalQuote&);
Outcome<PoliticalFiscalQuote> decode_political_fiscal_quote(const BoundaryValue&);
// 当前有权者须亲在治所查本地公文；此读口不开放全国政策或公共余额。
Outcome<BoundaryValue> query_political_fiscal_entries(const CommonStore&,const CommonCatalog&,const PoliticsState&,const PoliticalFiscalState&,const PoliticalFiscalCatalog&,const OrganizationState&,const OrganizationCatalog&,const ActivityRead&,const StableId& actor,const StableId& site,const PoliticsViewIndex* index=nullptr);
}
