// 安民支出只持有具名受领责任和资源收据；任务/工作时钟归S3，公职与民心结果归S6。
#pragma once
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident {
struct LocalEconomyState;
// 每项受领明确同地群体和其唯一家庭资产，禁止把组织Grant当人物私产控制权。
struct HouseholdReliefRecipient {StableId household_group_id,cohort_id;Integer cash=0,grain=0;};
struct PublicReliefTerms {
    StableId id,task_id,contract_id,acceptance_fact_id,authority_source_fact_id;
    StableId jurisdiction_id,site_id,organization_id,public_wallet_id,public_inventory_id,item_id;
    // 原ReservationRef::Kind::contract；id必须等于真实治理contract_id，不新增task枚举。
    StableId reservation_id;
    Integer total_cash=0,total_grain=0;
    std::vector<HouseholdReliefRecipient> recipients;
};
// 接受与工作事实唯一读取S3的TaskGovernanceRead，不另定义第二个任务状态或完成来源。
class TaskGovernanceRead;
// 所有受领者都实际收到后才形成一份结果；记录不保存任何钱包余额。
struct HouseholdReliefTransfer {
    StableId household_group_id,cohort_id,wallet_id,inventory_id;
    Integer cash=0,grain=0;
};
struct PublicReliefReceipt {
    StableId id,terms_id,task_id,contract_id,action_id,acceptance_fact_id,authority_source_fact_id,work_fact_id;
    Integer tick=0,cash_transferred=0,grain_transferred=0;
    std::vector<HouseholdReliefTransfer> recipients;
};
// S3只消费同task/action/work_fact的实际收据，不能按预计金额把任务标成最终成功。
class PublicReliefResultRead {
public:
    virtual ~PublicReliefResultRead()=default;
    virtual Outcome<std::optional<PublicReliefReceipt>> public_relief_receipt(const StableId& receipt_id)const=0;
};
}
