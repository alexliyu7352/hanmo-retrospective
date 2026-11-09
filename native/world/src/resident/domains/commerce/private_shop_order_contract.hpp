// 商铺订单是明确的自营财货授权，不替换Actor默认背包/报酬，也不把任意旧Task改成经营订单。
#pragma once
#include "private_shop_contract.hpp"
namespace hanmo::resident {
// 本人只请求原客户当前明确say提出的订单，尚未受领不分配Task或占用价款。
struct PrivateShopRequestOrder {StableId shop_id,policy_id;};
// 客户在具名岗位的原say提出固定购买条件；期限从发行起冻结，受领不能延长原报价。
struct PrivateShopOrderPolicy {
    StableId id,premises_id,client_actor_id,client_role_id,dialogue_id,node_id,task_template_id,item_id;
    Integer quantity=0,contract_price=0,accept_ticks=2,deadline_ticks=8;
};
struct PrivateShopOrderCatalog {OrderedTable<PrivateShopOrderPolicy> policies;};
// S5持有一次冻结的订单与成本；S3只读原授权并沿唯一Task实交/清算写者执行资产。
struct PrivateShopOrderTerms {
    StableId order_id,shop_id,proprietor_actor_id;
    Owner original_client;
    StableId task_template_id,item_id,stock_inventory_id,shop_wallet_id,client_inventory_id,client_wallet_id;
    Integer quantity=0,contract_price=0,accepted_tick=-1,deadline_tick=0;
};
struct PrivateShopOrderSource {
    WorldIdentity source_identity;
    PrivateShopOrderTerms terms;
    StableId policy_id,issue_fact_id,task_id,acceptance_fact_id,status;
    PrivateShopMeetingSource client_declaration;
    Integer issued_tick=0,valid_through_tick=0;
    // 仅实际完成两腿之后确认，不把未售成本或订单承诺记成已实现利润。
    std::optional<Integer> goods_cost;StableId cost_fact_id;
};
// 发行与受领绑定分开保存；资金只由真实Task接受后的原预约写者处理。
struct PrivateShopOrderRecord {
    StableId id,policy_id,status="pending_order",task_id,acceptance_fact_id,binding_fact_id;
    PrivateShopOrderTerms terms;PrivateShopMeetingSource client_declaration;
    Integer issued_tick=0,valid_through_tick=0;
};
struct PrivateShopOrderBinding {StableId id,order_id,task_id,acceptance_fact_id;Integer tick=0;};
struct PrivateShopOrderState {OrderedTable<PrivateShopOrderRecord> orders;OrderedTable<PrivateShopOrderBinding> bindings;};
class PrivateShopOrderRead {
public:
    virtual ~PrivateShopOrderRead()=default;
    // 必须仅验本账/Common真实头和原条款；不能反调完整Shop/Task资格造成冷验证递归。
    virtual Outcome<std::optional<PrivateShopOrderSource>> private_shop_order_source(const StableId& order_id)const=0;
};
// S3永久实物/付款来源只读返回真实两腿及原结算ID；未实交、取消和退款不能冒成功订单。
struct PrivateShopOrderLotTransfer {StableId original_lot_id,destination_lot_id;Integer quantity=0;};
struct PrivateShopOrderExecution {
    WorldIdentity source_identity;
    StableId order_id,task_id,settlement_id,receipt_fact_id;
    PrivateShopOrderTerms terms;
    StableId outcome;Integer completed_tick=0,delivered_quantity=0,paid_price=0,refunded_price=0;
    std::vector<PrivateShopOrderLotTransfer> lots;
};
class PrivateShopOrderExecutionRead {
public:
    virtual ~PrivateShopOrderExecutionRead()=default;
    // 完整Task/实物/支付史由World先验；本口不能从今天的余额倒推历史成交或客户。
    virtual Outcome<std::optional<PrivateShopOrderExecution>> private_shop_order_execution(const StableId& task_id)const=0;
};
}
