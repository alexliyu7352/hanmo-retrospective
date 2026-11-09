// 营业行动只提供本人实际到岗的日服务；全部财货与租金仍提交原Common/LE候选。
#pragma once
#include "private_shop.hpp"
#include "private_shop_accounting.hpp"
namespace hanmo::resident {
struct PrivateShopOperationQuote {WorldIdentity source_identity;StableId actor_id;PrivateShopOperate command;Integer rent_cash=0,duration=2;StableId staff_contract_id;Integer wage_cap=0;std::vector<ReadVersion> versions;};
Outcome<PrivateShopOperationQuote> preview_private_shop_operation(const PrivateShopContext&,const StableId&,const PrivateShopOperate&);
// 只有外层完整候选拥有Common保存点；本域guards保持同地址引用，失败必须连资产/行动一起丢弃。
Outcome<StableId> start_private_shop_operation_in_candidate(CommonStore&,LocalEconomyState&,PrivateShopState&,const PrivateShopContext&,const PrivateShopOperationQuote&);
struct PrivateShopOperationSourcesState;struct PrivateShopOperationCapabilitiesState;struct PrivateShopOperationSourceContext;
// Same original rent/staff/Action writer, with real exact acceptance and sole
// S0 adoption before validation. Caller must finish actual market reconciliation
// in the same unpublished World candidate before any publication.
Outcome<StableId> start_private_shop_operation_in_candidate(CommonStore&,LocalEconomyState&,PrivateShopState&,
 PrivateShopOperationSourcesState&,PrivateShopOperationCapabilitiesState&,ActionElapsedStore&,
 const PrivateShopOperationSourceContext&,const PrivateShopOperationQuote&,const ActorFoundationRuleRef& actual_market_definition);
// from/current到to/current+1的实际Action步进；不按缺勤补班，不产生第二个工时计数器。
Outcome<std::optional<StableId>> step_private_shop_operation_in_candidate(CommonStore&,LocalEconomyState&,PrivateShopState&,const PrivateShopContext&,const StableId& action_id,const DynamicStepInput&);
// 报价与正式取消共用本人和真实进行中Action门。
Status preview_private_shop_cancel_operation(const PrivateShopContext&,const StableId& actor,const StableId& action);
Outcome<StableId> cancel_private_shop_operation_in_candidate(CommonStore&,LocalEconomyState&,PrivateShopState&,const PrivateShopContext&,const StableId& actor,const StableId& action_id);
// 完整冷验从本表和原LE事实推导日费用；正文已验证店、市场及原采购载荷后调用。
Status validate_private_shop_operations(const PrivateShopContext&);
Outcome<Integer> private_shop_observed_tick(const PrivateShopContext&);
}
