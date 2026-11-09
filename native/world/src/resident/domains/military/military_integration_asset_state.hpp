// 整编的实际交接快照；只记录来源及前后值，不拥有第二份库存或人口。
#pragma once
#include "siege_integration_state.hpp"
namespace hanmo::resident {
// 钱包只转移当时合法未预留公款，收据记录两端余额以核守恒。
struct MilitaryIntegrationWallet {
    StableId source_wallet_id,destination_wallet_id,source_organization_id,recipient_organization_id;
    Integer amount=0,source_before=0,source_after=0,destination_before=0,destination_after=0;
};
// 全批次保留原ID；私人、合同及行动预留使整次交接明确失败。
struct MilitaryIntegrationLot {
    StableId source_inventory_id,destination_inventory_id,lot_id,item_id,source_organization_id,recipient_organization_id;
    Integer quantity=0;
};
// 实际看管从一个合法军团移交到另一个；不改变俘虏原军籍或分类。
struct MilitaryIntegrationCustody { StableId member_id,from_army_id,to_army_id; };
// 军籍是原编组后的追加事件；原军团身份和编组条款永远不改。
struct MilitaryIntegrationFact {
    StableId id,proposal_id,proposal_key,siege_id,stronghold_id,recipient_army_id,recipient_organization_id;
    Integer tick=0;
    std::vector<StableId> decision_source_ids;
    std::vector<SiegeIntegrationArmy> source_armies;
    std::vector<StableId> recipient_units_before,recipient_units_after,recipient_members_before,recipient_members_after;
    std::vector<MilitaryIntegrationWallet> wallets;
    std::vector<MilitaryIntegrationLot> lots;
    std::vector<MilitaryIntegrationCustody> custody;
    std::vector<SiegeIntegrationAppointment> ended_appointments;
    std::vector<StableId> closed_campaign_ids,population_policy_ids,population_receipt_ids;
};
// 报价不分配ID；应用必须重新报价并比较所有当前资源与精确人员。
struct MilitaryIntegrationQuote { MilitaryIntegrationFact transfer;Integer receiving_weight_before=0,receiving_weight_after=0,receiving_capacity=0;std::vector<ReadVersion> resource_versions; };
} // 世界候选是唯一提交边界，报价或读取历史本身不产生同意。
