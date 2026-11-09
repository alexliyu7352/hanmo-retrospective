// 政治财政只提供有效政策和真实来源；税源、留粮、征收、欠期及钱货转移由S5唯一结算。
#pragma once
#include "resident/model/dynamic_contract.hpp"
#include "hanmo/resident/world_moment.hpp"
namespace hanmo::resident {
enum class FiscalTaxMode { lenient,normal,surcharge };
// revision是同辖区政策序号；source_fact_id必须指向实际政治登记/变更事实。
struct FiscalPolicySnapshot {
    StableId jurisdiction_id,site_id,organization_id,public_wallet_id,public_inventory_id,source_fact_id;
    Integer policy_revision=0,issued_tick=0,effective_from_tick=0;
    FiscalTaxMode mode=FiscalTaxMode::normal;
};
// 只证明真实失控边界及原公库转属链；不能授权收款或把旧税债改派。
// actual_occupation_at仅真原source reader填写；这是非持久读值，/1仍仅用原tick。
struct FiscalPolicyOccupationBoundary { WorldIdentity source_identity;StableId occupation_fact_id;Integer occupation_tick=0; std::optional<WorldMoment> actual_occupation_at; };
class FiscalPolicyOccupationRead {
public:
    virtual ~FiscalPolicyOccupationRead()=default;
    virtual Outcome<std::optional<FiscalPolicyOccupationBoundary>> fiscal_policy_occupation(const FiscalPolicySnapshot&)const=0;
};
// S5按原责任due_tick冻结本值；ok+empty表示该期未启用/无生效政策，绝不默认常赋开征。
// 未知地点、未来结算tick或损坏来源返回错误；period/税基/纳税来源与留粮数值不归此读口。
class FiscalPolicyRead {
public:
    virtual ~FiscalPolicyRead()=default;
    virtual Outcome<std::optional<FiscalPolicySnapshot>> fiscal_policy_for_due_tick(const StableId& site_id,Integer due_tick)const=0;
    // 已成立税期必须保留当时精确政策；普通实现也不能用任意旧source替代应期政策。
    virtual Outcome<std::optional<FiscalPolicySnapshot>> fiscal_policy_for_assessment_source(const StableId& site_id,Integer due_tick,const StableId& source_fact_id)const {
        auto result=fiscal_policy_for_due_tick(site_id,due_tick);
        if(!result.ok)return result;
        if(!result.value||result.value->source_fact_id!=source_fact_id)return {false,{},RuleError{{"FiscalAssessmentPolicySourceMismatch",source_fact_id}}};
        return result;
    }
};
} // 本接口不含可花余额，不授权从Actor私人钱包补贴税政。
