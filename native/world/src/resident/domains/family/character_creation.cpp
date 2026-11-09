#include "family_projection.hpp"
#include "resident/domains/interaction/interaction_projection.hpp"
#include "character_creation.hpp"
#include "resident/domains/growth/growth_skill_api.hpp"
#include "resident/model/common_string_hash.hpp"
#include <array>

namespace hanmo::resident {
namespace {
using Attribute=std::pair<const char*,Integer ActorAttributes::*>;
const std::array<Attribute,5> attributes{{
    {"leadership",&ActorAttributes::leadership},{"martial",&ActorAttributes::martial},
    {"intellect",&ActorAttributes::intellect},{"administration",&ActorAttributes::administration},
    {"charisma",&ActorAttributes::charisma}
}};
bool valid_name(const StableId& name){
    if(name.empty()||static_cast<unsigned char>(name.front())<=32||static_cast<unsigned char>(name.back())<=32)return false;
    std::size_t count=0;
    for(const auto raw:name){const auto byte=static_cast<unsigned char>(raw);if(byte<32||byte==127)return false;if((byte&0xc0)!=0x80&&++count>80)return false;}
    return godot_string_hash_utf8(name).ok;
}
}
StableId character_creation_mode_name(CharacterCreationMode mode){
    if(mode==CharacterCreationMode::standard)return "standard";
    if(mode==CharacterCreationMode::free)return "free";
    return {};
}
Outcome<CharacterCreationBudget> inspect_character_creation_budget(const CharacterCreationDraft& draft,bool extended){
    const auto fail=[](const StableId& code,const StableId& path=StableId{})->Outcome<CharacterCreationBudget>{return {false,{},common_failure(code,path).error};};
    if(character_creation_mode_name(draft.mode).empty())return fail("InvalidCharacterCreationMode");
    const bool standard=draft.mode==CharacterCreationMode::standard;
    if(draft.skills.extended.has_value()!=extended)return fail("CharacterCreationSkillProfileMismatch");
    CharacterCreationBudget result;
    for(const auto& item:attributes){const auto value=draft.attributes.*item.second;if(value<(standard?20:1)||value>(standard?85:100))return fail("CharacterCreationAttributeRange",item.first);result.attribute_total+=value;}
    for(const auto& id:growth_skill_ids(extended)){
        const auto xp=growth_skill_xp(draft.skills,id);if(xp<0||xp>1200)return fail("CharacterCreationSkillRange",id);
        // 标准初始点买的是完整等级，不能夹带靠近下级门槛的额外经验。
        if(standard&&xp!=0&&xp!=100&&xp!=300)return fail("CharacterCreationSkillPointsRequired",id);
        result.skill_level_total+=growth_skill_level(xp);
    }
    result.attribute_budget_complete=!standard||result.attribute_total==300;
    result.skill_budget_complete=!standard||result.skill_level_total==4;
    return {true,result,{}};
}
Status validate_character_creation_draft(const CharacterCreationDraft& draft,bool extended){
    if(!valid_name(draft.display_name))return common_failure("InvalidActorDisplayName");
    if(draft.age_years<18||draft.age_years>(draft.mode==CharacterCreationMode::standard?60:120))return common_failure("CharacterCreationAgeRange");
    const auto budget=inspect_character_creation_budget(draft,extended);if(!budget.ok)return {false,{},budget.error};
    if(!budget.value.attribute_budget_complete)return common_failure("CharacterCreationAttributeBudget");
    if(!budget.value.skill_budget_complete)return common_failure("CharacterCreationSkillBudget");
    return common_success();
}
Outcome<CharacterCreationDraft> decode_character_creation_draft(const BoundaryValue& value,bool extended){try{
    using namespace boundary;
    fields(value,{"creation_mode","display_name","age_years","attributes","skills"},{},"character_creation");
    CharacterCreationDraft draft;const auto mode=get_string(value,"creation_mode","character_creation");
    if(mode=="standard")draft.mode=CharacterCreationMode::standard;else if(mode=="free")draft.mode=CharacterCreationMode::free;else fail("InvalidCharacterCreationMode","character_creation/creation_mode");
    draft.display_name=get_string(value,"display_name","character_creation");draft.age_years=get_int(value,"age_years","character_creation");
    const auto& attrs=field(value,"attributes","character_creation");fields(attrs,{"leadership","martial","intellect","administration","charisma"},{},"character_creation/attributes");
    for(const auto& item:attributes)draft.attributes.*item.second=get_int(attrs,item.first,"character_creation/attributes");
    const auto& skills=field(value,"skills","character_creation");const auto& ids=growth_skill_ids(extended);
    if(object(skills,"character_creation/skills").size()!=ids.size())fail("CharacterCreationSkillProfileMismatch","character_creation/skills");
    if(extended)draft.skills.extended.emplace();
    for(const auto& id:ids){auto result=growth_set_skill(draft.skills,id,get_int(skills,id,"character_creation/skills"));if(!result.ok)return {false,{},result.error};}
    draft.field_order=order(value);draft.attributes_order=order(attrs);draft.skills_order=order(skills);
    auto valid=validate_character_creation_draft(draft,extended);if(!valid.ok)return {false,{},valid.error};return {true,std::move(draft),{}};
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
Status emit_character_creation_draft_fields(ProjectionWriter& out,const CharacterCreationDraft& draft,bool extended){out.field("creation_mode",character_creation_mode_name(draft.mode));out.field("display_name",draft.display_name);out.field("age_years",draft.age_years);out.object("attributes",draft.attributes_order,[&](auto& w){for(const auto& item:attributes)w.field(item.first,draft.attributes.*item.second);return w.status();});out.object("skills",draft.skills_order,[&](auto& w){for(const auto& id:growth_skill_ids(extended))w.field(id,growth_skill_xp(draft.skills,id));return w.status();});return out.order_fields(draft.field_order);}
BoundaryValue encode_character_creation_draft(const CharacterCreationDraft& draft,bool extended){return family_projection::encode([&](auto& w){return emit_character_creation_draft_fields(w,draft,extended);});}
}
