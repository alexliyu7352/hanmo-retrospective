// Family段唯一有限版本映射：旧1..16不变，新生前路径只为原9..16增加16。
#pragma once
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
struct FamilyCodecSelection {
    bool household_control=false,character_creation=false,adult_adoption=false;
    bool voluntary_succession=false,voluntary_paths=false;
};
// selected/inspect/JSON/typed入口共同调用；不借Common schema_features猜Family形状。
inline Outcome<Integer> select_family_codec_version(const FamilyCodecSelection& s){
    if(s.voluntary_paths&&!s.voluntary_succession)
        return {false,{},common_failure("VoluntaryPathSuccessionPairRequired").error};
    return {true,1+(s.household_control?1:0)+(s.character_creation?2:0)+(s.adult_adoption?4:0)+
                   (s.voluntary_succession?8:0)+(s.voluntary_paths?16:0),{}};
}
inline Outcome<FamilyCodecSelection> inspect_family_codec_version(Integer version){
    const bool paths=version>=25&&version<=32;
    if(!paths&&(version<1||version>16))return {false,{},common_failure("UnsupportedFamilyCodecVersion").error};
    const Integer flags=(paths?version-16:version)-1;
    return {true,{(flags&1)!=0,(flags&2)!=0,(flags&4)!=0,(flags&8)!=0,paths},{}};
}
// 已解析的原manifest模块，未出现用nullopt；Known版本由本次目录的原严格读确定。
inline Status validate_family_codec_module_pair(Integer family_version,
    std::optional<Integer> voluntary_version,std::optional<Integer> paths_version,
    std::optional<Integer> known_personal_version){
    auto shape=inspect_family_codec_version(family_version);if(!shape.ok)return {false,{},shape.error};
    if(shape.value.voluntary_paths){
        if(voluntary_version!=std::optional<Integer>{2}||paths_version!=std::optional<Integer>{1}||
           known_personal_version!=std::optional<Integer>{1})return common_failure("VoluntaryPathSourcePairRequired");
    }else if(paths_version||voluntary_version!=(shape.value.voluntary_succession?std::optional<Integer>{1}:std::nullopt))
        return common_failure("FamilyVoluntaryCodecModuleMismatch");
    return common_success();
}
}
