// 永久采购成本引用只能钉住真实原trade；本口不提供全市场价格历史或第二份贸易账。
#pragma once
#include "private_shop.hpp"
namespace hanmo::resident {
// 在Growth回收前读取并硬验缺源；当前完整Growth先验仍由World负责，缺trade不可被跳过成空pin。
Outcome<std::vector<StableId>> read_private_shop_trade_pins(const CommonStore&,const PrivateShopState&,const GrowthState&);
}
