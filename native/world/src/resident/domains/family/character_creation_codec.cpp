#include "family_projection.hpp"
#include "resident/domains/interaction/interaction_projection.hpp"
#include "character_creation_source.hpp"
#include "resident/domains/growth/growth_skill_api.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include <array>
#include <set>
namespace hanmo::resident {
namespace {
using Attribute=std::pair<const char*,Integer ActorAttributes::*>;
const std::array<Attribute,5> attributes{{{"leadership",&ActorAttributes::leadership},{"martial",&ActorAttributes::martial},{"intellect",&ActorAttributes::intellect},{"administration",&ActorAttributes::administration},{"charisma",&ActorAttributes::charisma}}};
using Feature=std::pair<const char*,StableId AppearanceFeatures::*>;
const std::array<Feature,9> features{{{"face_shape",&AppearanceFeatures::face_shape},{"eyes",&AppearanceFeatures::eyes},{"brows",&AppearanceFeatures::brows},{"nose",&AppearanceFeatures::nose},{"mouth",&AppearanceFeatures::mouth},{"hair",&AppearanceFeatures::hair},{"beard",&AppearanceFeatures::beard},{"skin_palette",&AppearanceFeatures::skin_palette},{"build",&AppearanceFeatures::build}}};
// 顺序是可持久化的导出协议：空序列沿原默认，非空必须是完整唯一键集合。
bool valid_order(const FieldOrder& order,const std::vector<StableId>& expected){
    if(order.empty())return true;if(order.size()!=expected.size())return false;
    std::set<StableId> seen;for(const auto& key:order)if(std::find(expected.begin(),expected.end(),key)==expected.end()||!seen.insert(key).second)return false;return true;
}
Status visit_draft(CodecVisitor& c,CharacterCreationDraft& draft,bool extended){
    auto mode=character_creation_mode_name(draft.mode);c.field("creation_mode",mode);
    if(mode=="standard")draft.mode=CharacterCreationMode::standard;else if(mode=="free")draft.mode=CharacterCreationMode::free;else return c.reject("InvalidCharacterCreationMode");
    c.field("display_name",draft.display_name);c.field("age_years",draft.age_years);
    c.object("attributes",[&](auto& v){for(const auto& item:attributes)v.field(item.first,draft.attributes.*item.second);return v.status();});
    if(c.reading()){draft.skills.extended.reset();if(extended)draft.skills.extended.emplace();}
    if(draft.skills.extended.has_value()!=extended)return c.reject("CharacterCreationSkillProfileMismatch");
    c.object("skills",[&](auto& v){for(const auto& id:growth_skill_ids(extended)){auto xp=growth_skill_xp(draft.skills,id);v.field(id.c_str(),xp);if(v.reading()){auto result=growth_set_skill(draft.skills,id,xp);if(!result.ok)return v.reject(result.error.original.code);}}return v.status();});
    common_checkpoint::strings(c,"field_order",draft.field_order);common_checkpoint::strings(c,"attributes_order",draft.attributes_order);common_checkpoint::strings(c,"skills_order",draft.skills_order);
    if(!c.status().ok)return c.status();
    if(!valid_order(draft.field_order,{"creation_mode","display_name","age_years","attributes","skills"})||!valid_order(draft.attributes_order,{"leadership","martial","intellect","administration","charisma"})||!valid_order(draft.skills_order,growth_skill_ids(extended)))return c.reject("BoundaryFieldOrderMismatch");
    auto valid=validate_character_creation_draft(draft,extended);return valid.ok?c.status():c.reject(valid.error.original.code);
}
}
Outcome<CharacterCreationCatalog> decode_character_creation_catalog(const BoundaryValue& data,const StableId& scenario_id){try{
    using namespace boundary;const auto& scenario=field(field(data,"scenarios","data"),scenario_id,"scenarios");const auto& value=field(scenario,"character_creation",scenario_id);
    fields(value,{"profile","prototype_actor_id","starting_place_id","allowed_modes","initial_money","initial_goods","appearance_choices"},{},"character_creation");
    if(get_string(value,"profile","")!="point_buy_character_v1")unsupported("UnsupportedCharacterCreationProfile","character_creation/profile");
    if(get_string(field(scenario,"dynamic_world",scenario_id),"founder_policy",scenario_id)!="created_character_v1")fail("CharacterCreationFounderPolicyRequired",scenario_id);
    for(const auto* name:{"playable_actor_ids","playable_actor_appearance_policies","player_start_descriptions","player_appearance_choices","actor_card_unlocks"})if(const auto* item=scenario.find(name))if(!item->as<BoundaryValue::Array>()||!item->as<BoundaryValue::Array>()->empty())fail("CharacterCreationExistingActorConflict",name);
    CharacterCreationCatalog result;result.prototype_actor_id=get_string(value,"prototype_actor_id","");result.starting_place_id=get_string(value,"starting_place_id","");result.initial_money=get_int(value,"initial_money","");
    const auto& prototype=field(field(data,"actors","data"),result.prototype_actor_id,"actors");
    if(get_string(prototype,"origin",result.prototype_actor_id)!="original"||get_string(scenario,"player_actor",scenario_id)!=result.prototype_actor_id)fail("CharacterCreationOriginalPrototypeRequired",scenario_id);
    const auto& enabled=field(scenario,"enabled",scenario_id);const auto actors=strings(field(enabled,"actors",scenario_id),scenario_id);const auto places=strings(field(enabled,"places",scenario_id),scenario_id);
    if(std::find(actors.begin(),actors.end(),result.prototype_actor_id)!=actors.end())fail("CharacterCreationPrototypeAlreadyActive",result.prototype_actor_id);
    if(std::find(places.begin(),places.end(),result.starting_place_id)==places.end())fail("CharacterCreationStartingPlaceRequired",result.starting_place_id);
    for(const auto& actor:array(field(field(scenario,"initial",scenario_id),"actors",scenario_id),scenario_id))if(get_string(actor,"actor_id",scenario_id)==result.prototype_actor_id)fail("CharacterCreationPrototypeAlreadyActive",result.prototype_actor_id);
    const auto& modes=array(field(value,"allowed_modes",""),"character_creation/allowed_modes");std::set<StableId> seen;
    for(const auto& mode:modes){const auto name=string(mode,"allowed_modes");if(!seen.insert(name).second)fail("InvalidCharacterCreationModes","character_creation/allowed_modes");if(name=="standard")result.allowed_modes.push_back(CharacterCreationMode::standard);else if(name=="free")result.allowed_modes.push_back(CharacterCreationMode::free);else fail("InvalidCharacterCreationModes","character_creation/allowed_modes");}
    if(!seen.count("standard")||seen.size()>2)fail("InvalidCharacterCreationModes","character_creation/allowed_modes");
    const auto& goods=array(field(value,"initial_goods",""),"initial_goods");if(goods.size()>20)fail("InvalidCharacterCreationGoods","initial_goods");
    for(const auto& row:goods){fields(row,{"item_id","quantity"},{},"initial_goods");result.initial_goods.push_back({get_string(row,"item_id",""),get_int(row,"quantity","")});}
    const auto& choices=array(field(value,"appearance_choices",""),"appearance_choices");if(choices.empty()||choices.size()>4096)fail("CharacterCreationAppearanceRequired","character_creation/appearance_choices");
    for(const auto& row:choices){fields(row,{"id","base_features","attire_style"},{},"appearance_choices");CharacterCreationAppearance appearance;appearance.id=get_string(row,"id","");appearance.attire_style=get_string(row,"attire_style","");const auto& base=field(row,"base_features","");fields(base,{"face_shape","eyes","brows","nose","mouth","hair","beard","skin_palette","build"},{},"base_features");appearance.features.present=true;appearance.features.field_order=order(base);for(const auto& feature:features)appearance.features.*feature.second=get_string(base,feature.first,"");const auto appearance_id=appearance.id;if(!result.appearance_choices.insert(appearance_id,std::move(appearance)))fail("DuplicateCharacterCreationAppearance","character_creation/appearance_choices");}
    return {true,std::move(result),{}};
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
CharacterCreationFact decode_character_creation_fact(const BoundaryValue& value,bool extended){
    using namespace boundary;fields(value,{"kind","actor_id","prototype_actor_id","scenario_id","appearance_choice_id","created_tick","draft"},{},"character_creation_fact");
    CharacterCreationFact f;f.kind=get_string(value,"kind","");f.actor_id=get_string(value,"actor_id","");f.prototype_actor_id=get_string(value,"prototype_actor_id","");f.scenario_id=get_string(value,"scenario_id","");f.appearance_choice_id=get_string(value,"appearance_choice_id","");f.created_tick=get_int(value,"created_tick","");
    auto draft=decode_character_creation_draft(field(value,"draft",""),extended);if(!draft.ok)fail(draft.error.original.code,draft.error.original.path);f.draft=std::move(draft.value);f.field_order=order(value);return f;
}
Status emit_character_creation_fact_fields(ProjectionWriter& out,const CharacterCreationFact& f,bool extended){out.field("kind",f.kind);out.field("actor_id",f.actor_id);out.field("prototype_actor_id",f.prototype_actor_id);out.field("scenario_id",f.scenario_id);out.field("appearance_choice_id",f.appearance_choice_id);out.field("created_tick",f.created_tick);out.object("draft",[&](auto& w){return emit_character_creation_draft_fields(w,f.draft,extended);});return out.order_fields(f.field_order);}
BoundaryValue encode_character_creation_fact(const CharacterCreationFact& f,bool extended){return family_projection::encode([&](auto& w){return emit_character_creation_fact_fields(w,f,extended);});}
Status visit_character_creation_fact(CodecVisitor& c,CharacterCreationFact& f,bool extended){c.field("kind",f.kind);c.field("actor_id",f.actor_id);c.field("prototype_actor_id",f.prototype_actor_id);c.field("scenario_id",f.scenario_id);c.field("appearance_choice_id",f.appearance_choice_id);c.field("created_tick",f.created_tick);c.object("draft",[&](auto& v){return visit_draft(v,f.draft,extended);});common_checkpoint::strings(c,"field_order",f.field_order);if(!c.status().ok)return c.status();return valid_order(f.field_order,{"kind","actor_id","prototype_actor_id","scenario_id","appearance_choice_id","created_tick","draft"})?c.status():c.reject("BoundaryFieldOrderMismatch");}
}
