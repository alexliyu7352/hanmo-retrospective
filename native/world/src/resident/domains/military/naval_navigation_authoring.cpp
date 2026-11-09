// 每项周期与远航阈值都须作者明示；未知字段/重名/未来profile不能静默变成默认规则。
#include "naval_navigation_authoring.hpp"
#include "naval_json_guard.hpp"
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident {
Outcome<NavalNavigationPolicy> decode_naval_navigation_policy(const BoundaryValue& value){auto unique=validate_naval_json_unique_keys(value,"military_naval/navigation_policy");if(!unique.ok)return {false,{},unique.error};try{const StableId p="military_naval/navigation_policy";boundary::fields(value,{"profile","wear_interval","wear_damage","long_voyage_min_half_days","long_voyage_min_integrity"},{},p);NavalNavigationPolicy out;out.profile=boundary::get_string(value,"profile",p);out.wear_interval=boundary::get_int(value,"wear_interval",p);out.wear_damage=boundary::get_int(value,"wear_damage",p);out.long_voyage_min_half_days=boundary::get_int(value,"long_voyage_min_half_days",p);out.long_voyage_min_integrity=boundary::get_int(value,"long_voyage_min_integrity",p);return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
}
