// 只覆盖本次补齐的旧政策字段隔离与18至120岁边界，不重复已通过的338或192矩阵。
#include "resident/kernel/native_actor_selection.hpp"
#include "resident/kernel/native_appearance_selection.hpp"
#include "resident/storage/common_codec.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
using V=BoundaryValue;
Integer checks=0;
// 真实返回码和输入字节均参与断言，失败不能由Release关闭assert掩盖。
void check(bool condition,const StableId& message){++checks;if(!condition)throw std::runtime_error(message);}
// 旧政策只需字段存在即拒绝，不先解释本不属于它的字段类型或内容。
void old_policy_fields(){
    using namespace boundary;
    for(const auto* policy:{"fixed_actor","fixed_actor_appearance_v1","fixed_actor_appearance_composition_v1"})for(const auto* key:{"playable_actor_appearance_policies","player_start_descriptions"})for(const auto& value:{V{V::Array{}},V{V::Dict{}},V{std::monostate{}}}){
        const auto scenario=obj({{"dynamic_world",obj({{"founder_policy",V{policy}}})},{key,value}});const auto before=render_boundary_json(scenario);check(before.ok,"old-policy input serializes");const auto result=validate_native_appearance_choices(V{V::Dict{}},scenario);
        check(!result.ok&&result.error.original.code=="UnexpectedActorSelectionFields"&&result.error.original.path==StableId{"scenario/"}+key,"new actor field is rejected with exact policy field path");check(render_boundary_json(scenario).value==before.value,"policy rejection preserves original scenario");
    }
    const auto old=obj({{"dynamic_world",obj({{"founder_policy",V{"fixed_actor"}}})}});check(validate_native_appearance_choices(V{V::Dict{}},old).ok,"old fixed policy without new fields stays admitted");
    const auto unknown=obj({{"dynamic_world",obj({{"founder_policy",V{"unknown"}}})},{"player_start_descriptions",V{V::Array{}}}});const auto unsupported=validate_native_appearance_choices(V{V::Dict{}},unknown);check(!unsupported.ok&&unsupported.error.original.code=="UnsupportedFounderPolicy","unknown policy retains original failure priority");
}
// 只测原人物年龄准入，preserve许可不得强迫套入组合外貌的18至59岁范围。
void actor_age_limits(){
    using namespace boundary;
    for(Integer age:{17,18,120,121}){
        const auto data=obj({{"actors",obj({{"hero",obj({{"initial_age_years",V{age}}})},{"peer",obj({{"initial_age_years",V{Integer{43}}}})}})},{"active_static_actor_ids",string_array({"hero","peer"})}});
        const auto scenario=obj({{"player_actor",V{"hero"}},{"playable_actor_ids",string_array({"hero","peer"})},{"playable_actor_appearance_policies",V{V::Array{obj({{"actor_id",V{"hero"}},{"policy",V{"preserve_existing_v1"}}}),obj({{"actor_id",V{"peer"}},{"policy",V{"preserve_existing_v1"}}})}}},{"dynamic_world",obj({{"founder_policy",V{"selected_existing_actor_v1"}}})},{"enabled",obj({{"actors",string_array({"hero","peer"})}})},{"initial",obj({{"actors",V{V::Array{obj({{"actor_id",V{"hero"}},{"age_years",V{age}},{"alive",V{true}},{"available",V{true}}}),obj({{"actor_id",V{"peer"}},{"age_years",V{Integer{43}}},{"alive",V{true}},{"available",V{true}}})}}}})}});
        const auto before_data=render_boundary_json(data).value,before_scenario=render_boundary_json(scenario).value;const auto selected=select_native_actor(data,scenario,StableId{"hero"});
        check(age>=18&&age<=120?selected.ok&&selected.value=="hero":!selected.ok&&selected.value.empty()&&selected.error.original.code=="InvalidPlayableActors","author age bounds match18 through120");
        check(render_boundary_json(data).value==before_data&&render_boundary_json(scenario).value==before_scenario,"age check preserves full inputs");
    }
}
}
// 本窄门没有世界安装、计时或资产命令，整合运行由中央单独执行。
int main(){try{old_policy_fields();actor_age_limits();std::cout<<"NATIVE_ACTOR_POLICY_ISOLATION "<<checks<<" checks 0 failures\n";return 0;}catch(const std::exception& error){std::cerr<<"NATIVE_ACTOR_POLICY_ISOLATION FAIL "<<error.what()<<'\n';return 1;}}
