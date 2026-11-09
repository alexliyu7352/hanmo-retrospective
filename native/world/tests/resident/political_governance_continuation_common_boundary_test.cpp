// 共同冷入口只登记这一具名续辦事实；完整安民来源与玩家结果仍须World后验。
#include "resident/storage/common_codec.hpp"
#include "resident/storage/common_store.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {int checks=0;
void check(bool pass,const char* label){++checks;if(!pass)throw std::runtime_error(label);}
Outcome<CommonState> decode(const StableId& kind,const StableId& domain){CommonState state;state.metadata.ruleset_id=dynamic_ruleset_id;state.next_instance_id=3;state.facts.insert("political_governance_continuation:1",{"political_governance_continuation:1",kind,domain,{}});auto source=encode_common_state(state);std::get<BoundaryValue::Dict>(source.data).emplace_back("results",BoundaryValue{BoundaryValue::Dict{}});auto* fact=source.find("facts")->find("political_governance_continuation:1");std::get<BoundaryValue::Dict>(fact->data).emplace_back("domain",BoundaryValue{domain});return decode_common_seed(source,CommonCatalog{},nullptr,CommonSeedValidation::defer_until_bundle);}
}
int main(){try{
 auto accepted=decode("political_governance_continued","political_governance_continuations");check(accepted.ok,"registered continuation rejected");check(accepted.value.facts.find("political_governance_continuation:1")->domain=="political_governance_continuations","continuation domain lost");
 for(const auto& kind:{"political_governance_unknown","military_contact","political_shop_registry_accepted"}){auto wrong=decode(kind,"political_governance_continuations");check(!wrong.ok&&wrong.error.original.code=="InvalidFactDomain","unrelated kind accepted as continuation");}
 for(const auto& domain:{"interaction","politics","military","political_governance_unknown"}){auto wrong=decode("political_governance_continued",domain);check(!wrong.ok&&wrong.error.original.code=="InvalidFactDomain","continuation accepted in wrong domain");}
 std::cout<<"GOVERNANCE_CONTINUATION_COMMON "<<checks<<" checks 0 failures\n";return 0;
}catch(const std::exception& e){std::cerr<<"GOVERNANCE_CONTINUATION_COMMON "<<checks<<" checks 1 failure: "<<e.what()<<'\n';return 1;}}
