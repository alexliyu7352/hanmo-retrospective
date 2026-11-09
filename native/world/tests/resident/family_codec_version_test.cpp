#include "resident/domains/family/family_codec_version.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {Integer checks=0;void check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}}
int main(){try{
    // 直接消费产品helper，并与已经发布的16个固定版本逐值比；不在测试中复制另一解码器。
    const FamilyCodecSelection selections[]={
        {false,false,false,false,false},{true,false,false,false,false},
        {false,true,false,false,false},{true,true,false,false,false},
        {false,false,true,false,false},{true,false,true,false,false},
        {false,true,true,false,false},{true,true,true,false,false},
        {false,false,false,true,false},{true,false,false,true,false},
        {false,true,false,true,false},{true,true,false,true,false},
        {false,false,true,true,false},{true,false,true,true,false},
        {false,true,true,true,false},{true,true,true,true,false}};
    for(Integer i=0;i<16;++i){auto v=select_family_codec_version(selections[i]);check(v.ok&&v.value==i+1,"original version mapping preserved");
        auto read=inspect_family_codec_version(v.value);check(read.ok&&select_family_codec_version(read.value).value==v.value,"original inspect round trip");
        check(validate_family_codec_module_pair(v.value,i<8?std::nullopt:std::optional<Integer>{1},{},{}).ok,"original modules accepted without new source");}
    for(Integer i=8;i<16;++i){auto s=selections[i];s.voluntary_paths=true;auto v=select_family_codec_version(s);
        check(v.ok&&v.value==i+17,"only existing voluntary version maps plus16");
        check(inspect_family_codec_version(v.value).ok,"new version accepted by same inspect");
        check(validate_family_codec_module_pair(v.value,2,1,1).ok,"complete strict source pair accepted");
        check(!validate_family_codec_module_pair(v.value,1,1,1).ok,"old module cannot read new format");
        check(!validate_family_codec_module_pair(v.value,2,{},1).ok,"new policy tail is required");
        check(!validate_family_codec_module_pair(v.value,2,1,{}).ok,"real Known source module is required");}
    for(Integer v=17;v<=24;++v)check(!inspect_family_codec_version(v).ok,"reserved hole rejected");
    check(!inspect_family_codec_version(0).ok,"zero version rejected");check(!inspect_family_codec_version(33).ok,"unknown later version rejected");
    auto bad=selections[0];bad.voluntary_paths=true;check(!select_family_codec_version(bad).ok,"new path cannot omit original voluntary control");
    check(!validate_family_codec_module_pair(16,1,1,1).ok,"new tail cannot masquerade as old Family");
    std::cout<<"FAMILY_CODEC_VERSION "<<checks<<" checks 0 failures\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
