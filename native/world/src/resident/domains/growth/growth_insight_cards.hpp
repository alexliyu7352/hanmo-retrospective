// 见闻卡只显示本人已完成经历；外部图鉴只改变发现展示，不生成本局经历。
#pragma once
#include "growth_insight_sources.hpp"
#include "resident/storage/common_codec.hpp"

namespace hanmo::resident {
struct GrowthInsightCardDefinition {
    StableId id,name_key,description_key,hint_key,icon_id;
    StableId condition="completed_camp_training_v1";
};
struct GrowthInsightCardCatalog {
    bool enabled=false;
    OrderedTable<GrowthInsightCardDefinition> definitions;
};
Status validate_growth_insight_card_catalog(const GrowthInsightCardCatalog&,const CommonCatalog&);
Outcome<GrowthInsightCardCatalog> decode_growth_insight_card_catalog(
    const BoundaryValue& definitions,const BoundaryValue& selected_config,const CommonCatalog&);
// 只有当前玩家可查；返回最多64项，永久经历不含execution_epoch，临时页面保完整identity。
Outcome<BoundaryValue> growth_insight_cards_view(const CommonState&,const CommonCatalog&,
    const MilitaryState&,const GrowthInsightCardCatalog&,const StableId& actor_id,
    const std::vector<StableId>& discovered,Integer offset=0,Integer limit=32);
}
