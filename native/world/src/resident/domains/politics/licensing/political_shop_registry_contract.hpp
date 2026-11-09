// 接册只延续原完整司法册；后继法官的每项新处分仍另核当时公职与该命令专项Grant。
#pragma once
#include "resident/domains/politics/takeover/politics_takeover_contract.hpp"
namespace hanmo::resident {
inline constexpr const char* political_shop_registry_succession_module="political_shop_registry_successions";
inline constexpr const char* political_shop_registry_accept_command="political_accept_shop_registry";
inline constexpr const char* political_shop_registry_accept_kind="political_shop_registry_accepted";
inline constexpr const char* political_shop_registry_accept_prefix="political_shop_registry_acceptance";

// 后继柜台来自显式作者政策，同一原辖区/铺位范围，不由当前Owner或接任人自行指定。
struct PoliticalShopRegistryEndpoint {
    StableId court_id,jurisdiction_id,organization_id,office_id,site_id;
    StableId role_id,dialogue_id,request_node_id;
};

struct PoliticalShopRegistryExecutionSource {
    WorldIdentity source_identity;
    PoliticalShopRegistryEndpoint endpoint;
    StableId acceptance_fact_id,initial_registry_fact_id,policy_id;
    std::optional<StableId> previous_acceptance_fact_id;
    StableId registration_fact_id,approval_fact_id,occupation_fact_id;
    ActingAuthority accepting_authority;
    StableId accepting_grant_id;
    Integer accepted_tick=0,observed_tick=0;
    // 仅表示这本册在该截面仍由此政权接管，绝不是登记、解除、撤证或签发许可的授权位。
    bool registry_current=false;
    StableId unavailable_code;
};

class PoliticalShopRegistrySuccessionRead {
public:
    virtual ~PoliticalShopRegistrySuccessionRead()=default;
    // empty仅表示没有接册来源；调用方仍须用原稳定控制读决定旧初册是否可用。
    virtual Outcome<std::optional<PoliticalShopRegistryExecutionSource>> shop_registry_current(
        const StableId& court_id)const=0;
    // 只接受法律1的真实处分/许可头或真实private_shop_opened。
    // source_identity仍为同一候选；observed_tick是实际载荷时刻，接册必须严格位于该头之前。
    // 本纯口不可反调完整Shop、Task、Growth或政治原责任全集；各域完整冷验在World外层进行。
    virtual Outcome<std::optional<PoliticalShopRegistryExecutionSource>> shop_registry_before_legal_fact(
        const StableId& court_id,const StableId& actual_fact_id,Integer tick)const=0;
};
}
