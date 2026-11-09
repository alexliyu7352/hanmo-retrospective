// 商铺纯计价和成本分摊；输入须由真实采购/运费来源核验，本文件不授产权或生成钱货。
#pragma once
#include "resident/model/common_model.hpp"
namespace hanmo::resident {
// 成本批次只保已发生费用及已售份额，不保存库存余额；实际批次始终来自Common。
struct PrivateShopCostBasis {
    StableId acquisition_fact_id;
    Integer received_quantity=0,purchase_cash=0,allocated_transport_cash=0;
    Integer sold_quantity=0,recognized_cost=0;
};
struct PrivateShopCostSlice {
    Integer quantity=0,cost=0;
    Integer sold_quantity_after=0,recognized_cost_after=0;
};
struct PrivateShopRetailRate {Integer unit_price=0,daily_quantity_limit=0;};
struct PrivateShopOperatingResult {
    Integer quantity=0,retail_income=0,cost_of_goods=0,rent_cash=0,wage_cash=0,operating_transport_cash=0,net_income=0;
};
// 均价按累计实售份额向下取整，最后一件收尽原成本；拆成多次出售不会消失成本或重复费用。
Outcome<PrivateShopCostSlice> quote_private_shop_cost_slice(const PrivateShopCostBasis&,Integer quantity);
// 中间价由原市场具名只读口提供；这里只实现既定商铺加价0/500/1000与日需3/2/1。
Outcome<PrivateShopRetailRate> quote_private_shop_retail_rate(Integer verified_middle_price,Integer markup_basis_points);
// 已资本化进货成本中的运费不能再传作营业运费；个人投入、退款和开店本金均不是收入。
Outcome<PrivateShopOperatingResult> quote_private_shop_operating_result(const PrivateShopRetailRate&,const PrivateShopCostSlice&,Integer paid_rent,Integer paid_wage,Integer paid_operating_transport=0);
}
