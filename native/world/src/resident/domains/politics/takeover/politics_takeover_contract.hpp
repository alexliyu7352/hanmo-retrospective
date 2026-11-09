// 接管批准只引用军事原占领、原经济责任与真实Office授权；不转资产或复制欠账。
#pragma once
#include "resident/domains/organization/organization_model.hpp"
namespace hanmo::resident {
inline constexpr const char* political_takeover_command_kind = "political_assume_public_responsibilities";
inline constexpr const char* political_takeover_fact_kind = "political_public_responsibilities_assumed";
// 配置必须显式给出；20半日/5000bp只可来自采用该历史参数的作者规则。
struct PoliticalOccupationRuleSource {
    StableId rule_id;
    Integer rule_version = 0, integration_ticks = 0, income_basis_points = 0;
};
// 必须是S2已实际执行资产接管的原事实。因果可见性由原事实读口核实，不由tick推先后。
struct PoliticalTakeoverSource {
    WorldIdentity source_identity;
    StableId fact_id, occupation_fact_id, jurisdiction_id, site_id;
    StableId former_organization_id, successor_organization_id;
    ActingAuthority authority;
    StableId grant_id;
    Integer occupation_tick = 0, effective_tick = 0;
    std::vector<StableId> public_wallet_ids, public_inventory_ids;
    PoliticalOccupationRuleSource occupation_rule;
};
enum class PoliticalResponsibilityKind {
    production_unit, production_asset_use, pending_configuration,
    obligation, arrear, fiscal_assessment, relief_contract, supply_source, task_contract
};
// 设施使用按真实unit与asset组合引用；共享设施不能被误当成单一经营者的责任。
struct PoliticalResponsibilityRef {
    PoliticalResponsibilityKind kind = PoliticalResponsibilityKind::obligation;
    StableId responsibility_id;
    std::optional<StableId> unit_id, asset_id;
};
struct PoliticalResponsibilityAssetSource {
    enum class Kind { wallet, inventory, production_unit, production_asset };
    Kind kind = Kind::wallet;
    StableId id;
    Owner frozen_owner;
};
// settlement_required保留未清责任；它不是付款、免债或任务完成的事实。
enum class PoliticalResponsibilityDisposition {
    continue_under_original, suspend, assume_by_successor, settlement_required
};
// 保留实际账户角色；两个角色共用同一ID也不能合并成任意退款去向。
struct PoliticalExecutionAccount {
    StableId role,original_container_id,execution_container_id;
    bool inventory=false;
};
struct PoliticalResponsibilityDecision {
    StableId decision_fact_id, takeover_fact_id;
    PoliticalResponsibilityRef responsibility;
    PoliticalResponsibilityDisposition disposition = PoliticalResponsibilityDisposition::suspend;
    // 原债权债务主体永久保留；当前执行主体由明确决定另行引用。
    std::optional<SubjectRef> original_principal;
    std::vector<SubjectRef> original_beneficiaries;
    // 任务的受托执行人与货物受益人分列；领酬资格不能代替原交付对象。
    std::optional<SubjectRef> original_assignee;
    std::vector<SubjectRef> original_delivery_beneficiaries;
    std::optional<SubjectRef> execution_principal;
    std::vector<SubjectRef> collection_beneficiaries;
    // SubjectRef保实际cohort等责任主体；资产Owner不承担债权或收受同意的含义。
    std::vector<PoliticalResponsibilityAssetSource> original_assets;
    std::optional<StableId> payment_wallet_id, receipt_wallet_id;
    std::optional<StableId> payment_inventory_id, receipt_inventory_id;
    // 生产经营/设施使用逐原ID授权；不能仅凭输出库已转属推导经营权。
    std::vector<StableId> production_asset_ids;
    // Task仅派生funding/reward同ID映射；完整实际支付由原任务域核验并执行。
    std::vector<PoliticalExecutionAccount> execution_accounts;
};
struct PoliticalTakeoverApproval {
    PoliticalTakeoverSource source;
    std::vector<PoliticalResponsibilityDecision> decisions;
};
// 未裁定与当前暂停都保留原责任；只有真实决定才带decision，不伪造暂停事实。
enum class PoliticalResponsibilityExecutionStatus { pending, approved, suspended };
// 登记来源只证明实际军事接管及其范围；没有批准时不填假Authority或Grant。
struct PoliticalTakeoverRegistrationSource {
    WorldIdentity source_identity;
    StableId registration_fact_id,occupation_fact_id,jurisdiction_id,site_id;
    StableId former_organization_id,successor_organization_id;
    Integer occupation_tick=0;
    std::vector<StableId> public_wallet_ids,public_inventory_ids;
    PoliticalOccupationRuleSource occupation_rule;
};
struct PoliticalResponsibilityExecution {
    StableId registration_fact_id;
    PoliticalResponsibilityRef responsibility;
    PoliticalTakeoverRegistrationSource registration;
    PoliticalResponsibilityExecutionStatus status = PoliticalResponsibilityExecutionStatus::pending;
    std::optional<PoliticalResponsibilityDecision> decision;
    StableId code;
    // 从原登记派生的真实来源ID；不增加任何持久化字段。
    std::vector<StableId> original_source_fact_ids;
};
// 内部读口先核真实S2已完成清单、完整逐责任覆盖及政治Office/Grant历史。
// 不是玩家任意ID查询。历史读不以今日Owner改写原principal/beneficiary。
// 仅适用于上述军事接管；私人死亡/继承或无军事占领的组织解散不能借填ID套用。
class PoliticalTakeoverRead {
public:
    virtual ~PoliticalTakeoverRead() = default;
    virtual Outcome<PoliticalTakeoverApproval> political_takeover_source(const StableId& takeover_fact_id) const = 0;
    // 当前执行另核实际接管链、责任决定和合法容器。未承接应返回明确暂停，不能假定旧债消失。
    virtual Outcome<PoliticalResponsibilityExecution> political_responsibility_for_execution(
        const StableId& takeover_fact_id, const PoliticalResponsibilityRef&,
        const WorldIdentity& expected_identity) const = 0;
    // 本域phase传已验证责任键；只从真实登记定位最新覆盖，未登记才返回empty。
    virtual Outcome<std::optional<PoliticalResponsibilityExecution>> find_political_responsibility_for_execution(
        const PoliticalResponsibilityRef&,const WorldIdentity& expected_identity) const = 0;
    // 按具名经济或任务执行事实之前的最后登记/裁定读历史，不把旧批准当永久付款许可。
    virtual Outcome<std::optional<PoliticalResponsibilityExecution>> political_responsibility_before_fact(
        const PoliticalResponsibilityRef&,const StableId& execution_fact_id,Integer execution_tick,
        const WorldIdentity& expected_identity) const {
        return {false,{},common_failure("PoliticalTakeoverHistoryReadRequired").error};
    }
};
}
