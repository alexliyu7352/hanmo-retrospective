// 固定UI报价只读原required_capabilities；不凭对白内容猜社会能力，也不改变检查点模式。
#include "resident/domains/interaction/interaction_codec.hpp"
#include "resident/storage/common_codec.hpp"
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
using namespace hanmo::resident;
// 小夹具只改变冷目录的声明项，实际完整作者目录与对白图保持不变。
int main(int argc,char** argv){
    assert(argc==2);std::ifstream input(argv[1]);assert(input.good());std::string bytes((std::istreambuf_iterator<char>(input)),{});auto parsed=parse_boundary_json(bytes);assert(parsed.ok);auto data=*parsed.value.find("data");
    auto common=decode_common_catalog(data);assert(common.ok);auto shared=std::make_shared<const CommonCatalog>(std::move(common.value));auto original=decode_interaction_catalog(data,shared);assert(original.ok&&original.value.npc_society&&original.value.road_encounters);
    auto& required=*data.find("required_capabilities")->as<BoundaryValue::Array>();required.erase(std::remove_if(required.begin(),required.end(),[](const auto& x){return *x.template as<std::string>()=="npc_society";}),required.end());
    auto without=decode_interaction_catalog(data,shared);assert(without.ok&&!without.value.npc_society&&without.value.road_encounters);assert(without.value.dialogues.size()==original.value.dialogues.size());
    required.push_back(BoundaryValue{std::string{"unrelated_society_name"}});auto unrelated=decode_interaction_catalog(data,shared);assert(unrelated.ok&&!unrelated.value.npc_society);
    required.push_back(BoundaryValue{std::string{"npc_society"}});auto restored=decode_interaction_catalog(data,shared);assert(restored.ok&&restored.value.npc_society);
    required.back()=BoundaryValue{Integer{1}};auto malformed=decode_interaction_catalog(data,shared);assert(!malformed.ok&&malformed.error.original.code=="InvalidResidentString");
    assert(InteractionCatalog{}.npc_society==false);
    std::cout<<"background_ui_quote_capability: PASS (6 cold declared-capability cases; original dialogue unchanged)\n";
}
