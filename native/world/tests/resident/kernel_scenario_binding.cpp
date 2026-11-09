// 真实种子与同checksum目录下的另一场景不能共享已准备地图/模型开关；冷拒绝保旧世界。
#include "hanmo/resident_world_api.hpp"
#include "resident/kernel/world_codec.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
using namespace hanmo::resident;
BoundaryValue read(const std::string& path){std::ifstream input(path);std::ostringstream text;text<<input.rdbuf();auto value=parse_boundary_json(text.str());if(!value.ok)throw std::runtime_error(value.error.original.code);return std::move(value.value);}
int main(int argc,char** argv){
    if(argc!=3)return 2;const std::string root=argv[1];const bool expect_reject=std::string(argv[2])=="--expect-reject";
    auto index=read(root+"/oracle-index.json");auto state=read(root+"/"+*index.find("initial_state")->find("file")->as<StableId>());auto catalog=read(root+"/expanded-catalog.json");
    auto prepared=prepare_catalog(catalog,state);if(!prepared.ok)return 3;
    auto initial=decode_world_seed(state,*prepared.value);if(!initial.ok)return 4;
    ResidentWorld live;if(!live.install(prepared.value,std::move(initial.value)).ok)return 5;
    auto before=live.checkpoint();auto identity=live.identity().value;if(!before.ok)return 6;
    auto altered=decode_world_checkpoint(before.value,*prepared.value);if(!altered.ok)return 7;
    altered.value.common.metadata.scenario_id="scenario.lived_city";
    WorldBundle other(prepared.value,std::move(altered.value));auto bad=encode_world_checkpoint(other);if(!bad.ok)return 8;
    auto restored=live.restore(bad.value,prepared.value);
    std::cout<<"restore_ok="<<restored.ok<<" code="<<restored.error.original.code<<'\n';
    if(!expect_reject)return restored.ok?0:9;
    if(restored.ok||restored.error.original.code!="ResidentScenarioMismatch")return 10;
    auto after=live.checkpoint();auto current=live.identity().value;
    if(!after.ok||after.value!=before.value||current.world_id!=identity.world_id||current.execution_epoch!=identity.execution_epoch||current.revision!=identity.revision||current.tick!=identity.tick)return 11;
    std::cout<<"SCENARIO_BINDING checks=3 failures=0\n";
}
