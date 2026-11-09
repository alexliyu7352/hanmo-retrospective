// 军营四页有限只读输入；正式报价使用中央preview回调保持全部公共/领域守卫次序。
#pragma once
#include "resident/domains/military/military_refs.hpp"
#include "resident/storage/common_store.hpp"
#include "resident/domains/estate/estate_model.hpp"
namespace hanmo::resident {
using MilitaryCampPreview=std::function<Outcome<BoundaryValue>(const StableId& kind,const BoundaryValue& params,const StableId& actor)>;
// 本人原军籍只读投影；空闲与行军均不授予统帅命令。
Outcome<BoundaryValue> military_service_page(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const CommonCatalog&);
BoundaryValue military_camp_provider_context(const CommonStore&,const MilitaryCatalog&,const CommonCatalog&);
Outcome<BoundaryValue> military_camp_page(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const CommonCatalog&,const EstateState&,const StableId& tab,const BoundaryValue& selection,const BoundaryValue& provider_context);
Outcome<BoundaryValue> military_camp_offer(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const CommonCatalog&,const TravelCatalog&,const StableId& kind,const BoundaryValue& params,const MilitaryCampPreview&);
}
