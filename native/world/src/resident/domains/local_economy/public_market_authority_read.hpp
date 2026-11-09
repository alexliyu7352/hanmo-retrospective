#pragma once
#include "market_service_model.hpp"
namespace hanmo::resident {
// S6返回原批准的借用投影，不在S5持另一份批准状态；源/采用顺序必须由实际批准历史证明。
struct PublicMarketAuthorityView {
 PublicMarketAssetBasis asset_basis;
 MarketServiceOpeningBasis opening_basis;
 MarketServiceUsageRightBasis usage_right_basis;
};
class PublicMarketAuthorityRead {
public:
 virtual ~PublicMarketAuthorityRead()=default;
 virtual Outcome<MarketServiceChangeCause> read_public_market_service_change(const MarketServiceDefinition&,const FacilitySourceRef&)const=0;
 // actual_change是同candidate已成立的原业务结果；同Moment按原Common采用顺序取批准历史。
 virtual Outcome<PublicMarketAuthorityView> read_public_market_authority_at_adoption(const MarketServiceDefinition&,const SourceRef& actual_change,const WorldMoment& actual_at)const=0;
 // 仅原公有营业/用途初始化、修订或结束源，不能用Office/Grant或staffing批准代替。
 virtual Outcome<WorldMoment> read_public_market_authority_change_moment(const SourceRef&)const=0;
 virtual Status validate_public_market_authority_history(const MarketServiceDefinition&,const PublicMarketAuthorityView&,const SourceRef& actual_change,const WorldMoment& actual_at)const=0;
};
}
