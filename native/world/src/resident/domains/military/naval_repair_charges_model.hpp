// Naval4收费只登记原工单的预约权源；钱包与支付仍由Common唯一账结算，旧Naval1..3不序列化本段。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
// 作者明确泊地、原公营服务组织与公库；当前Owner不能自动授予继任政权经营权。
struct NavalRepairService {
 StableId id,version,rule_id,berth_place_id,garrison_id,organization_id,wallet_id;
 Integer fee=0;
};
struct NavalRepairChargePolicy {
 StableId profile="public_dock_fees_v1";
 std::vector<NavalRepairService> services;
};
// 开工时冻结原army钱袋、双方Owner与收款营真实占领源；不把当前军职/产权倒填历史。
struct NavalRepairChargeTerms {
 StableId service_id,service_version,rule_id,berth_place_id,garrison_id,occupation_source_id;
 StableId army_id,payer_wallet_id,payee_wallet_id;
 Owner payer_owner,payee_owner;
 Integer amount=0;
 FieldOrder field_order;
};
// 与原naval_repair_start同一action事实绑定；实物工单与收费记录不得各自重复开工或支付。
struct NavalRepairChargeFact {
 StableId action_id;
 NavalRepairChargeTerms terms;
 FieldOrder field_order;
};
struct NavalRepairChargeState {std::vector<NavalRepairChargeFact> charges;FieldOrder field_order;};
}
