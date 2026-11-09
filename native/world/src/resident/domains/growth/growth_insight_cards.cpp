#include "resident/kernel/catalog_section_observation.hpp"
#include "growth_insight_cards.hpp"
#include <algorithm>
#include <set>

namespace hanmo::resident {
namespace {
using V=BoundaryValue;
using namespace boundary;
bool id_valid(const StableId& id){return !id.empty()&&id.size()<=256;}
StableId hex(const WorldId& id){StableId out;const char* digits="0123456789abcdef";for(auto byte:id){out+=digits[byte>>4];out+=digits[byte&15];}return out;}
V experience(const GrowthCampTrainingExperience& value){
    return obj({{"kind",V{"completed_camp_training_v1"}},{"world_id",V{hex(value.world_id)}},
        {"scenario_id",V{value.scenario_id}},{"actor_id",V{value.actor_id}},{"fact_id",V{value.fact_id}},
        {"garrison_id",V{value.garrison_id}},{"place_id",V{value.place_id}},{"unit_id",V{value.unit_id}},
        {"started_tick",V{value.started_tick}},{"completed_tick",V{value.completed_tick}}});
}
}

Status validate_growth_insight_card_catalog(const GrowthInsightCardCatalog& cards,const CommonCatalog& common){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 43, "GrowthInsightCardCatalog", "validate_growth_insight_card_catalog", &cards);

    if(!cards.enabled)return cards.definitions.empty()?common_success():common_failure("InsightCardsCapabilityRequired");
    if(cards.definitions.empty()||cards.definitions.size()>4096)return common_failure("InvalidInsightCardCatalogSize");
    for(const auto& row:cards.definitions.rows()){
        const auto& card=row.second;
        if(!id_valid(row.first)||card.id!=row.first)return common_failure("InvalidInsightCardIdentity",row.first);
        if(card.condition!="completed_camp_training_v1")return common_failure("UnsupportedInsightCondition",row.first,ErrorKind::unsupported_capability);
        for(const auto& prior:cards.definitions.rows()){if(&prior==&row)break;if(prior.second.condition==card.condition)return common_failure("DuplicateInsightCondition",row.first);}
        for(const auto* key:{&card.name_key,&card.description_key,&card.hint_key})
            if(!id_valid(*key)||!common.texts.find(*key))return common_failure("MissingInsightCardText",row.first);
        if(card.icon_id.size()>256)return common_failure("InvalidInsightCardIcon",row.first);
    }
    return common_success();
}

Outcome<GrowthInsightCardCatalog> decode_growth_insight_card_catalog(const V& definitions,const V& config,const CommonCatalog& common){try{
    fields(config,{"profile","card_ids"},{},"insight_cards");
    if(get_string(config,"profile","insight_cards")!="completed_camp_training_insights_v1")unsupported("UnsupportedInsightCardsProfile","insight_cards/profile");
    object(definitions,"insight_cards/definitions");
    const auto& selected=array(field(config,"card_ids","insight_cards"),"insight_cards/card_ids");
    if(selected.empty()||selected.size()>4096)fail("InvalidInsightCardCatalogSize","insight_cards/card_ids");
    GrowthInsightCardCatalog cards;cards.enabled=true;
    for(const auto& entry:selected){
        const auto id=string(entry,"insight_cards/card_ids");const auto path="insight_cards/definitions/"+id;
        if(!id_valid(id))fail("InvalidInsightCardIdentity",path);
        const auto& value=field(definitions,id,"insight_cards/definitions");
        fields(value,{"id","category","name_key","description_key","prerequisites","acquire_condition","effect_kind","effect_params","hint_key","icon_id","repeat_policy"},{},path);
        if(get_string(value,"category",path)!="insight")unsupported("UnsupportedInsightCategory",path+"/category");
        const auto& prerequisite=field(value,"prerequisites",path);fields(prerequisite,{"kind"},{},path+"/prerequisites");
        if(get_string(prerequisite,"kind",path)!="current_player_participation")unsupported("UnsupportedInsightPrerequisite",path+"/prerequisites");
        const auto& acquire=field(value,"acquire_condition",path);fields(acquire,{"kind"},{},path+"/acquire_condition");
        const auto& effect=field(value,"effect_params",path);fields(effect,{"kind"},{},path+"/effect_params");
        if(get_string(value,"effect_kind",path)!="record_insight"||get_string(effect,"kind",path)!="camp_training")unsupported("UnsupportedInsightEffect",path+"/effect_kind");
        if(get_string(value,"repeat_policy",path)!="once_per_actor_world")unsupported("UnsupportedInsightRepeatPolicy",path+"/repeat_policy");
        GrowthInsightCardDefinition card;card.id=get_string(value,"id",path);card.name_key=get_string(value,"name_key",path);card.description_key=get_string(value,"description_key",path);card.hint_key=get_string(value,"hint_key",path);card.icon_id=get_string(value,"icon_id",path);card.condition=get_string(acquire,"kind",path);
        if(!cards.definitions.insert(id,std::move(card)))fail("DuplicateInsightCard",path);
    }
    const auto checked=validate_growth_insight_card_catalog(cards,common);if(!checked.ok)return {false,{},checked.error};
    return {true,std::move(cards),{}};
}catch(const boundary::DecodeFailure& failure){return {false,{},failure.error};}}

