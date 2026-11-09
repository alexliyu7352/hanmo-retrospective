// 税政由政治域决定；本叶只把具名税源的真实钱粮转到原公共容器，不持第二份余额。
#pragma once
#include "resident/domains/local_economy/local_economy_model.hpp"
#include "resident/domains/politics/politics_fiscal_contract.hpp"
namespace hanmo::resident {
struct EconomicTakeoverReadContext;
// taxpayer仅允许实际cohort或organization；Actor私人钱包绝不作为财政不足的补贴来源。
struct FiscalTaxSource {
    StableId id;
    SubjectRef taxpayer;
    StableId wallet_id,inventory_id,grain_item_id;
    std::vector<StableId> production_unit_ids;
    Integer base_cash_due=0,grain_share_bp=0,civilian_grain_reserve=0;
};
// 周期与保留量必须由作者明确给出；normal_recovery_bp是明示开发参数，不冒充GDD固定数值。
struct FiscalDistrictProfile {
    StableId id,site_id;
    Integer period_ticks=1,first_due_tick=1,initial_tax_base_bp=10000,normal_recovery_bp=100;
    OrderedTable<FiscalTaxSource> sources;
};
struct FiscalSettlementCatalog {Integer version=1;OrderedTable<FiscalDistrictProfile> districts;};
// 初始来源有真实事实头；地区进度和有理余数不替代原账户资产。
struct FiscalDistrictProgress {StableId id,registration_fact_id;Integer next_due_tick=0,tax_base_bp=10000;};
struct FiscalSourceRemainders {Integer cash=0,grain=0;};
// 每辖区每期一个结果供S6消费民心，避免有多户税源就重复施加同一期政策后果。
struct FiscalPeriodAssessment {
    StableId id,district_id,registration_fact_id;
    Integer period_start_tick=0,due_tick=0,tax_base_bp=10000;
    std::optional<FiscalPolicySnapshot> policy;
    std::vector<StableId> source_assessment_ids;
};
// 两种欠额沿原ArrearRecord整数结构，只各自描述钱/粮应付；其paid_amount从实际支付收据求证。
struct FiscalSourceAssessment {
    StableId id,period_id,source_id,wallet_id,inventory_id,grain_item_id;
    Owner owner;
    Integer produced_grain=0,civilian_grain_reserve=0;
    Integer cash_remainder_before=0,grain_remainder_before=0,cash_remainder_after=0,grain_remainder_after=0;
    std::vector<StableId> production_receipt_ids;
    ArrearRecord cash,grain;
};
// 支付冻结当时源/目的和数额；以后改税政不能重写旧欠期或再次全额收费。
struct FiscalPaymentReceipt {StableId id,assessment_id;Integer tick=0,cash_paid=0,grain_paid=0;};
struct FiscalSettlementState {
    Integer version=1,initialized_tick=0,last_step_tick=0;
    OrderedTable<FiscalDistrictProgress> districts;
    OrderedTable<FiscalSourceRemainders> remainders;
    OrderedTable<FiscalPeriodAssessment> periods;
    OrderedTable<FiscalSourceAssessment> assessments;
    OrderedTable<FiscalPaymentReceipt> payments;
};
// S6只消费完整评税事实；无政策的空期不会成为加减民心的来源。
struct FiscalAssessmentSummary {
    StableId fact_id,jurisdiction_id,site_id,organization_id,policy_fact_id;
    Integer due_tick=0;
    FiscalTaxMode mode=FiscalTaxMode::normal;
    bool assessment_completed=false;
};
Status validate_fiscal_sources(const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,const OrganizationRead&,const FiscalPolicyRead&,const FiscalSettlementCatalog&);
Status validate_fiscal_sources(const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,const OrganizationRead&,const FiscalPolicyRead&,const FiscalSettlementCatalog&,const EconomicTakeoverReadContext* takeover);
Outcome<FiscalSettlementState> make_initial_fiscal_settlement(CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,const OrganizationRead&,const FiscalPolicyRead&,const FiscalSettlementCatalog&);
// 接管登记只保原债并暂停收款；债权让与未获明确合同支持时不向新Owner收费。
// 必须在原地方生产完成后、同一半日候选内调用；政策read由中央绑定该阶段的to_tick。
Outcome<LocalEconomyEffects> step_fiscal_settlement(CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,const OrganizationRead&,const FiscalPolicyRead&,const FiscalSettlementCatalog&,FiscalSettlementState&,const DynamicStepInput&);
Outcome<LocalEconomyEffects> step_fiscal_settlement(CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,const OrganizationRead&,const FiscalPolicyRead&,const FiscalSettlementCatalog&,FiscalSettlementState&,const DynamicStepInput&,const EconomicTakeoverReadContext* takeover);
Status validate_fiscal_settlement(const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,const OrganizationRead&,const FiscalPolicyRead&,const FiscalSettlementCatalog&,const FiscalSettlementState&);
Status validate_fiscal_settlement(const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,const OrganizationRead&,const FiscalPolicyRead&,const FiscalSettlementCatalog&,const FiscalSettlementState&,const EconomicTakeoverReadContext* takeover);
std::optional<DynamicFactView> fiscal_settlement_dynamic_fact(const FiscalSettlementState&,const FiscalSettlementCatalog&,const StableId& fact_id);
std::optional<FiscalAssessmentSummary> fiscal_assessment_result(const CommonStore&,const FiscalSettlementState&,const StableId& fact_id);
// 独立模块冷入口只保存税源责任与真实收据，旧local_economy布局不隐式增加财政字段。
Outcome<FiscalSettlementCatalog> decode_fiscal_settlement_catalog(const BoundaryValue&);
class ProjectionWriter;
Status emit_fiscal_settlement_state_fields(ProjectionWriter&,const FiscalSettlementState&);
BoundaryValue encode_fiscal_settlement_state(const FiscalSettlementState&);
Outcome<FiscalSettlementState> decode_fiscal_settlement_state(const BoundaryValue&);
Status visit_fiscal_settlement_checkpoint(CodecVisitor&,FiscalSettlementState&);
}
