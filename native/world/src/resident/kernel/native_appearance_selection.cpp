#include "hanmo/resident/checkpoint_memory.hpp"
// 已选人物的有限成人外貌许可；只核作者元组，不读美术、不调用随机数或分配世界ID。
#include "native_appearance_selection.hpp"
#include "native_actor_selection.hpp"
#include "resident/storage/common_codec.hpp"
#include <set>
namespace hanmo::resident {
namespace {
using V=BoundaryValue;
constexpr const char* features[]={"face_shape","eyes","brows","nose","mouth","hair","beard","skin_palette","build"};
constexpr const char* composition_faces[]={"medium_clear_long","medium_square_jaw","medium_round_chin","medium_high_cheek","female_round_broad","female_straight_jaw","female_short_chin","female_high_cheek","long_narrow","long_straight_jaw","long_full_oval","long_high_cheek"};
// 语义token和既有appearance-v1保持ASCII边界；宽松JSON字符串不能混入控制字符或资源路径。
bool token(const StableId& value){
    if(value.empty()||value.size()>64||value.front()<'a'||value.front()>'z')return false;
    for(const auto c:value)if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='.'||c=='-'))return false;
    return true;
}
// 明确核九槽完整字段，保持字段顺序无关，不将未知键或拼接元组自动修复。
void check_tuple(const V& value,const StableId& path,bool authored){
    using namespace boundary;
    if(authored)fields(value,{"id","base_features","attire_style"},{},path);
    else fields(value,{"base_features","attire_style"},{},path);
    if(authored&&!token(get_string(value,"id",path)))fail("InvalidAppearanceChoice",path+"/id");
    if(!token(get_string(value,"attire_style",path)))fail("InvalidAppearanceChoice",path+"/attire_style");
    const auto& f=field(value,"base_features",path);fields(f,{"face_shape","eyes","brows","nose","mouth","hair","beard","skin_palette","build"},{},path+"/base_features");
    for(const auto* key:features)if(!token(get_string(f,key,path)))fail("InvalidAppearanceChoice",path+"/base_features/"+key);
}
// 新政策限定作者语义域；资源存在性归表现层，旧十二代表不套用这些新增限制。
void check_composition_tuple(const V& value,const StableId& path){
    using namespace boundary;const auto& f=field(value,"base_features",path);
    const auto face=get_string(f,"face_shape",path);
    if(std::none_of(std::begin(composition_faces),std::end(composition_faces),[&](const auto* allowed){return face==allowed;}))fail("InvalidAppearanceChoice",path+"/base_features/face_shape");
    for(const auto& entry:{std::pair<const char*,std::pair<const char*,const char*>>{"eyes",{"open","narrow"}},{"brows",{"arched","straight"}},{"nose",{"straight","round"}},{"mouth",{"neutral","smile"}}}){
        const auto selected=get_string(f,entry.first,path);
        if(selected!=entry.second.first&&selected!=entry.second.second)fail("InvalidAppearanceChoice",path+"/base_features/"+entry.first);
    }
    for(const auto& entry:{std::pair<const char*,const char*>{"hair","bun"},{"beard","none"},{"skin_palette","warm"},{"build","normal"}})
        if(get_string(f,entry.first,path)!=entry.second)fail("InvalidAppearanceChoice",path+"/base_features/"+entry.first);
    if(get_string(value,"attire_style",path)!="plain")fail("InvalidAppearanceChoice",path+"/attire_style");
}
// 所有槽位及衣着必须来自同一完整选择；候选各自合法不意味着交叉混搭合法。
bool same_tuple(const V& a,const V& b){
    using namespace boundary;
    if(get_string(a,"attire_style","")!=get_string(b,"attire_style",""))return false;
    const auto& af=field(a,"base_features","");const auto& bf=field(b,"base_features","");
    for(const auto* key:features)if(get_string(af,key,"")!=get_string(bf,key,""))return false;
    return true;
}
// 从选中情景快照读取年龄与默认，不把全目录未激活人物当创始人。
const V& founder(const V& data,const V& scenario){
    using namespace boundary;const auto id=get_string(scenario,"player_actor","scenario");
    const auto& definition=field(field(data,"actors","data"),id,"actors");
    if(get_string(definition,"origin",id)=="historical")fail("HistoricalAppearanceOverride",id);
    const auto age=get_int(definition,"initial_age_years",id);if(age<18||age>59)fail("UnsupportedAppearanceAge",id);
    const auto& appearance=field(definition,"appearance_definition",id);
    if(get_string(appearance,"kind",id)!="fixed"||get_string(appearance,"generator_version",id)!="appearance-v1")fail("InvalidAppearanceChoiceDefault",id);
    return appearance;
}
// 局部只读投影复用既有组合准入；原场景、人物表及其他出生字段都不写回。
V composition_scenario(const V& scenario,const StableId& actor){
    auto result=scenario;
    *result.find("player_actor")=V{actor};
    *result.find("dynamic_world")->find("founder_policy")=V{"fixed_actor_appearance_composition_v1"};
    // 内部复用旧组合门的副本不再携带新选人字段；外部原场景及旧政策拒绝边界均保持。
    auto& fields=*result.as<V::Dict>();
    fields.erase(std::remove_if(fields.begin(),fields.end(),[](const auto& row){return row.first=="playable_actor_appearance_policies"||row.first=="player_start_descriptions";}),fields.end());
    return result;
}
}
// 两个外貌政策各自核完整作者表；192个不重复合法元组恰好覆盖十二骨相与四项二选一。
Status validate_native_appearance_choices(const V& data,const V& scenario){try{
    using namespace boundary;const auto& dynamic=field(scenario,"dynamic_world","scenario");const auto policy=get_string(dynamic,"founder_policy","dynamic_world");
    if(native_existing_actor_policy(policy)){
        const auto actors_valid=validate_native_actor_choices(data,scenario);if(!actors_valid.ok)return actors_valid;
        bool composition=false;
        for(const auto& row:array(field(scenario,"playable_actor_appearance_policies","scenario"),"playable_actor_appearance_policies")){
            if(get_string(row,"policy","playable_actor_appearance_policies")=="fixed_actor_appearance_composition_v1"){
                composition=true;
                const auto local=composition_scenario(scenario,get_string(row,"actor_id","playable_actor_appearance_policies"));
                const auto valid=validate_native_appearance_choices(data,local);if(!valid.ok)return valid;
            }else if(policy=="selected_existing_actor_roster_v1"){
                // 新名单保留者必须已有固定档案；生成配置不能借空覆盖在开局重新抽样。
                const auto id=get_string(row,"actor_id","playable_actor_appearance_policies");
                const auto& actor=field(field(data,"actors","data"),id,"actors");
                const auto* definition=actor.find("appearance_definition");
                const auto* kind=definition?definition->find("kind"):nullptr;
                const auto* version=definition?definition->find("generator_version"):nullptr;
                if(!kind||!kind->as<StableId>()||*kind->as<StableId>()!="fixed"||!version||!version->as<StableId>()||*version->as<StableId>()!="appearance-v1")fail("InvalidPreservedActorAppearance","data/actors/"+id+"/appearance_definition");
            }
        }
        if(policy=="selected_existing_actor_roster_v1"&&!composition&&scenario.find("player_appearance_choices"))fail("UnexpectedAppearanceChoices","scenario/player_appearance_choices");
        return common_success();
    }
    // 两个新选人字段仅属于新政策；旧政策不能借Schema扩展静默接收新的准入含义。
    if(policy=="fixed_actor"||policy=="fixed_actor_appearance_v1"||policy=="fixed_actor_appearance_composition_v1")
        for(const auto* key:{"playable_actor_appearance_policies","player_start_descriptions"})if(scenario.find(key))fail("UnexpectedActorSelectionFields",StableId{"scenario/"}+key);
    const auto* choices=scenario.find("player_appearance_choices");
    if(policy=="fixed_actor"){if(choices)fail("UnexpectedAppearanceChoices","scenario/player_appearance_choices");return common_success();}
    const bool composition=policy=="fixed_actor_appearance_composition_v1";
    if(policy!="fixed_actor_appearance_v1"&&!composition)unsupported("UnsupportedFounderPolicy","dynamic_world/founder_policy");
    if(!choices)fail("MissingAppearanceChoices","scenario/player_appearance_choices");
    const auto& values=array(*choices,"player_appearance_choices");
    if(values.size()!=(composition?192u:12u))fail("InvalidAppearanceChoiceCount","player_appearance_choices");
    const auto& original=founder(data,scenario);std::set<StableId> ids;bool default_found=false;
    for(std::size_t n=0;n<values.size();++n){const auto& choice=values[n];const auto path="player_appearance_choices/"+std::to_string(n);check_tuple(choice,path,true);
        if(composition)check_composition_tuple(choice,path);
        if(!ids.insert(get_string(choice,"id",path)).second)fail("DuplicateAppearanceChoice",path);
        for(std::size_t p=0;p<n;++p)if(same_tuple(values[p],choice))fail("DuplicateAppearanceTuple",path);
        if(same_tuple(original,choice))default_found=true;
    }
    if(!default_found)fail("InvalidAppearanceChoiceDefault","scenario/player_actor");
    return common_success();
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
// 源码默认仍沿原fixed初始化；明确草案仅在完全匹配后生成原custom档案。
Outcome<std::shared_ptr<const AppearanceProfile>> select_native_appearance(const V& data,const V& scenario,const std::optional<V>& request){try{
    using namespace boundary;
    if(native_existing_actor_policy(get_string(field(scenario,"dynamic_world","scenario"),"founder_policy","dynamic_world")))return select_native_appearance(data,scenario,request,StableId{});
    auto valid=validate_native_appearance_choices(data,scenario);if(!valid.ok)return {false,{},valid.error};
    if(!request)return {true,{}, {}};
    object(*request,"appearance_request");if(request->as<V::Dict>()->empty())return {true,{}, {}};
    if(get_string(field(scenario,"dynamic_world","scenario"),"founder_policy","")=="fixed_actor")unsupported("FixedActorAppearanceRequired","appearance_request");
    check_tuple(*request,"appearance_request",false);bool found=false;
    for(const auto& choice:array(field(scenario,"player_appearance_choices","scenario"),"player_appearance_choices"))if(same_tuple(choice,*request)){found=true;break;}
    if(!found)fail("AppearanceChoiceNotAllowed","appearance_request");
    auto profile=checkpoint_make_shared<AppearanceProfile>();profile->generator_version="appearance-v1";profile->generation_seed="not-sampled";profile->source="custom";
    profile->attire_style=get_string(*request,"attire_style","appearance_request");profile->base_features.present=true;
    const auto& f=field(*request,"base_features","appearance_request");auto& out=profile->base_features;
    for(const auto& entry:{std::pair<const char*,StableId*>{"face_shape",&out.face_shape},{"eyes",&out.eyes},{"brows",&out.brows},{"nose",&out.nose},{"mouth",&out.mouth},{"hair",&out.hair},{"beard",&out.beard},{"skin_palette",&out.skin_palette},{"build",&out.build}})*entry.second=get_string(f,entry.first,"appearance_request/base_features");
    return {true,std::move(profile),{}};
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
// 原外貌许可只返回空覆盖；任何非空草案明确拒绝，绝不补九槽或替换既有人物的档案。
Outcome<std::shared_ptr<const AppearanceProfile>> select_native_appearance(const V& data,const V& scenario,const std::optional<V>& request,const StableId& selected_actor){try{
    using namespace boundary;
    if(!native_existing_actor_policy(get_string(field(scenario,"dynamic_world","scenario"),"founder_policy","dynamic_world"))){
        if(selected_actor!=get_string(scenario,"player_actor","scenario"))fail("InvalidPlayableActor","request/founder_selection");
        return select_native_appearance(data,scenario,request);
    }
    const auto selected=select_native_actor(data,scenario,selected_actor.empty()?std::nullopt:std::optional<StableId>{selected_actor});if(!selected.ok)return {false,{},selected.error};
    const auto valid=validate_native_appearance_choices(data,scenario);if(!valid.ok)return {false,{},valid.error};
    for(const auto& row:array(field(scenario,"playable_actor_appearance_policies","scenario"),"playable_actor_appearance_policies"))if(get_string(row,"actor_id","playable_actor_appearance_policies")==selected.value){
        if(get_string(row,"policy","playable_actor_appearance_policies")=="fixed_actor_appearance_composition_v1")return select_native_appearance(data,composition_scenario(scenario,selected.value),request);
        if(request&&(!request->as<V::Dict>()||!request->as<V::Dict>()->empty()))fail("ExistingActorAppearanceRequired","request/appearance_request");
        return {true,{},{}};
    }
    return {false,{},common_failure("InvalidPlayableActorAppearancePolicies","scenario/playable_actor_appearance_policies").error};
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
}