Outcome<V> growth_insight_cards_view(const CommonState& common,const CommonCatalog& shared,const MilitaryState& military,
    const GrowthInsightCardCatalog& cards,const StableId& actor,const std::vector<StableId>& discovered,Integer offset,Integer limit){
    if(!cards.enabled)return {false,{},common_failure("InsightCardsUnavailable",{},ErrorKind::unsupported_capability).error};
    const auto checked=validate_growth_insight_card_catalog(cards,shared);if(!checked.ok)return {false,{},checked.error};
    const auto total=Integer(cards.definitions.size());
    if(offset<0||offset>total||limit<1||limit>64)return {false,{},common_failure("InvalidInsightCardPage").error};
    std::set<StableId> seen;
    if(discovered.size()>4096)return {false,{},common_failure("InvalidInsightDiscovery").error};
    for(const auto& id:discovered)if(!id_valid(id)||!seen.insert(id).second)return {false,{},common_failure("InvalidInsightDiscovery").error};
    const auto completed=first_growth_camp_training_experience(common,military,actor);if(!completed.ok)return {false,{},completed.error};
    auto ids=cards.definitions.order();std::sort(ids.begin(),ids.end());V::Array rows;
    for(Integer i=offset;i<std::min(total,offset+limit);++i){
        const auto& card=*cards.definitions.find(ids[std::size_t(i)]);
        const bool acquired=completed.value.has_value(),known=acquired||seen.count(card.id)>0;
        rows.push_back(obj({{"card_id",V{card.id}},{"category",V{"insight"}},
            {"name",V{known?*shared.texts.find(card.name_key):StableId{"尚未发现"}}},
            {"description",V{known?*shared.texts.find(card.description_key):StableId{}}},
            {"hint",V{*shared.texts.find(card.hint_key)}},{"discovered",V{known}},
            {"acquired_this_world",V{acquired}},{"experienced_this_world",V{acquired}},
            {"acquired_tick",acquired?V{completed.value->completed_tick}:V{std::monostate{}}},
            {"experience",acquired?experience(*completed.value):V{std::monostate{}}}}));
    }
    const auto& id=common.identity;const auto next=offset+Integer(rows.size());
    return {true,obj({{"actor_id",V{actor}},{"source_identity",obj({{"world_id",V{hex(id.world_id)}},{"execution_epoch",V{id.execution_epoch}},{"revision",V{id.revision}},{"tick",V{id.tick}}})},
        {"offset",V{offset}},{"limit",V{limit}},{"total",V{total}},{"cards",V{std::move(rows)}},
        {"next_offset",next<total?V{next}:V{std::monostate{}}}}),{}};
}
}
