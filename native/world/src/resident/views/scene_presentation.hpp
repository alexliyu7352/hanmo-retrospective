// 原场所/城市/世界场景的有限只读投影；只读既有知闻与真实可见人，不授予行动许可。
#pragma once
#include "resident/domains/interaction/interaction_actions.hpp"
#include "resident/domains/estate/estate_model.hpp"
#include "resident/domains/military/military_refs.hpp"
#include "resident/domains/social/social_model.hpp"
#include "common_presentation.hpp"
#include "character_presentation.hpp"
namespace hanmo::resident {
struct ScenePoint {double x=0,y=0;};
struct ScenePlaceDefinition {StableId id,settlement_id,name_key,visual_tag;std::optional<StableId> parent_place,scene_template_id;OrderedTable<StableId> scene_template_ids;bool template_mapping_valid=true,legacy_template_valid=true;std::vector<StableId> invalid_template_views;};
struct SceneSettlementDefinition {StableId id,name_key,entry_place;ScenePoint point;};
struct SceneRouteDefinition {StableId id;Integer duration_ticks=0;std::vector<ScenePoint> points;};
struct SceneMapDefinition {StableId id;std::optional<StableId> scene_template_id;bool template_mapping_valid=true;};
struct SceneCatalog {OrderedTable<ScenePlaceDefinition> places;OrderedTable<SceneSettlementDefinition> settlements;OrderedTable<SceneRouteDefinition> routes;OrderedTable<SceneMapDefinition> maps;OrderedTable<StableId> scenario_maps,estate_name_keys;bool estates=false;};
Outcome<SceneCatalog> decode_scene_catalog(const BoundaryValue&);
Outcome<BoundaryValue> query_scene_view(const CommonStore&,const InteractionState&,const InteractionCatalog&,const SocialState&,const EstateState&,const EstateCatalog&,const CommonCatalog&,const StableId& place_id,const StableId& view_kind);
Outcome<BoundaryValue> query_place_sidebar(const CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const EstateState&,const EstateCatalog&,const MilitaryState&,const MilitaryCatalog&,const CommonCatalog&);
Outcome<BoundaryValue> query_scene_character(const CommonStore&,const InteractionState&,const InteractionCatalog&,const CommonCatalog&,const StableId&);
}
