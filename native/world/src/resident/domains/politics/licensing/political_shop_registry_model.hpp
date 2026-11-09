// 独立显式接册段；原法律1的六表、原政治批准和各债权账均保持原来源与字节。
#pragma once
#include "political_shop_registry_contract.hpp"
namespace hanmo::resident {
struct PoliticalShopRegistrySuccessorPolicy {
    StableId id,court_id,organization_id,office_id,role_id,dialogue_id,request_node_id;
};
struct PoliticalShopRegistrySuccessionCatalog {
    bool enabled=false;
    Integer version=1;
    OrderedTable<PoliticalShopRegistrySuccessorPolicy> policies;
};
struct PoliticalShopRegistryPlayerReceipt {
    StableId result_id;
    Integer request_sequence=0,result_revision=0;
};
struct PoliticalShopRegistryAcceptance {
    StableId id,policy_id,court_id,initial_registry_fact_id;
    std::optional<StableId> previous_acceptance_fact_id;
    StableId registration_fact_id,approval_fact_id,occupation_fact_id;
    ActingAuthority authority;
    StableId grant_id;
    Integer tick=0;
    // 接册前本院法律1全部真实头，按Common追加序冻结：完整初册、通缉、解除、查核、许可、撤证。
    // 每次交接都须精确全覆盖，不是可挑选的清案清单；后来处分另留自己的新原事实。
    std::vector<StableId> retained_legal_fact_ids;
    // 只由真实玩家提交尾段在分配结果后绑定；NPC原命令不伪造玩家请求。
    // 新独立段尚未正式生产，显式保存而不从已裁剪Common历史猜配旧接册。
    std::optional<PoliticalShopRegistryPlayerReceipt> player_receipt;
};
struct PoliticalShopRegistrySuccessionState {
    Integer version=1;
    OrderedTable<PoliticalShopRegistryAcceptance> acceptances;
};
struct PoliticalAcceptShopRegistry {
    ActingAuthority authority;
    StableId policy_id,registration_fact_id,expected_approval_fact_id;
    std::optional<StableId> expected_previous_acceptance_fact_id;
};
struct PoliticalShopRegistryAcceptanceQuote {
    WorldIdentity identity;
    StableId actor_id,grant_id;
    PoliticalAcceptShopRegistry command;
    PoliticalShopRegistryEndpoint endpoint;
    StableId initial_registry_fact_id,occupation_fact_id;
    std::vector<StableId> retained_legal_fact_ids;
};
BoundaryValue encode_political_shop_registry_catalog(const PoliticalShopRegistrySuccessionCatalog&);
Outcome<PoliticalShopRegistrySuccessionCatalog> decode_political_shop_registry_catalog(const BoundaryValue&);
BoundaryValue encode_political_shop_registry_successions(const PoliticalShopRegistrySuccessionState&);
Outcome<PoliticalShopRegistrySuccessionState> decode_political_shop_registry_successions(const BoundaryValue&);
Status visit_political_shop_registry_successions_checkpoint(CodecVisitor&,PoliticalShopRegistrySuccessionState&);
BoundaryValue encode_political_accept_shop_registry(const PoliticalAcceptShopRegistry&);
Outcome<PoliticalAcceptShopRegistry> decode_political_accept_shop_registry(const BoundaryValue&);
}
