// 死后私产交割仅消费已核原声明/本人接受的剩余额度；它不授予继承、豁免责任或远程接触。
#pragma once
#include "succession_personal_assets.hpp"
#include <functional>
namespace hanmo::resident {
// 仅由遗产账从原声明减去逐次实际交割推导，外部命令不能自行提供这个权源。
struct InheritancePersonalEntitlement {
    StableId declaration_id,acceptance_fact_id,death_fact_id,donor_actor_id,beneficiary_actor_id,source_wallet_id,source_inventory_id;
    Integer money_remaining=0;std::vector<SuccessionPersonalLot> remaining_lots;
};
struct InheritancePersonalSelection { Integer money=0;std::vector<SuccessionPersonalLot> lots; };
// 同候选原责任域逐项证明。空before_fact仅当前准入；冷验证必须实际边界，未知历史须拒。
using InheritancePrivateClearanceRead=std::function<Status(const StableId& actor,const StableId& wallet,const StableId& inventory,Integer tick,const StableId& before_fact_id)>;
struct InheritancePersonalQuote {
    WorldIdentity identity;InheritancePersonalEntitlement entitlement;InheritancePersonalSelection selected;
    SuccessionPersonalTerms transfer;StableId place_id,settlement_id;
    Integer source_balance=0,destination_balance=0,source_spendable=0,source_inventory_revision=0,destination_inventory_revision=0,destination_weight=0,transferred_weight=0;
};
// 只有同一真实AtPlace才可接触原随身钱物；水上锚点/同军关系不替代打捞或当前接触。
Outcome<InheritancePersonalQuote> preview_inheritance_personal_assets(const CommonStore&,const InheritancePersonalEntitlement&,const InheritancePersonalSelection&,const InheritancePrivateClearanceRead&);
// 精确原lot减量与新私lot按一份Common recipe完成，不用同品类TransferLots替换已失去的旧批次。
Outcome<ResourceRecipe> inheritance_personal_asset_recipe(const CommonStore&,const InheritancePersonalEntitlement& current,const InheritancePersonalQuote&,const InheritancePrivateClearanceRead&);
bool inheritance_personal_entitlement_equal(const InheritancePersonalEntitlement&,const InheritancePersonalEntitlement&);
}
