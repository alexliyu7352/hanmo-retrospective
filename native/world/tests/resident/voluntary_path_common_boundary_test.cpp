// Common显式domain准入只识别新撤回头；完整来源/版号仍由Family独立冷门核验。
#include "resident/storage/common_codec.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
// 用原Common编码结构后附上真实领域payload中的domain，避免只测默认推断分支。
Outcome<CommonState> decoded(const StableId& id,const StableId& kind,const StableId& domain){CommonState state;state.next_instance_id=3;state.metadata.ruleset_id=dynamic_ruleset_id;state.facts.insert(id,{id,kind,domain,{}});auto value=encode_common_state(state);std::get<BoundaryValue::Dict>(value.data).emplace_back("results",BoundaryValue{BoundaryValue::Dict{}});auto* fact=value.find("facts")->find(id);std::get<BoundaryValue::Dict>(fact->data).emplace_back("domain",BoundaryValue{domain});return decode_common_seed(value,CommonCatalog{},nullptr,CommonSeedValidation::defer_until_bundle);}
}
int main(){int n=0;try{auto need=[&](bool ok,const char* why){++n;if(!ok)throw std::runtime_error(why);};auto original=decoded("voluntary_path:1","family_voluntary_path_cancelled","family_voluntary_paths");if(!original.ok)std::cerr<<"actual original boundary rejection="<<original.error.original.code<<'\n';need(original.ok,"valid explicit cancellation domain rejected");need(original.value.facts.find("voluntary_path:1")->domain=="family_voluntary_paths","domain preserved");
 for(const auto* kind:{"family_voluntary_path_unknown","family_voluntary_succession","unknown"}){auto bad=decoded("voluntary_path:1",kind,"family_voluntary_paths");need(!bad.ok&&bad.error.original.code=="InvalidFactDomain","unrecognized kind accepted");}
 for(const auto* domain:{"family","interaction","task_message_returns"}){auto bad=decoded("voluntary_path:1","family_voluntary_path_cancelled",domain);need(!bad.ok&&bad.error.original.code=="InvalidFactDomain","wrong explicit domain accepted");}
 auto wrong=decoded("family:1","family_voluntary_path_cancelled","family_voluntary_paths");need(!wrong.ok,"wrong cancellation prefix accepted");for(const auto* id:{"voluntary_path:3","voluntary_path:4"}){auto collision=decoded(id,"family_voluntary_path_cancelled","family_voluntary_paths");need(!collision.ok&&collision.error.original.code=="InstanceCounterCollision","next instance collision accepted");}std::cout<<"VOLUNTARY_COMMON_BOUNDARY "<<n<<"/0\n";return 0;
}catch(const std::exception& e){std::cerr<<"VOLUNTARY_COMMON_BOUNDARY "<<n<<"/1 "<<e.what()<<'\n';return 1;}}
