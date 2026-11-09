// 真实1.8目录的同一选择边界正反例；内部组合投影不得被外部旧政策隔离门误拒。
#include "resident/kernel/native_appearance_selection.hpp"
#include "resident/storage/common_codec.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
using V=BoundaryValue;
Integer checks=0;
// Release仍执行所有真实断言，保留原错误而不改期待值或生产目录。
void check(bool condition,const StableId& label){++checks;if(!condition)throw std::runtime_error(label);}
// 输出只作原输入字节比较，不生成另一份世界或存档。
StableId json(const V& value){const auto encoded=render_boundary_json(value);check(encoded.ok,"serialize exact boundary input");return encoded.value;}
// 只接已由实际Catalog.read_prepared导出的文件，失败不能用作者原包假充。
V read(const char* path){std::ifstream file(path,std::ios::binary);check(bool(file),"read real prepared catalog");std::ostringstream bytes;bytes<<file.rdbuf();auto parsed=parse_boundary_json(bytes.str());check(parsed.ok,"parse real prepared catalog");check(boundary::get_bool(parsed.value,"ok","catalog"),"prepared catalog is admitted");return std::move(parsed.value);}
// 纯函数要求有效快照；仅在测试局部副本提供它读取的年龄/外貌及活动集合，原目录不变，完整生产投影由联合world门验证。
V appearance_view(const V& catalog,const V& scenario){
    using namespace boundary;auto data=field(catalog,"data","catalog");V::Array active;
    for(const auto& row:array(field(field(scenario,"initial","scenario"),"actors","initial"),"initial/actors")){
        const auto actor=get_string(row,"actor_id","actor");active.push_back(V{actor});auto* definition=data.find("actors")->find(actor);check(definition,"actual snapshot actor exists");
        *definition->find("initial_age_years")=field(row,"age_years","actor");*definition->find("appearance_definition")=field(row,"appearance_definition","actor");
    }
    data.as<V::Dict>()->emplace_back("active_static_actor_ids",V{std::move(active)});return data;
}
// 本地删除只用于外部旧policy负例；被测生产代码不能更改调用者字典。
void erase(V& value,const StableId& key){auto& rows=*value.as<V::Dict>();rows.erase(std::remove_if(rows.begin(),rows.end(),[&](const auto& row){return row.first==key;}),rows.end());}
// 每次默认、非默认及保原外貌共用同一完整作者目录，全部输入序和值必须保持。
void selected(const V& data,const V& scenario,const StableId& actor,const std::optional<V>& request,bool override_expected,const StableId& code={}){
    const auto original_data=json(data),original_scenario=json(scenario),raw=request?json(*request):StableId{};const auto result=select_native_appearance(data,scenario,request,actor);
    check(code.empty()?(result.ok&&bool(result.value)==override_expected):(!result.ok&&!result.value&&result.error.original.code==code),"selected appearance: expected "+code+", actual "+result.error.original.code+" "+result.error.original.path);
    if(override_expected){check(bool(result.value)&&bool(request),"explicit complete override exists");const auto& profile=*result.value;const auto& expected=boundary::field(*request,"base_features","request");const auto& actual=profile.base_features;
        for(const auto& entry:{std::pair<const char*,const StableId*>{"face_shape",&actual.face_shape},{"eyes",&actual.eyes},{"brows",&actual.brows},{"nose",&actual.nose},{"mouth",&actual.mouth},{"hair",&actual.hair},{"beard",&actual.beard},{"skin_palette",&actual.skin_palette},{"build",&actual.build}})check(*entry.second==boundary::get_string(expected,entry.first,"request"),StableId{"exact selected slot "}+entry.first);
        check(profile.attire_style==boundary::get_string(*request,"attire_style","request")&&profile.source=="custom"&&profile.generation_seed=="not-sampled","complete original non-sampled custom profile");
    }
    check(json(data)==original_data&&json(scenario)==original_scenario&&(!request||json(*request)==raw),"caller catalog scenario and draft remain byte exact");
}
}
// 六个外部旧policy字段负例与真实新policy正例必须同门通过，避免只补单侧而再次相互冲突。
int main(int argc,char** argv){try{
    check(argc==3,"Usage: test_native_actor_appearance_projection PREPARED_CATALOG SCENARIO_ID");const auto catalog=read(argv[1]);const auto original=json(catalog);using namespace boundary;const auto& scenario=field(field(field(catalog,"data","catalog"),"scenarios","data"),argv[2],"scenarios");
    check(get_string(field(scenario,"dynamic_world","scenario"),"founder_policy","dynamic_world")=="selected_existing_actor_v1","actual new policy required");check(scenario.find("playable_actor_appearance_policies")&&scenario.find("player_start_descriptions"),"actual scenario contains both new fields");const auto data=appearance_view(catalog,scenario);const auto first=get_string(scenario,"player_actor","scenario");StableId other;
    for(const auto& row:array(field(scenario,"playable_actor_appearance_policies","scenario"),"policies"))if(get_string(row,"policy","policy")=="preserve_existing_v1")other=get_string(row,"actor_id","policy");
    check(first=="society::actor.apprentice"&&other=="society::actor.study_swordswoman","actual Cheng He and Shen Heng author IDs");
    selected(data,scenario,first,std::nullopt,false);selected(data,scenario,first,V{V::Dict{}},false);
    const auto& choice=array(field(scenario,"player_appearance_choices","scenario"),"choices").back();const auto request=obj({{"base_features",field(choice,"base_features","choice")},{"attire_style",field(choice,"attire_style","choice")}});
    check(!compare_boundary(field(request,"base_features","request"),field(field(field(field(data,"actors","data"),first,"actor"),"appearance_definition","actor"),"base_features","appearance")).values_equal,"explicit tested composition is not authored default");selected(data,scenario,first,request,true);
    selected(data,scenario,other,std::nullopt,false);selected(data,scenario,other,V{V::Dict{}},false);selected(data,scenario,other,request,false,"ExistingActorAppearanceRequired");
    for(const auto* policy:{"fixed_actor","fixed_actor_appearance_v1","fixed_actor_appearance_composition_v1"})for(const auto* retained:{"playable_actor_appearance_policies","player_start_descriptions"}){
        auto old=scenario;*old.find("dynamic_world")->find("founder_policy")=V{policy};erase(old,StableId{retained}=="playable_actor_appearance_policies"?"player_start_descriptions":"playable_actor_appearance_policies");const auto before=json(old);const auto result=validate_native_appearance_choices(data,old);check(!result.ok&&result.error.original.code=="UnexpectedActorSelectionFields"&&result.error.original.path==StableId{"scenario/"}+retained,"external old-policy field still rejected");check(json(old)==before,"external rejection leaves scenario unchanged");
    }
    check(json(catalog)==original,"actual original prepared catalog remains unchanged");std::cout<<"NATIVE_ACTOR_APPEARANCE_PROJECTION "<<checks<<" checks 0 failures\n";return 0;
}catch(const boundary::DecodeFailure& error){std::cerr<<"NATIVE_ACTOR_APPEARANCE_PROJECTION FAIL "<<error.error.original.code<<" "<<error.error.original.path<<'\n';return 1;}catch(const std::exception& error){std::cerr<<"NATIVE_ACTOR_APPEARANCE_PROJECTION FAIL "<<error.what()<<'\n';return 1;}}
