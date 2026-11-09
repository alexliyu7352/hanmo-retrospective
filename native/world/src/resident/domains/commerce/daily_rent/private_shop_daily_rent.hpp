// 显式日租保存原租期和原 LE 来源；不保存另一份余额、欠款总额或经营主体。
#pragma once
#include "private_shop_rent_period.hpp"
#include "resident/domains/commerce/private_shop.hpp"
namespace hanmo::resident {
inline constexpr const char* private_shop_daily_rent_module="private_shop_daily_rent";
inline constexpr const char* private_shop_daily_rent_capability="private_shop_daily_rent_v1";
inline constexpr const char* private_shop_rent_assessed_kind="private_shop_rent_assessed";
inline constexpr const char* private_shop_rent_paid_kind="private_shop_rent_paid";
inline constexpr const char* private_shop_rent_resumed_kind="private_shop_rent_resumed";
// 不足只计一期、暂停期间停止新增应付，属于这份租约明确选择的条款。
struct PrivateShopDailyRentCatalog {
    Integer version=1;
    StableId profile="opening_anchored_two_halfdays_v1";
    StableId arrears_policy="one_unpaid_period_then_suspend_v1";
};
// 原应付只有 economy.arrears[arrear_id]；本记录指明何时取得或暂停该期占铺。
struct PrivateShopRentAssessment {
    StableId id,shop_id,arrear_id,trigger,resume_fact_id,automatic_payment_fact_id;
    Integer tick=0,period_index=0;
};
// 付款金额只从原 PaymentReceipt 取。自愿出资经过原店钱包、整笔支付原出租人，原债务主体不变。
struct PrivateShopRentPayment {
    StableId id,assessment_fact_id,local_payment_id,funding_kind,actor_id,payer_wallet_id;
    StableId mandate_fact_id,death_notice_binding_fact_id;
    Integer tick=0;
};
// 偿清不自动复业；仅原店主明确决定，仍沿原开设时刻的期次网格。
struct PrivateShopRentResumption {
    StableId id,shop_id,actor_id,assessment_fact_id;
    Integer tick=0;
};
// 玩家来源永久绑本次实际提交；自动日历来源不伪造玩家回执。
struct PrivateShopRentPlayerResult {
    StableId id,actor_id,command_kind,result_id;
    Integer request_seq=0,revision=0,tick=0;
};
struct PrivateShopDailyRentState {
    Integer version=1,last_step_tick=0;
    OrderedTable<PrivateShopRentAssessment> assessments;
    OrderedTable<PrivateShopRentPayment> payments;
    OrderedTable<PrivateShopRentResumption> resumptions;
    OrderedTable<PrivateShopRentPlayerResult> player_results;
};
// 只读派生，款项引用唯一原 LE 账；该摘要不是可独立提交或消费的状态。
struct PrivateShopRentLedgerView {
    StableId shop_id,assessment_fact_id,pending_arrear_id,last_payment_id;
    Integer period_index=0,through_tick=0,paid_cash=0,shop_cash_paid=0,contributed_cash=0;
    bool suspended=true,closed=false;
};
// 早位仅结构/期次/原钱账来源，不回调 Shop、法律、继产或 Task 全验。经营与实际授权须末位完整复核。
Status validate_private_shop_daily_rent_sources(const CommonStore&,const LocalEconomyState&,const PrivateShopState&,const PrivateShopDailyRentCatalog&,const PrivateShopDailyRentState&,const DynamicStepInput* phase=nullptr);
Outcome<PrivateShopRentLedgerView> read_private_shop_rent_ledger(const CommonStore&,const LocalEconomyState&,const PrivateShopState&,const PrivateShopDailyRentCatalog&,const PrivateShopDailyRentState&,const StableId& shop_id,const DynamicStepInput* phase=nullptr);
// 仅 post-clock 原半日调用：Common 已到 to，日租仍在 from；返回旧完整租期供本次逐店应计。
Outcome<std::vector<PrivateShopRentLedgerView>> read_private_shop_rent_before_calendar(const CommonStore&,const LocalEconomyState&,const PrivateShopState&,const PrivateShopDailyRentCatalog&,const PrivateShopDailyRentState&,const DynamicStepInput& actual_step);
// 禁用档也调用空态口，拒绝孤新事实；不会补写缺失段。
Status validate_private_shop_daily_rent_layout(const CommonStore&,const PrivateShopDailyRentState*);
}
