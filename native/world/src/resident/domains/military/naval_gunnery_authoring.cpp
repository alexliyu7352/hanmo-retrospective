// 工法所有射程明填，基础船体伤害固定依07§7.3，不新增未经作者声明的武器或火攻。
#include "naval_gunnery_authoring.hpp"
#include "resident/model/common_model.hpp"
#include "naval_json_guard.hpp"
#include "resident/storage/common_codec.hpp"
#include <set>
namespace hanmo::resident {
Outcome<NavalGunneryPolicy> decode_naval_gunnery_policy(const BoundaryValue& v){const StableId p="military_naval/combat_policy/gunnery";auto unique=validate_naval_json_unique_keys(v,p);if(!unique.ok)return {false,{},unique.error};try{boundary::fields(v,{"profile","weapons"},{},p);NavalGunneryPolicy out;out.profile=boundary::get_string(v,"profile",p);if(out.profile!="entry_naval_skill_v1")boundary::fail("InvalidNavalGunneryPolicy",p);std::set<StableId> ids;for(const auto& r:boundary::array(boundary::field(v,"weapons",p),p+"/weapons")){boundary::fields(r,{"hull_id","minimum_range","maximum_range"},{},p);NavalGunneryWeapon w{boundary::get_string(r,"hull_id",p),boundary::get_int(r,"minimum_range",p),boundary::get_int(r,"maximum_range",p)};if(w.hull_id.empty()||!ids.insert(w.hull_id).second||w.minimum_range<1||w.maximum_range<w.minimum_range||w.maximum_range>16)boundary::fail("InvalidNavalGunneryWeapon",w.hull_id);out.weapons.push_back(std::move(w));}if(out.weapons.empty()||out.weapons.size()>128)boundary::fail("InvalidNavalGunneryPolicy",p);return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
}
