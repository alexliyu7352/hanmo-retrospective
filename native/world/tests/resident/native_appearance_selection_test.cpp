// 原生新局成人选择的窄入口测试；完整192组合与旧政策隔离，不以夹具代替世界存读验收。
#include "resident/kernel/native_appearance_selection.hpp"
#include "resident/storage/common_codec.hpp"
#include <cassert>
#include <iostream>
using namespace hanmo::resident;
namespace {
using V=BoundaryValue;
// 完整元组由原九槽构造，测试保留字段语义而不是只传代表索引。
V tuple(const StableId& face){return boundary::obj({{"base_features",boundary::obj({{"face_shape",V{face}},{"eyes",V{"open"}},{"brows",V{"straight"}},{"nose",V{"round"}},{"mouth",V{"neutral"}},{"hair",V{"bun"}},{"beard",V{"none"}},{"skin_palette",V{"warm"}},{"build",V{"normal"}}})},{"attire_style",V{"plain"}}});}
// 只建作者冷输入；该夹具不分配人物资产或行进时间。
struct Input {
    V scenario,data;
    Input(){V::Array choices;for(char c='a';c<='l';++c){auto value=tuple(StableId{"sample."}+c);value.as<V::Dict>()->emplace_back("id",V{StableId{"sample."}+c});choices.push_back(std::move(value));}
        auto definition=tuple("sample.a");definition.as<V::Dict>()->emplace_back("kind",V{"fixed"});definition.as<V::Dict>()->emplace_back("generator_version",V{"appearance-v1"});
        data=boundary::obj({{"actors",boundary::obj({{"actor.hero",boundary::obj({{"origin",V{"original"}},{"initial_age_years",V{Integer{22}}},{"appearance_definition",std::move(definition)}})}})}});
        scenario=boundary::obj({{"player_actor",V{"actor.hero"}},{"dynamic_world",boundary::obj({{"founder_policy",V{"fixed_actor_appearance_v1"}}})},{"player_appearance_choices",V{std::move(choices)}}});
    }
};
// 新政策测试明确列出十二骨相，四个bit穷举四项二选一，避免只验证作者默认那一项。
constexpr const char* faces[]={"medium_clear_long","medium_square_jaw","medium_round_chin","medium_high_cheek","female_round_broad","female_straight_jaw","female_short_chin","female_high_cheek","long_narrow","long_straight_jaw","long_full_oval","long_high_cheek"};
// 请求始终保留九槽与衣着，不借助作者ID或资源路径定位结果。
V composition_tuple(const StableId& face,unsigned bits){
    auto value=tuple(face);auto& f=*value.find("base_features");
    *f.find("eyes")=V{bits&1u?"narrow":"open"};*f.find("brows")=V{bits&2u?"straight":"arched"};
    *f.find("nose")=V{bits&4u?"round":"straight"};*f.find("mouth")=V{bits&8u?"smile":"neutral"};return value;
}
// 从旧夹具只替换明确政策与作者表；旧政策夹具和原断言不被扩宽。
Input composition_input(){
    Input result;*result.scenario.find("dynamic_world")->find("founder_policy")=V{"fixed_actor_appearance_composition_v1"};
    V::Array choices;for(const auto* face:faces)for(unsigned bits=0;bits<16;++bits){auto value=composition_tuple(face,bits);value.as<V::Dict>()->emplace_back("id",V{StableId{face}+"."+std::to_string(bits)});choices.push_back(std::move(value));}
    *result.scenario.find("player_appearance_choices")=V{std::move(choices)};
    auto& definition=*result.data.find("actors")->find("actor.hero")->find("appearance_definition");
    *definition.find("base_features")=*composition_tuple(faces[0],0).find("base_features");return result;
}
// 删除字段用于验证缺项，不把缺省值补回测试输入。
void erase_field(V& value,const StableId& key){auto& rows=*value.as<V::Dict>();rows.erase(std::remove_if(rows.begin(),rows.end(),[&](const auto& row){return row.first==key;}),rows.end());}
// 所有拒绝须返回原失败身份且不返回半个档案，三个输入JSON逐字保持原样。
void rejected_without_effect(const Input& input,const V& request,const StableId& code){
    const auto before_data=render_boundary_json(input.data),before_scenario=render_boundary_json(input.scenario),before_request=render_boundary_json(request);
    assert(before_data.ok&&before_scenario.ok&&before_request.ok);const auto result=select_native_appearance(input.data,input.scenario,request);
    if(result.ok||result.value||result.error.original.code!=code){std::cerr<<"Expected "<<code<<", got "<<result.error.original.code<<"\n";assert(false);}
    assert(render_boundary_json(input.data).value==before_data.value&&render_boundary_json(input.scenario).value==before_scenario.value&&render_boundary_json(request).value==before_request.value);
}
// 合法值逐槽核回原AppearanceProfile，避免通过只比较face遗漏其他外貌变化。
void check_composition_selection(){
    auto input=composition_input();const auto before_data=render_boundary_json(input.data).value,before_scenario=render_boundary_json(input.scenario).value;
    assert(validate_native_appearance_choices(input.data,input.scenario).ok);
    for(const auto* face:faces)for(unsigned bits=0;bits<16;++bits){
        auto request=composition_tuple(face,bits);const auto selected=select_native_appearance(input.data,input.scenario,request);assert(selected.ok&&selected.value);
        const auto& p=*selected.value;const auto& f=p.base_features;assert(p.format_version==1&&p.generator_version=="appearance-v1"&&p.source=="custom"&&p.generation_seed=="not-sampled"&&p.parent_sources.empty());
        assert(p.attire_style=="plain"&&f.present&&f.face_shape==face&&f.eyes==(bits&1u?"narrow":"open")&&f.brows==(bits&2u?"straight":"arched")&&f.nose==(bits&4u?"round":"straight")&&f.mouth==(bits&8u?"smile":"neutral")&&f.hair=="bun"&&f.beard=="none"&&f.skin_palette=="warm"&&f.build=="normal");
    }
    assert(render_boundary_json(input.data).value==before_data&&render_boundary_json(input.scenario).value==before_scenario);
    for(Integer age:{18,59}){*input.data.find("actors")->find("actor.hero")->find("initial_age_years")=V{age};assert(select_native_appearance(input.data,input.scenario,composition_tuple(faces[11],15)).ok);}
    auto absent=select_native_appearance(input.data,input.scenario,std::nullopt);assert(absent.ok&&!absent.value);
    auto empty=select_native_appearance(input.data,input.scenario,V{V::Dict{}});assert(empty.ok&&!empty.value);
    auto request=composition_tuple(faces[11],15);std::reverse(request.as<V::Dict>()->begin(),request.as<V::Dict>()->end());std::reverse(request.find("base_features")->as<V::Dict>()->begin(),request.find("base_features")->as<V::Dict>()->end());assert(select_native_appearance(input.data,input.scenario,request).ok);
    for(const auto* field:{"face_shape","eyes","brows","nose","mouth","hair","beard","skin_palette","build"}){
        request=composition_tuple(faces[0],0);*request.find("base_features")->find(field)=V{"outside"};rejected_without_effect(input,request,"AppearanceChoiceNotAllowed");
        request=composition_tuple(faces[0],0);erase_field(*request.find("base_features"),field);rejected_without_effect(input,request,"MissingResidentField");
        request=composition_tuple(faces[0],0);*request.find("base_features")->find(field)=V{"res://outside.png"};rejected_without_effect(input,request,"InvalidAppearanceChoice");
    }
    request=composition_tuple(faces[0],0);*request.find("attire_style")=V{"silk"};rejected_without_effect(input,request,"AppearanceChoiceNotAllowed");
    request=composition_tuple(faces[0],0);erase_field(request,"attire_style");rejected_without_effect(input,request,"MissingResidentField");
    for(const auto* field:{"id","appearance_id","portrait_path","rig"}){request=composition_tuple(faces[0],0);request.as<V::Dict>()->emplace_back(field,V{"forbidden"});rejected_without_effect(input,request,"UnsupportedResidentField");}
    request=composition_tuple(faces[0],0);request.find("base_features")->as<V::Dict>()->emplace_back("color",V{"red"});rejected_without_effect(input,request,"UnsupportedResidentField");
    rejected_without_effect(input,V{Integer{1}},"InvalidResidentObject");
}
// 整表失败早于请求解析，旧政策的数量和拼接边界保持原失败身份。
void check_composition_policy_guards(){
    const auto request=composition_tuple(faces[0],0);
    auto input=composition_input();auto& values=*input.scenario.find("player_appearance_choices")->as<V::Array>();
    values.pop_back();rejected_without_effect(input,V{Integer{1}},"InvalidAppearanceChoiceCount");
    input=composition_input();auto& duplicated=*input.scenario.find("player_appearance_choices")->as<V::Array>();duplicated[191]=duplicated[0];rejected_without_effect(input,request,"DuplicateAppearanceChoice");
    *duplicated[191].find("id")=V{"different.id"};rejected_without_effect(input,request,"DuplicateAppearanceTuple");
    for(const auto* field:{"face_shape","eyes","brows","nose","mouth","hair","beard","skin_palette","build"}){
        input=composition_input();auto& choice=input.scenario.find("player_appearance_choices")->as<V::Array>()->back();*choice.find("base_features")->find(field)=V{"outside"};rejected_without_effect(input,request,"InvalidAppearanceChoice");
    }
    input=composition_input();*input.data.find("actors")->find("actor.hero")->find("appearance_definition")->find("attire_style")=V{"silk"};rejected_without_effect(input,request,"InvalidAppearanceChoiceDefault");
    input=composition_input();*input.data.find("actors")->find("actor.hero")->find("origin")=V{"historical"};rejected_without_effect(input,request,"HistoricalAppearanceOverride");
    for(Integer age:{-1,17,60}){input=composition_input();*input.data.find("actors")->find("actor.hero")->find("initial_age_years")=V{age};rejected_without_effect(input,request,"UnsupportedAppearanceAge");}
    input=composition_input();*input.scenario.find("dynamic_world")->find("founder_policy")=V{"fixed_actor_appearance_v1"};rejected_without_effect(input,request,"InvalidAppearanceChoiceCount");
    input=Input{};*input.scenario.find("dynamic_world")->find("founder_policy")=V{"fixed_actor_appearance_composition_v1"};rejected_without_effect(input,request,"InvalidAppearanceChoiceCount");
    input=Input{};rejected_without_effect(input,request,"AppearanceChoiceNotAllowed");
    input=composition_input();*input.scenario.find("dynamic_world")->find("founder_policy")=V{"fixed_actor"};rejected_without_effect(input,request,"UnexpectedAppearanceChoices");
    erase_field(input.scenario,"player_appearance_choices");rejected_without_effect(input,request,"FixedActorAppearanceRequired");
    input=composition_input();erase_field(input.scenario,"player_appearance_choices");rejected_without_effect(input,request,"MissingAppearanceChoices");
    input=composition_input();*input.scenario.find("dynamic_world")->find("founder_policy")=V{"unrecognized"};rejected_without_effect(input,request,"UnsupportedFounderPolicy");
}

}
int main(){
    Input in;auto before=render_boundary_json(in.data);assert(before.ok);
    assert(validate_native_appearance_choices(in.data,in.scenario).ok);
    for(char c='a';c<='l';++c){auto selected=select_native_appearance(in.data,in.scenario,tuple(StableId{"sample."}+c));assert(selected.ok&&selected.value&&selected.value->source=="custom"&&selected.value->generation_seed=="not-sampled"&&selected.value->base_features.face_shape==StableId{"sample."}+c);}
    assert(render_boundary_json(in.data).value==before.value);
    auto absent=select_native_appearance(in.data,in.scenario,std::nullopt);assert(absent.ok&&!absent.value);
    auto request=tuple("sample.a");*request.find("base_features")->find("eyes")=V{"narrow"};assert(!select_native_appearance(in.data,in.scenario,request).ok);
    request=tuple("sample.a");request.as<V::Dict>()->emplace_back("rig",V{Integer{0}});assert(!select_native_appearance(in.data,in.scenario,request).ok);
    request=tuple("sample.a");std::reverse(request.as<V::Dict>()->begin(),request.as<V::Dict>()->end());assert(select_native_appearance(in.data,in.scenario,request).ok);
    *in.data.find("actors")->find("actor.hero")->find("initial_age_years")=V{Integer{17}};assert(!validate_native_appearance_choices(in.data,in.scenario).ok);
    *in.data.find("actors")->find("actor.hero")->find("initial_age_years")=V{Integer{60}};assert(!validate_native_appearance_choices(in.data,in.scenario).ok);
    Input duplicate;auto* values=duplicate.scenario.find("player_appearance_choices")->as<V::Array>();(*values)[1]=(*values)[0];assert(!validate_native_appearance_choices(duplicate.data,duplicate.scenario).ok);
    Input old;*old.scenario.find("dynamic_world")->find("founder_policy")=V{"fixed_actor"};assert(!validate_native_appearance_choices(old.data,old.scenario).ok);
    auto& fields=*old.scenario.as<V::Dict>();fields.erase(std::remove_if(fields.begin(),fields.end(),[](const auto& row){return row.first=="player_appearance_choices";}),fields.end());assert(validate_native_appearance_choices(old.data,old.scenario).ok);assert(!select_native_appearance(old.data,old.scenario,tuple("sample.a")).ok);assert(select_native_appearance(old.data,old.scenario,std::nullopt).ok);
    check_composition_selection();check_composition_policy_guards();
    std::cout<<"native appearance selection: 192 complete combinations, original twelve tuples, isolated policies, error precedence and unchanged inputs passed\n";
}
