// 原character_input的有限投影：先身份、再外貌，不能用sidebar空名回退掩盖无效人物。
#include "character_presentation.hpp"
#include "../storage/common_codec.hpp"

namespace hanmo::resident {
namespace {
// Godot的有界名字/seed按Unicode码点计；<=32边缘和ASCII控制符原样拒绝。
bool valid_saved_text(const std::string& text,std::size_t limit){
    if(text.empty()||static_cast<unsigned char>(text.front())<=32||static_cast<unsigned char>(text.back())<=32)return false;
    std::size_t count=0;
    for(std::size_t i=0;i<text.size();){
        const auto lead=static_cast<unsigned char>(text[i++]);std::uint32_t scalar=0;unsigned trailing=0;
        if(lead<0x80)scalar=lead;
        else if(lead>=0xc2&&lead<=0xdf){scalar=lead&0x1f;trailing=1;}
        else if(lead>=0xe0&&lead<=0xef){scalar=lead&0x0f;trailing=2;}
        else if(lead>=0xf0&&lead<=0xf4){scalar=lead&0x07;trailing=3;}
        else return false;
        if(text.size()-i<trailing)return false;
        for(unsigned n=0;n<trailing;++n){const auto byte=static_cast<unsigned char>(text[i++]);if((byte&0xc0)!=0x80)return false;scalar=(scalar<<6)|(byte&0x3f);}
        if((trailing==1&&scalar<0x80)||(trailing==2&&scalar<0x800)||(trailing==3&&scalar<0x10000)||scalar>0x10ffff||(scalar>=0xd800&&scalar<=0xdfff))return false;
        if(scalar<32||scalar==127||++count>limit)return false;
    }
    return true;
}
// 九槽和服饰沿原ASCII语义token，不接受资源路径或任意额外字段。
bool token(const StableId& value){
    if(value.empty()||value.size()>64)return false;
    for(std::size_t i=0;i<value.size();++i){const auto c=static_cast<unsigned char>(value[i]);if(!(c>='a'&&c<='z')&&(i==0||!((c>='0'&&c<='9')||c=='-'||c=='.'||c=='_')))return false;}
    return true;
}
// 只校验当前已准入的完整档案；继承来源仍由原共同profile明确拒绝，读者不新增支持。
Status appearance_valid(const AppearanceProfile& p){
    if(p.format_version!=1)return common_failure("InvalidAppearanceProfile");
    if(p.source=="legacy")return p.generator_version=="legacy-portrait-v1"&&p.generation_seed=="not-sampled"&&!p.base_features.present&&p.attire_style=="legacy"&&p.parent_sources.empty()?common_success():common_failure("InvalidLegacyAppearance");
    if(p.generator_version!="appearance-v1")return common_failure("UnsupportedAppearanceGenerator");
    if(p.source=="inherited")return common_failure("AppearanceInheritanceCapabilityRequired",{},ErrorKind::unsupported_capability);
    if((p.source!="fixed"&&p.source!="custom"&&p.source!="generated")||!p.base_features.present||!token(p.attire_style))return common_failure("InvalidAppearanceProfile");
    const auto& f=p.base_features;
    for(const auto* value:{&f.face_shape,&f.eyes,&f.brows,&f.nose,&f.mouth,&f.hair,&f.beard,&f.skin_palette,&f.build})if(!token(*value))return common_failure("InvalidAppearanceProfile");
    if(p.source=="fixed"||p.source=="custom"){if(p.generation_seed!="not-sampled")return common_failure("InvalidAppearanceSeed");}
    else if(!valid_saved_text(p.generation_seed,256))return common_failure("InvalidAppearanceSeed");
    return p.parent_sources.empty()?common_success():common_failure("InvalidAppearanceParents");
}
// query原错误没有部分视图；路径只加内部定位，不回传被拒人物或私密档案。
Outcome<CharacterPresentationView> invalid_actor(const StableId& actor){return {false,{},common_failure("InvalidPresentationActor",actor).error};}
} // 不在只读投影中生成外貌、修复档案、补生日或枚举其它人物。

Outcome<CharacterPresentationView> CommonCharacterPresentation::character(const StableId& actor_id) const{
    const auto* actor=state_.actors.find(actor_id);if(!actor)return invalid_actor(actor_id);
    const auto* definition=catalog_.actor_names.find(actor->prototype_id?*actor->prototype_id:actor_id);
    if(!definition||(actor->display_name&&!valid_saved_text(*actor->display_name,80)))return invalid_actor(actor_id);
    CharacterPresentationView view;auto& public_actor=view.actor_view;public_actor.actor_id=actor_id;
    const auto* authored=catalog_.texts.find(definition->name_key);
    public_actor.display_name=actor->display_name?*actor->display_name:(authored?*authored:actor_id);
    public_actor.presentation_profile_id=definition->presentation_profile_id;public_actor.portrait_kind=definition->portrait_kind;
    public_actor.visibility_version=state_.identity.revision;
    if(actor->birth_tick&&*actor->birth_tick>=-2147483647&&*actor->birth_tick<=state_.identity.tick){
        // 只在差值非负时相减；用无符号中间值避免畸形typed直装导致有符号溢出。
        const auto elapsed=static_cast<std::uint64_t>(state_.identity.tick)-static_cast<std::uint64_t>(*actor->birth_tick);
        public_actor.age_years=static_cast<Integer>(elapsed/720);
    }
    auto& visible=view.appearance_view;
    if(!actor->appearance){visible.generator_version="legacy-portrait-v1";visible.attire_style="legacy";visible.legacy=true;return {true,std::move(view),{}};}
    const auto& profile=*actor->appearance;auto valid=appearance_valid(profile);if(!valid.ok)return {false,{},valid.error};
    visible.format_version=profile.format_version;visible.generator_version=profile.generator_version;
    visible.base_features=profile.base_features;visible.attire_style=profile.attire_style;visible.legacy=profile.source=="legacy";
    return {true,std::move(view),{}};
}

Outcome<BoundaryValue> encode_character_presentation(const CharacterPresentationView& input){
    try {
        using namespace boundary;const auto& a=input.actor_view;const auto& p=input.appearance_view;const auto& f=p.base_features;
        BoundaryValue features{BoundaryValue::Dict{}};
        if(f.present)features=ordered(obj({{"face_shape",BoundaryValue{f.face_shape}},{"eyes",BoundaryValue{f.eyes}},{"brows",BoundaryValue{f.brows}},{"nose",BoundaryValue{f.nose}},{"mouth",BoundaryValue{f.mouth}},{"hair",BoundaryValue{f.hair}},{"beard",BoundaryValue{f.beard}},{"skin_palette",BoundaryValue{f.skin_palette}},{"build",BoundaryValue{f.build}}}),f.field_order);
        return {true,obj({{"ok",BoundaryValue{true}},{"actor_view",obj({{"actor_id",BoundaryValue{a.actor_id}},{"display_name",BoundaryValue{a.display_name}},{"age_years",BoundaryValue{a.age_years}},{"presentation_profile_id",BoundaryValue{a.presentation_profile_id}},{"portrait_kind",BoundaryValue{a.portrait_kind}},{"visibility_version",BoundaryValue{a.visibility_version}}})},{"appearance_view",obj({{"format_version",BoundaryValue{p.format_version}},{"generator_version",BoundaryValue{p.generator_version}},{"base_features",std::move(features)},{"attire_style",BoundaryValue{p.attire_style}},{"legacy",BoundaryValue{p.legacy}}})}}),{}};
    }catch(const boundary::DecodeFailure& e){return {false,{},e.error};}
}
} // 唯一边界只拷贝白名单，内部appearance字段不会因增加模型成员而自动扩散。
