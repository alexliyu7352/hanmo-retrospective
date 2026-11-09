// Godot4.7.2 String.hash的UTF-32 DJB2加法语义；UTF-8只在进入此纯叶时逐字符解码。
// 算法核对固定引擎ed1daf0bf001b61586d9930840f2f1394092c079/core/string/ustring.cpp:2606。
// Godot Engine contributors，MIT许可证见common_rng_LICENSE.txt；不使用宿主std::hash。
#pragma once
#include "common_model.hpp"
namespace hanmo::resident {
// UTF-8必须是已验证的Unicode标量；错误不替换字符后继续生成另一个战场随机种子。
inline Outcome<Integer> godot_string_hash_utf8(const StableId& text){std::uint32_t hash=5381;for(std::size_t i=0;i<text.size();){auto first=static_cast<unsigned char>(text[i++]);if(first==0)break;std::uint32_t cp=0;unsigned more=0;if(first<0x80)cp=first;else if(first>=0xc2&&first<=0xdf){cp=first&31;more=1;}else if(first>=0xe0&&first<=0xef){cp=first&15;more=2;}else if(first>=0xf0&&first<=0xf4){cp=first&7;more=3;}else return {false,{},common_failure("InvalidResidentString","hash/utf8").error};auto width=more;for(unsigned n=0;n<more;++n){if(i>=text.size())return {false,{},common_failure("InvalidResidentString","hash/utf8").error};auto next=static_cast<unsigned char>(text[i++]);if((next&0xc0)!=0x80)return {false,{},common_failure("InvalidResidentString","hash/utf8").error};cp=(cp<<6)|(next&63);}if((width==1&&cp<0x80)||(width==2&&cp<0x800)||(width==3&&cp<0x10000)||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff))return {false,{},common_failure("InvalidResidentString","hash/utf8").error};hash=hash*33u+cp;}return {true,static_cast<Integer>(hash),{}};}
}
