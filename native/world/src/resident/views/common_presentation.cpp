// 原身份名字优先级的窄实现；作者文本不截断、不修整、不被运行名字覆盖。
#include "common_presentation.hpp"

namespace hanmo::resident {
namespace {
// Godot4.7.2的String长度按码点计，strip_edges只裁<=32；不把UTF-8字节数当名字长度。
bool valid_saved_name(const std::string& name){
    if(name.empty()||static_cast<unsigned char>(name.front())<=32||static_cast<unsigned char>(name.back())<=32)return false;
    std::size_t count=0;
    for(std::size_t i=0;i<name.size();){
        const auto lead=static_cast<unsigned char>(name[i++]);std::uint32_t scalar=0;unsigned trailing=0;
        if(lead<0x80)scalar=lead;
        else if(lead>=0xc2&&lead<=0xdf){scalar=lead&0x1f;trailing=1;}
        else if(lead>=0xe0&&lead<=0xef){scalar=lead&0x0f;trailing=2;}
        else if(lead>=0xf0&&lead<=0xf4){scalar=lead&0x07;trailing=3;}
        else return false;
        if(name.size()-i<trailing)return false;
        for(unsigned n=0;n<trailing;++n){const auto byte=static_cast<unsigned char>(name[i++]);if((byte&0xc0)!=0x80)return false;scalar=(scalar<<6)|(byte&0x3f);}
        if((trailing==1&&scalar<0x80)||(trailing==2&&scalar<0x800)||(trailing==3&&scalar<0x10000)||scalar>0x10ffff||(scalar>=0xd800&&scalar<=0xdfff))return false;
        if(scalar<32||scalar==127||++count>80)return false;
    }
    return true;
}
} // UTF-8检查只防typed直装无效字符串；不扩大世界身份准入策略。

Outcome<std::string> CommonPresentation::display_name(const StableId& actor_id) const{
    // 无效definition为空字典，原sidebar接着查texts[""]，不存在时才是空字符串。
    const auto fallback=[&]()->Outcome<std::string>{const auto* value=catalog_.texts.find("");return {true,value?*value:std::string{},{}};};
    const auto* actor=state_.actors.find(actor_id);if(!actor)return fallback();
    const auto& prototype=actor->prototype_id?*actor->prototype_id:actor_id;
    const auto* definition=catalog_.actor_names.find(prototype);if(!definition)return fallback();
    if(actor->display_name){if(!valid_saved_name(*actor->display_name))return fallback();return {true,*actor->display_name,{}};}
    const auto* authored=catalog_.texts.find(definition->name_key);
    return {true,authored?*authored:actor_id,{}};
}

Outcome<std::string> CommonPresentation::text(const StableId& text_key) const{
    const auto* value=catalog_.texts.find(text_key);if(value)return {true,*value,{}};
    auto error=common_failure("MissingPresentationText","texts/"+text_key,ErrorKind::internal).error;error.domain="presentation";
    return {false,{},std::move(error)};
}
} // 调用者拥有观察者权限、说话者替换与选项条件；该读取器不决定哪些人物可见。
