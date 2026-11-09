// 本域只发行真实客户声明和一次Task绑定；原Task负责价款预留与两腿交付。
#pragma once
#include "private_shop.hpp"
namespace hanmo::resident {
struct PrivateShopOrderQuote {
    WorldIdentity source_identity;StableId actor_id;PrivateShopRequestOrder command;
    PrivateShopOrderTerms terms;Integer valid_through_tick=0;AuthoredProviderSourceProof agreement;
};
// 低层来源仅验本账、作者条款和Common事实，不调用完整Shop或Task验证器。
Status validate_private_shop_order_ledger(const PrivateShopContext&);
Status validate_private_shop_orders(const PrivateShopContext&);
Outcome<PrivateShopOrderQuote> preview_private_shop_order(const PrivateShopContext&,const PrivateShopMeetingContext&,const StableId&,const PrivateShopRequestOrder&);
Outcome<StableId> issue_private_shop_order_in_candidate(CommonStore&,PrivateShopState&,const PrivateShopContext&,const PrivateShopMeetingContext&,const PrivateShopOrderQuote&);
Outcome<StableId> bind_private_shop_order_in_candidate(CommonStore&,PrivateShopState&,const PrivateShopContext&,const StableId& order_id,const StableId& task_id);
// Task原两腿已经实际完成后，在同一外层候选确认已售成本；取消只确认0收入，不冒现金退款。
Outcome<StableId> capture_private_shop_order_execution_in_candidate(CommonStore&,PrivateShopState&,const PrivateShopContext&,const StableId& order_id);
Status validate_private_shop_order_settlements(const PrivateShopContext&,bool read_task_sources);
class PrivateShopOrderAccess final:public PrivateShopOrderRead {
public:
    explicit PrivateShopOrderAccess(const PrivateShopContext& context):context_(context){}
    Outcome<std::optional<PrivateShopOrderSource>> private_shop_order_source(const StableId&)const override;
private:const PrivateShopContext& context_;
};
// 只重核发行时冻结的客户当前真实say；S3接受前在同候选调用，不创建第二次订单。
Status revalidate_private_shop_order_acceptance_meeting(const PrivateShopContext&,const PrivateShopMeetingContext&,const StableId& proprietor,const StableId& order_id);
}
