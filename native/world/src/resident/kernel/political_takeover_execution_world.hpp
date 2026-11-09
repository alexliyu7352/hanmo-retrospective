// 当前World调用栈持有全部跨域读对象；不保存到服务单例，不跨clock或回滚重用。
#pragma once
#include "political_takeover_integration.hpp"
#include "economic_supply_policy_integration.hpp"
#include "resident/domains/politics/takeover/politics_occupation_taxpayer.hpp"
#include "resident/domains/local_economy/takeover/execution_phase.hpp"
#include "resident/domains/tasks/task_public_account_context.hpp"
#include "resident/domains/tasks/task_personal_advance_rules.hpp"
namespace hanmo::resident {
class PoliticalTakeoverExecutionWorldAccess final {
public:
    static Outcome<std::unique_ptr<PoliticalTakeoverExecutionWorldAccess>> create(const WorldBundle&,const DynamicStepInput* phase=nullptr);
    const EconomicTakeoverReadContext* economy()const{return economy_?&*economy_:nullptr;}
    const TaskExecutionServices* tasks()const{return politics_?&tasks_:nullptr;}
    const EconomicSupplyPolicyRead* issued_supply()const{return supply_?&supply_->read():nullptr;}
    const PoliticalTakeoverRead* responsibilities()const{return politics_?&politics_->responsibilities():nullptr;}
    // 普通读限已提交时钟；显式单半日借口只为本候选暂停投影，不生成库存观察。
    Outcome<std::vector<EconomicSupplySuspension>> supply_suspensions()const;
    const OccupationFiscalTaxpayerRead* taxpayer()const{return taxpayer_?&*taxpayer_:nullptr;}
    const FiscalPolicyOccupationRead* fiscal()const{return politics_?&politics_->fiscal():nullptr;}
    PoliticalTakeoverExecutionWorldAccess(const PoliticalTakeoverExecutionWorldAccess&)=delete;
    PoliticalTakeoverExecutionWorldAccess& operator=(const PoliticalTakeoverExecutionWorldAccess&)=delete;
private:
    PoliticalTakeoverExecutionWorldAccess(const WorldBundle&,std::unique_ptr<PoliticalTakeoverWorldAccess>,std::unique_ptr<EconomicSupplyWorldAccess>,const DynamicStepInput*);
    std::unique_ptr<PoliticalTakeoverWorldAccess> politics_;
    std::unique_ptr<EconomicSupplyWorldAccess> supply_;
    std::optional<PoliticalOccupationFiscalTaxpayerAccess> taxpayer_;
    std::optional<TaskPersonalAdvanceAccess> advances_;
    std::optional<EconomicTakeoverReadContext> economy_;
    TaskExecutionServices tasks_;
};
}
