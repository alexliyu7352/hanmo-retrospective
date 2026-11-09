// 双现存成人选择的纯边界用例；完整输入逐字不变，不冒充真实世界/银行验收。
#include "resident/kernel/native_appearance_selection.hpp"
#include "resident/kernel/native_actor_selection.hpp"
#include "resident/storage/common_codec.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
using V=BoundaryValue;
Integer checks=0;
// Release仍执行每项实际断言，失败保留边界错误而非靠assert编译开关。
void check(bool condition,const StableId& label){++checks;if(!condition)throw std::runtime_error(label);}
// 夹具元组只包含持久九槽及衣着，不依赖图像、名字或人物ID。
V tuple(const StableId& face,unsigned bits){return boundary::obj({{"base_features",boundary::obj({{"face_shape",V{face}},{"eyes",V{bits&1u?"narrow":"open"}},{"brows",V{bits&2u?"straight":"arched"}},{"nose",V{bits&4u?"round":"straight"}},{"mouth",V{bits&8u?"smile":"neutral"}},{"hair",V{"bun"}},{"beard",V{"none"}},{"skin_palette",V{"warm"}},{"build",V{"normal"}}})},{"attire_style",V{"plain"}}});}
// 两位现存人物有独立属性/资产哨兵；选择不得交换、补齐或清空其中任何值。
struct Input {
    V data,scenario;
    Input(){
        using namespace boundary;
        constexpr const char* faces[]={"medium_clear_long","medium_square_jaw","medium_round_chin","medium_high_cheek","female_round_broad","female_straight_jaw","female_short_chin","female_high_cheek","long_narrow","long_straight_jaw","long_full_oval","long_high_cheek"};
        V::Array choices;for(const auto* face:faces)for(unsigned bits=0;bits<16;++bits){auto value=tuple(face,bits);value.as<V::Dict>()->emplace_back("id",V{StableId{face}+"."+std::to_string(bits)});choices.push_back(std::move(value));}
        auto definition=tuple(faces[0],0);definition.as<V::Dict>()->emplace_back("kind",V{"fixed"});definition.as<V::Dict>()->emplace_back("generator_version",V{"appearance-v1"});
        const auto original=obj({{"kind",V{"fixed"}},{"generator_version",V{"appearance-v1"}},{"attire_style",V{"plain"}}});
        data=obj({{"actors",obj({{"actor.hero",obj({{"origin",V{"original"}},{"initial_age_years",V{Integer{22}}},{"appearance_definition",definition},{"skills",obj({{"study",V{Integer{40}}}})}})},{"actor.peer",obj({{"origin",V{"original"}},{"initial_age_years",V{Integer{43}}},{"appearance_definition",original},{"skills",obj({{"sword",V{Integer{71}}}})}})}})},{"active_static_actor_ids",string_array({"actor.hero","actor.peer"})},{"wallets",obj({{"hero",V{Integer{91}}},{"peer",V{Integer{212}}}})},{"inventories",string_array({"original.hero.goods","original.peer.goods"})},{"relations",string_array({"original.relation"})},{"obligations",string_array({"original.contract"})}});
        scenario=obj({{"player_actor",V{"actor.hero"}},{"playable_actor_ids",string_array({"actor.hero","actor.peer"})},{"playable_actor_appearance_policies",V{V::Array{obj({{"actor_id",V{"actor.hero"}},{"policy",V{"fixed_actor_appearance_composition_v1"}}}),obj({{"actor_id",V{"actor.peer"}},{"policy",V{"preserve_existing_v1"}}})}}},{"dynamic_world",obj({{"founder_policy",V{"selected_existing_actor_v1"}}})},{"player_appearance_choices",V{std::move(choices)}},{"enabled",obj({{"actors",string_array({"actor.hero","actor.peer"})}})},{"initial",obj({{"actors",V{V::Array{obj({{"actor_id",V{"actor.hero"}},{"alive",V{true}},{"available",V{true}},{"age_years",V{Integer{22}}}}),obj({{"actor_id",V{"actor.peer"}},{"alive",V{true}},{"available",V{true}},{"age_years",V{Integer{43}}}})}}}})}});
    }
};
// 删除字段只服务坏输入用例；被测函数不得替调用者补回字段。
void erase(V& value,const StableId& key){auto& rows=*value.as<V::Dict>();rows.erase(std::remove_if(rows.begin(),rows.end(),[&](const auto& row){return row.first==key;}),rows.end());}
// 原序JSON同时覆盖人物属性、资产及额外表，不只比较被选择的ID。
StableId json(const V& value){auto result=render_boundary_json(value);check(result.ok,"input has exact JSON representation");return result.value;}
// 成功或拒绝都必须保持所有输入原字节；拒绝不能泄漏半个选择结果。
void actor_result(const Input& input,const std::optional<StableId>& request,const StableId& expected,const StableId& code={}){
    const auto data=json(input.data),scenario=json(input.scenario);const auto result=select_native_actor(input.data,input.scenario,request);
    check(code.empty()?(result.ok&&result.value==expected):(!result.ok&&result.value.empty()&&result.error.original.code==code),"actor result expected "+(code.empty()?expected:code)+", got "+result.error.original.code);
    check(json(input.data)==data&&json(input.scenario)==scenario,"actor selection preserves all original inputs");
}
// 外貌比较覆盖原九槽及档案来源；空指针必须表示保原档案，不是另一份新默认。
void appearance_result(const Input& input,const std::optional<V>& request,const StableId& actor,bool override_expected,const StableId& code={}){
    const auto data=json(input.data),scenario=json(input.scenario),raw=request?json(*request):StableId{};const auto result=select_native_appearance(input.data,input.scenario,request,actor);
    check(code.empty()?(result.ok&&bool(result.value)==override_expected):(!result.ok&&!result.value&&result.error.original.code==code),"appearance result expected "+code+", got "+result.error.original.code);
    if(result.ok&&result.value){
        check(bool(request),"override has explicit complete request");const auto& p=*result.value;const auto& f=boundary::field(*request,"base_features","request");
        check(p.format_version==1&&p.generator_version=="appearance-v1"&&p.generation_seed=="not-sampled"&&p.source=="custom"&&p.parent_sources.empty(),"original custom profile protocol");
        check(p.attire_style==boundary::get_string(*request,"attire_style","request")&&p.base_features.present,"attire and complete features retained");
        const auto& actual=p.base_features;
        for(const auto& field:{std::pair<const char*,const StableId*>{"face_shape",&actual.face_shape},{"eyes",&actual.eyes},{"brows",&actual.brows},{"nose",&actual.nose},{"mouth",&actual.mouth},{"hair",&actual.hair},{"beard",&actual.beard},{"skin_palette",&actual.skin_palette},{"build",&actual.build}})check(*field.second==boundary::get_string(f,field.first,"request"),StableId{"exact selected slot "}+field.first);
    }
    check(json(input.data)==data&&json(input.scenario)==scenario&&(!request||json(*request)==raw),"appearance selection preserves all original inputs");
}
// 取作者初态记录构造单一损坏点，避免靠修改运行人物绕过冷准入。
V& initial_actor(Input& input,std::size_t index){return input.scenario.find("initial")->find("actors")->as<V::Array>()->at(index);}
// 策略逐项严格覆盖原候选；数组顺序不产生额外默认选择语义。
V& policy_row(Input& input,std::size_t index){return input.scenario.find("playable_actor_appearance_policies")->as<V::Array>()->at(index);}
// 显式选择两位原人物，不复制钱包、库存、技能、关系或义务；缺失和越界请求拒绝。
void check_actor_requests(){
    Input input;actor_result(input,StableId{"actor.hero"},"actor.hero");actor_result(input,StableId{"actor.peer"},"actor.peer");
    actor_result(input,std::nullopt,{},"PlayableActorSelectionRequired");
    for(const auto* id:{"","actor.outside","ACTOR.PEER"," actor.peer"})actor_result(input,StableId{id},{},"InvalidPlayableActor");
    std::reverse(input.scenario.find("playable_actor_ids")->as<V::Array>()->begin(),input.scenario.find("playable_actor_ids")->as<V::Array>()->end());
    std::reverse(input.scenario.find("playable_actor_appearance_policies")->as<V::Array>()->begin(),input.scenario.find("playable_actor_appearance_policies")->as<V::Array>()->end());
    actor_result(input,StableId{"actor.peer"},"actor.peer");
}
// 两位必须是活动初态成人且存活可用；纯选择不能用一位合法人物掩盖另一位坏作者记录。
void check_actor_author_guards(){
    using namespace boundary;Input input;
    for(auto ids:std::vector<std::vector<StableId>>{{"actor.hero"},{"actor.hero","actor.peer","actor.third"},{"actor.hero","actor.hero"},{"actor.hero",""}}){input=Input{};*input.scenario.find("playable_actor_ids")=string_array(ids);actor_result(input,StableId{"actor.hero"},{},"InvalidPlayableActors");}
    input=Input{};*input.scenario.find("player_actor")=V{"actor.outside"};actor_result(input,StableId{"actor.peer"},{},"InvalidPlayableActors");
    input=Input{};erase(*input.data.find("actors"),"actor.peer");actor_result(input,StableId{"actor.hero"},{},"InvalidPlayableActors");
    input=Input{};*input.scenario.find("enabled")->find("actors")=string_array({"actor.hero"});actor_result(input,StableId{"actor.hero"},{},"InvalidPlayableActors");
    input=Input{};*input.data.find("active_static_actor_ids")=string_array({"actor.hero","actor.peer","actor.peer"});actor_result(input,StableId{"actor.hero"},{},"InvalidPlayableActors");
    input=Input{};input.scenario.find("initial")->find("actors")->as<V::Array>()->pop_back();actor_result(input,StableId{"actor.hero"},{},"InvalidPlayableActors");
    input=Input{};auto duplicate=initial_actor(input,1);input.scenario.find("initial")->find("actors")->as<V::Array>()->push_back(duplicate);actor_result(input,StableId{"actor.hero"},{},"InvalidPlayableActors");
    for(const auto* key:{"alive","available"}){input=Input{};*initial_actor(input,1).find(key)=V{false};actor_result(input,StableId{"actor.hero"},{},"InvalidPlayableActors");}
    input=Input{};*initial_actor(input,1).find("age_years")=V{Integer{17}};*input.data.find("actors")->find("actor.peer")->find("initial_age_years")=V{Integer{17}};actor_result(input,StableId{"actor.hero"},{},"InvalidPlayableActors");
    input=Input{};*initial_actor(input,1).find("age_years")=V{Integer{44}};actor_result(input,StableId{"actor.peer"},{},"InvalidPlayableActors");
    input=Input{};*initial_actor(input,1).find("age_years")=V{Integer{60}};*input.data.find("actors")->find("actor.peer")->find("initial_age_years")=V{Integer{60}};actor_result(input,StableId{"actor.peer"},"actor.peer");appearance_result(input,std::nullopt,"actor.peer",false);
    input=Input{};*input.scenario.find("dynamic_world")->find("founder_policy")=V{"fixed_actor"};actor_result(input,StableId{"actor.hero"},{},"UnsupportedFounderPolicy");
}
// 外貌许可数组不得缺、多、重复、跨人物或夹带未知策略/字段。
void check_policy_guards(){
    Input input;auto policies=[](Input& value)->V::Array&{return *value.scenario.find("playable_actor_appearance_policies")->as<V::Array>();};
    policies(input).pop_back();actor_result(input,StableId{"actor.hero"},{},"InvalidPlayableActorAppearancePolicies");
    input=Input{};auto extra=policy_row(input,1);policies(input).push_back(extra);actor_result(input,StableId{"actor.hero"},{},"InvalidPlayableActorAppearancePolicies");
    input=Input{};*policy_row(input,1).find("actor_id")=V{"actor.hero"};actor_result(input,StableId{"actor.hero"},{},"InvalidPlayableActorAppearancePolicies");
    input=Input{};*policy_row(input,1).find("actor_id")=V{"actor.outside"};actor_result(input,StableId{"actor.hero"},{},"InvalidPlayableActorAppearancePolicies");
    for(const auto* policy:{"fixed_actor","fixed_actor_appearance_v1","unknown"}){input=Input{};*policy_row(input,1).find("policy")=V{policy};actor_result(input,StableId{"actor.hero"},{},"InvalidPlayableActorAppearancePolicies");}
    input=Input{};policy_row(input,1).as<V::Dict>()->emplace_back("appearance_id",V{"forbidden"});actor_result(input,StableId{"actor.hero"},{},"UnsupportedResidentField");
    input=Input{};erase(input.scenario,"playable_actor_appearance_policies");actor_result(input,StableId{"actor.hero"},{},"MissingResidentField");
    input=Input{};*input.scenario.find("playable_actor_appearance_policies")=V{V::Dict{}};actor_result(input,StableId{"actor.hero"},{},"InvalidResidentArray");
}
// 组合仅依逐人明确许可；另一位保留原定义，任何非空草案都不得静默应用或补默认。
void check_appearance_policies(){
    Input input;auto valid=validate_native_appearance_choices(input.data,input.scenario);check(valid.ok,"complete new author policy admitted");
    appearance_result(input,tuple("long_high_cheek",15),"actor.hero",true);
    appearance_result(input,std::nullopt,"actor.hero",false);appearance_result(input,V{V::Dict{}},"actor.hero",false);
    appearance_result(input,std::nullopt,"actor.peer",false);appearance_result(input,V{V::Dict{}},"actor.peer",false);
    for(const auto& request:{tuple("medium_clear_long",0),V{Integer{0}},V{V::Array{}},V{std::monostate{}},V{""}})appearance_result(input,request,"actor.peer",false,"ExistingActorAppearanceRequired");
    appearance_result(input,std::nullopt,"actor.outside",false,"InvalidPlayableActor");
    appearance_result(input,std::nullopt,"",false,"PlayableActorSelectionRequired");
    const auto legacy_entry=select_native_appearance(input.data,input.scenario,std::nullopt);check(!legacy_entry.ok&&!legacy_entry.value&&legacy_entry.error.original.code=="PlayableActorSelectionRequired","new policy cannot use implicit old three-argument selection");
    auto bad=tuple("long_high_cheek",15);*bad.find("base_features")->find("eyes")=V{"outside"};appearance_result(input,bad,"actor.hero",false,"AppearanceChoiceNotAllowed");
    input.scenario.find("player_appearance_choices")->as<V::Array>()->pop_back();appearance_result(input,std::nullopt,"actor.peer",false,"InvalidAppearanceChoiceCount");
    input=Input{};*policy_row(input,0).find("policy")=V{"preserve_existing_v1"};erase(input.scenario,"player_appearance_choices");appearance_result(input,std::nullopt,"actor.hero",false);appearance_result(input,std::nullopt,"actor.peer",false);
}
// 新版本名单允许一位或多位合资格原人物；旧版本继续拒绝相同扩表，不静默升级。
void check_configurable_roster(){
    using namespace boundary;
    Input input;*input.scenario.find("dynamic_world")->find("founder_policy")=V{"selected_existing_actor_roster_v1"};
    actor_result(input,StableId{"actor.peer"},"actor.peer");
    *input.scenario.find("playable_actor_ids")=string_array({"actor.hero"});
    input.scenario.find("playable_actor_appearance_policies")->as<V::Array>()->pop_back();
    actor_result(input,StableId{"actor.hero"},"actor.hero");
    appearance_result(input,tuple("long_high_cheek",15),"actor.hero",true);
    input=Input{};*input.scenario.find("dynamic_world")->find("founder_policy")=V{"selected_existing_actor_roster_v1"};
    auto third=*input.data.find("actors")->find("actor.peer");
    input.data.find("actors")->as<V::Dict>()->emplace_back("actor.third",third);
    input.data.find("active_static_actor_ids")->as<V::Array>()->push_back(V{"actor.third"});
    input.scenario.find("enabled")->find("actors")->as<V::Array>()->push_back(V{"actor.third"});
    input.scenario.find("playable_actor_ids")->as<V::Array>()->push_back(V{"actor.third"});
    auto initial=initial_actor(input,1);*initial.find("actor_id")=V{"actor.third"};input.scenario.find("initial")->find("actors")->as<V::Array>()->push_back(initial);
    auto policy=policy_row(input,1);*policy.find("actor_id")=V{"actor.third"};input.scenario.find("playable_actor_appearance_policies")->as<V::Array>()->push_back(policy);
    actor_result(input,StableId{"actor.third"},"actor.third");
    appearance_result(input,std::nullopt,"actor.third",false);
    appearance_result(input,tuple("long_high_cheek",15),"actor.third",false,"ExistingActorAppearanceRequired");
    auto old=input;*old.scenario.find("dynamic_world")->find("founder_policy")=V{"selected_existing_actor_v1"};actor_result(old,StableId{"actor.third"},{},"InvalidPlayableActors");
    auto bad=input;*initial_actor(bad,2).find("alive")=V{false};actor_result(bad,StableId{"actor.hero"},{},"InvalidPlayableActors");
    bad=input;*initial_actor(bad,2).find("age_years")=V{Integer{17}};*bad.data.find("actors")->find("actor.third")->find("initial_age_years")=V{Integer{17}};actor_result(bad,StableId{"actor.hero"},{},"InvalidPlayableActors");
    bad=input;*bad.scenario.find("playable_actor_ids")=string_array({});bad.scenario.find("playable_actor_appearance_policies")->as<V::Array>()->clear();actor_result(bad,StableId{"actor.hero"},{},"InvalidPlayableActors");
}
// 保留原人物不能在新局重新抽样；任何未选末项坏定义同样阻止新名单外貌准入。
void check_roster_preserved_appearance(){
    Input input;*input.scenario.find("dynamic_world")->find("founder_policy")=V{"selected_existing_actor_roster_v1"};
    *policy_row(input,0).find("policy")=V{"preserve_existing_v1"};erase(input.scenario,"player_appearance_choices");
    appearance_result(input,std::nullopt,"actor.hero",false);
    auto bad=input;*bad.data.find("actors")->find("actor.peer")->find("appearance_definition")->find("kind")=V{"generated"};appearance_result(bad,std::nullopt,"actor.hero",false,"InvalidPreservedActorAppearance");
    bad=input;erase(*bad.data.find("actors")->find("actor.peer"),"appearance_definition");appearance_result(bad,std::nullopt,"actor.hero",false,"InvalidPreservedActorAppearance");
    bad=input;*bad.data.find("actors")->find("actor.peer")->find("appearance_definition")->find("generator_version")=V{"future"};appearance_result(bad,std::nullopt,"actor.hero",false,"InvalidPreservedActorAppearance");
    bad=input;bad.scenario.as<V::Dict>()->emplace_back("player_appearance_choices",V{V::Array{}});appearance_result(bad,std::nullopt,"actor.hero",false,"UnexpectedAppearanceChoices");
}
}
// 有界门只测本次纯边界及其受影响旧调用入口；真实Main、目录和银行仍由整合门验证。
int main(){try{check_actor_requests();check_actor_author_guards();check_policy_guards();check_appearance_policies();check_configurable_roster();check_roster_preserved_appearance();std::cout<<"NATIVE_ACTOR_SELECTION "<<checks<<" checks 0 failures\n";return 0;}catch(const std::exception& error){std::cerr<<"NATIVE_ACTOR_SELECTION FAIL "<<error.what()<<'\n';return 1;}}
