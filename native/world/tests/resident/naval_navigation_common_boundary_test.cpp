// Common只准入已登记Naval5及Naval6准备事实种类；真实领域来源仍由完整Naval历史后验。
#include "resident/storage/common_codec.hpp"
#include "resident/storage/common_store.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {int checks=0;
void check(bool value,const char* label){++checks;if(!value)throw std::runtime_error(label);}
Outcome<CommonState> decode(const StableId& kind,const StableId& domain){CommonState state;state.metadata.ruleset_id=dynamic_ruleset_id;state.next_instance_id=3;state.facts.insert("naval:1",{"naval:1",kind,domain,{}});auto encoded=encode_common_state(state);std::get<BoundaryValue::Dict>(encoded.data).emplace_back("results",BoundaryValue{BoundaryValue::Dict{}});auto* fact=encoded.find("facts")->find("naval:1");std::get<BoundaryValue::Dict>(fact->data).emplace_back("domain",BoundaryValue{domain});return decode_common_seed(encoded,CommonCatalog{},nullptr,CommonSeedValidation::defer_until_bundle);}
}
int main(){try{
 for(const auto& kind:{"naval_voyage","naval_navigation_step","naval_repair_result","naval_form","naval_gunnery_preparation"}){auto value=decode(kind,"military_naval");if(!value.ok)std::cerr<<value.error.original.code<<" "<<value.error.original.path<<"\n";check(value.ok,kind);check(value.value.facts.find("naval:1")->domain=="military_naval","domain lost");}
 for(const auto& kind:{"naval_navigation_unknown","naval_gunnery_unknown"}){auto value=decode(kind,"military_naval");check(!value.ok&&value.error.original.code=="InvalidFactDomain","unknown naval fact accepted");}
 auto wrong=decode("naval_navigation_step","interaction");check(!wrong.ok&&wrong.error.original.code=="InvalidFactDomain","wrong domain accepted");
 std::cout<<"NAVAL_NAVIGATION_COMMON "<<checks<<" checks 0 failures\n";return 0;
}catch(const std::exception& e){std::cerr<<"NAVAL_NAVIGATION_COMMON "<<checks<<" checks 1 failure: "<<e.what()<<'\n';return 1;}}
