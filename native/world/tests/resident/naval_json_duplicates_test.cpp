// 新舟战入口逐层拒重键；旧资产段字节和原入口不借此测试冒完整World验收。
#include "resident/domains/military/naval_codec.hpp"
#include "resident/domains/military/naval_asset_codec.hpp"
#include "resident/domains/military/naval_battle_command_codec.hpp"
#include "resident/storage/common_codec.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {int checks=0;void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}using V=BoundaryValue;
V duplicate(const char* key){return boundary::obj({{key,V{StableId{"one"}}},{key,V{StableId{"two"}}}});}
template<class T>void rejected(const Outcome<T>& r,const char* why){check(!r.ok&&r.error.original.code=="DuplicateNavalJsonKey",why);}
}
int main(){try{
 NavalCatalog cat;cat.combat_policy=NavalBattlePolicy{};cat.combat_policy->kind="boarding_ram_v1";NavalState empty;
 V clean=boundary::obj({{"assets",naval_asset_codec::encode(empty)},{"battle",naval_battle_codec::encode(empty.battle)}});
 check(naval_codec::decode(clean,cat).ok,"valid empty Naval2 still reads");
 auto root=clean;root.as<V::Dict>()->push_back(root.as<V::Dict>()->front());rejected(naval_codec::decode(root,cat),"duplicate Naval root denied before field selection");
 for(const char* field:{"id","owner","anchor"}){auto nested=clean;V bad=duplicate(field);if(StableId{field}=="owner")bad=boundary::obj({{"owner",duplicate("id")}});if(StableId{field}=="anchor")bad=boundary::obj({{"anchor",duplicate("map_id")}});nested.as<V::Dict>()->at(1).second=boundary::obj({{"records",V{V::Array{bad}}}});rejected(naval_codec::decode(nested,cat),"record and nested Owner/Location duplicate denied recursively");}
 auto asset=clean;V badowner=boundary::obj({{"owner",duplicate("id")}});asset.as<V::Dict>()->at(0).second=boundary::obj({{"vessels",V{V::Array{badowner}}}});rejected(naval_codec::decode(asset,cat),"Naval2 asset Owner duplicate denied too");
 rejected(naval_battle_codec::decode(duplicate("battles"),true),"direct battle root denied");
 auto orders=boundary::obj({{"unit",boundary::obj({{"action",V{StableId{"hold"}}},{"target_unit_id",V{StableId{}}},{"destination",V{V::Array{V{Integer{0}},V{Integer{0}}}}},{"heading",V{Integer{0}}}})}});
 check(naval_battle_codec::decode_orders(orders).ok,"clean tactical orders decode");auto badorders=orders;badorders.as<V::Dict>()->push_back(badorders.as<V::Dict>()->front());rejected(naval_battle_codec::decode_orders(badorders),"duplicate unit order denied");
 V command=boundary::obj({{"kind",V{StableId{"naval_battle_plan"}}},{"battle_id",V{StableId{"battle"}}},{"request_key",V{StableId{"key"}}},{"orders",badorders}});rejected(naval_battle_command_codec::decode_command(command,2),"command nested order duplicate denied");
 NavalBattleCommandReceipt receipt;receipt.kind="naval_battle_plan";receipt.request_key="key";receipt.receipt_id="naval_plan:7";receipt.battle_id="naval_battle:4";receipt.actor_id="captain";auto encoded=naval_battle_command_codec::encode_receipt(receipt,2);encoded.as<V::Dict>()->push_back(encoded.as<V::Dict>()->front());rejected(naval_battle_command_codec::decode_receipt(encoded,2),"duplicate receipt kind denied");
 auto disabled=naval_battle_command_codec::decode_command(command,1);check(!disabled.ok&&disabled.error.original.code!="DuplicateNavalJsonKey","old profile gate still takes precedence");
 NavalCatalog old;auto oldvalue=naval_asset_codec::encode(empty);auto restored=naval_codec::decode(oldvalue,old);check(restored.ok&&compare_boundary(naval_asset_codec::encode(restored.value),oldvalue).exact(),"old Naval1 clean bytes unchanged");
 std::cout<<"NAVAL_JSON_DUPLICATES="<<checks<<" FAILURES=0 SCOPE=DECODE_COMPONENT\n";return 0;
 }catch(const std::exception& e){std::cerr<<"checks="<<checks<<" "<<e.what()<<'\n';return 1;}}
