// 有符号除法向负无穷取整，与正数ceil分别实现，避免C++截零改变缺货/富余报价。
#include "market_price_rules.hpp"
#include <limits>
namespace hanmo::resident {namespace {
using Wide=__int128;
Wide floor_div(Wide n,Wide d){Wide q=n/d,r=n%d;return q-(r<0);}
Wide ceil_div(Wide n,Wide d){return -floor_div(-n,d);}
Outcome<Integer> price_failure(const StableId& code){return {false,0,common_failure(code).error};}
}
Status validate_market_rule_profile(const MarketRuleProfile& p){
 if(p.id.empty()||p.market_id.empty()||p.item_id.empty()||p.revision<=0||p.base_price<=0||p.target_stock<=0||p.stock_elasticity_bp<0||p.stock_elasticity_bp>10000||p.event_adjustment_bp < -10000||p.event_adjustment_bp>10000||p.buy_factor_bp!=11500||p.sell_factor_bp!=8500||p.maximum_discount_bp<0||p.maximum_discount_bp>800)return common_failure("InvalidMarketRuleProfile",p.id);
 if(p.event_adjustment_bp&&(p.event_source_ref.empty()||p.event_from_tick<0||p.event_until_tick<=p.event_from_tick))return common_failure("MarketEventSourceRequired",p.id);return common_success();
}
Outcome<Integer> compute_market_midpoint(const MarketRuleProfile& p,Integer stock){auto valid=validate_market_rule_profile(p);if(!valid.ok)return {false,0,valid.error};if(stock<0)return price_failure("InvalidMarketStock");const Wide base=p.base_price;
 Wide product=0;if(__builtin_mul_overflow(base,Wide(p.stock_elasticity_bp),&product)||__builtin_mul_overflow(product,Wide(p.target_stock)-stock,&product))return price_failure("MarketPriceOverflow");
 const Wide value=base+floor_div(product,Wide(10000)*p.target_stock)+floor_div(base*p.event_adjustment_bp,10000);const Wide midpoint=std::clamp(value,ceil_div(base,2),base*2);
 if(midpoint>std::numeric_limits<Integer>::max())return price_failure("MarketPriceOverflow");return {true,Integer(midpoint),{}};
}
Outcome<Integer> compute_market_line_total(const MarketRuleProfile& p,Integer stock,MarketTradeSide side,Integer quantity,Integer discount){auto valid=validate_market_rule_profile(p);if(!valid.ok)return {false,0,valid.error};if(stock<0||quantity<1||quantity>500||discount<0||discount>p.maximum_discount_bp||(side==MarketTradeSide::buy&&quantity>stock))return price_failure("InvalidMarketLine");
 Wide total=0;for(Integer i=0;i<quantity;++i){Integer at=0;if(side==MarketTradeSide::buy)at=stock-i-1;else if(__builtin_add_overflow(stock,i,&at))return price_failure("MarketStockOverflow");auto midpoint=compute_market_midpoint(p,at);if(!midpoint.ok)return midpoint;const Wide line=side==MarketTradeSide::buy?ceil_div(Wide(midpoint.value)*(p.buy_factor_bp-discount),10000):floor_div(Wide(midpoint.value)*(p.sell_factor_bp+discount),10000);total+=line;if(total>std::numeric_limits<Integer>::max())return price_failure("MarketPriceOverflow");}return {true,Integer(total),{}};
}
}
