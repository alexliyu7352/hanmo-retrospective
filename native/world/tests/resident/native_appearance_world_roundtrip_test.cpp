// 正式192组合经真实原生新局与检查点恢复；整世界JSON和失败身份核对，不以纯选择器测试冒充存读。
#include "resident/kernel/boundary_dispatch.hpp"
#include "resident/storage/common_codec.hpp"
#include <picosha2.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
using V=BoundaryValue;
Integer checks=0;
// Release也执行全部断言，保留具体拒绝码及其位置。
void check(bool condition,const StableId& label){++checks;if(!condition)throw std::runtime_error(label);}
// 只有真实成功才取生产载荷；错误详情直接交给失败日志。
template<class T>T must(Outcome<T> result,const StableId& label){check(result.ok,label+": "+result.error.original.code+" "+result.error.original.path);return std::move(result.value);}
// 无载荷的安装与恢复同样不能忽略原始失败。
void must(Status result,const StableId& label){check(result.ok,label+": "+result.error.original.code+" "+result.error.original.path);}
// JSON只用于冷边界比较和证据，不生成另一份世界或重新计算持久外貌。
StableId json(const V& value){return must(render_boundary_json(value),"render exact JSON");}
// 比较原字段顺序及值，返回可定位差异而非无序对象相等。
void exact(const V& left,const V& right,const StableId& label){const auto comparison=compare_boundary(left,right);const auto difference=comparison.value_difference?comparison.value_difference:comparison.order_difference;check(comparison.exact(),label+(difference?": "+difference->path+" "+difference->reason:""));check(json(left)==json(right),label+" exact JSON bytes");}
// 仅接已展开的实际prepared catalog；不在用例中改作者包后补摘要。
V read_catalog(const char* path){std::ifstream file(path,std::ios::binary);check(bool(file),"open prepared catalog");std::ostringstream bytes;bytes<<file.rdbuf();check(!file.bad(),"read prepared catalog");auto value=must(parse_boundary_json(bytes.str()),"parse prepared catalog");check(boundary::get_bool(value,"ok","catalog")&&value.find("data"),"prepared catalog required");return value;}
// 请求复制作者完整tuple，不传id、标签或美术路径。
V tuple(const V& choice){return boundary::obj({{"base_features",boundary::field(choice,"base_features","choice")},{"attire_style",boundary::field(choice,"attire_style","choice")}});}
// 冷请求身份沿现有完整模块顺序及实际目录字节；不省略包锁、课程或章节结果政策。
V identity(const V& catalog,const StableId& scenario){
    using namespace boundary;const auto& data=field(catalog,"data","catalog");const auto& selected=field(field(data,"scenarios","data"),scenario,"scenarios");const auto& dynamic=field(selected,"dynamic_world",scenario);
    const bool population=dynamic.find("population_military"),regional=dynamic.find("regional_life"),siege=dynamic.find("siege_integration");const auto& organization=field(dynamic,"organization","dynamic_world");const bool named=organization.find("named_membership_enabled")&&get_bool(organization,"named_membership_enabled","organization");
    V::Array modules;
    auto add=[&](const StableId& name,const StableId& version){modules.push_back(obj({{"name",V{name}},{"version",V{version}}}));};
    add("dynamic_world","local_society_v1");add("local_economy",named?"3":population?"2":"1");add("organization",named?"3":"2");add("affairs","1");add("director",named?"3":regional?"2":"1");
    if(population)add("population_military",siege?"2":"1");
    if(regional)add("regional_life","1");
    if(siege)add("siege_integration","1");
    if(named)add("named_membership","1");
    if(regional)if(const auto* outcomes=field(dynamic,"regional_life",scenario).find("outcome_topics"))if(!array(*outcomes,"outcome_topics").empty())add("regional_outcome","1");
    bool segmented=false;for(const auto& role:object(field(data,"roles","data"),"roles"))if(const auto* courses=role.second.find("skill_courses"))for(const auto& course:array(*courses,"courses"))if(const auto* policy=course.find("settlement_policy")){check(string(*policy,"settlement_policy")=="per_lesson_v1","known study policy");segmented=true;}
    if(segmented)add("study_segments","1");
    if(const auto* plan=selected.find("player_plan_profile")){check(string(*plan,"player_plan_profile")=="known_life_v1","known player plan policy");add("player_plan","1");}
    auto value=obj({{"content_id",field(catalog,"content_id","catalog")},{"content_version",field(catalog,"content_version","catalog")},{"content_digest",field(catalog,"content_digest","catalog")},{"ruleset_id",V{"hanmo.native.local_society/1"}},{"schema_sha256",field(catalog,"schema_sha256","catalog")},{"artifact_sha256",V{picosha2::hash256_hex_string(json(catalog))}},{"module_versions",V{std::move(modules)}}});
    if(const auto* pack=catalog.find("pack_set_digest"))value.as<V::Dict>()->emplace_back("pack_set_digest",*pack);
    return value;
}
// 原生session来自同一生产工厂，空实现必须明确失败。
std::unique_ptr<ResidentReplaySession> session(){auto value=create_resident_replay_session();check(bool(value),"real resident session factory");return value;}
// 不变性包含公开世界身份、全JSON、typed checkpoint与安装选择，覆盖ID/RNG/资产。
void unchanged(ResidentReplaySession& world,const WorldIdentity& before,const V& state,const Bytes& checkpoint,const V& selection){const auto after=must(world.identity(),"live identity after failure");check(after.world_id==before.world_id&&after.execution_epoch==before.execution_epoch&&after.revision==before.revision&&after.tick==before.tick,"failed operation preserves complete live identity");exact(state,must(world.export_state(),"live export after failure"),"failed operation preserves complete world");check(checkpoint==must(world.checkpoint(),"live checkpoint after failure"),"failed operation preserves typed checkpoint");exact(selection,must(world.installed_selection(),"selection after failure"),"failed operation preserves installed selection");}
// 草案坏字段只进入新session；拒绝后所有世界读口仍为NoWorld，原目录/请求逐字不变。
void reject_initial(const V& catalog,const V& request,const StableId& code){const auto input=json(catalog),raw=json(request);auto world=session();const auto rejected=world->initialize_native(catalog,request);check(!rejected.ok&&rejected.error.original.code==code,"initial rejection expected "+code+", actual "+rejected.error.original.code+" "+rejected.error.original.path);for(const auto& result:{world->identity().error.original.code,world->checkpoint().error.original.code,world->export_state().error.original.code,world->installed_selection().error.original.code})check(result=="NoWorld","failed creation leaves no installed world");check(json(catalog)==input&&json(request)==raw,"failed creation leaves original inputs unchanged");}
// 成功外貌仍是原七字段profile及完整九槽，不补默认或引入第二套选择身份。
const V& appearance(const V& state,const StableId& player){return boundary::field(boundary::field(boundary::field(state,"actors","state"),player,"actors"),"appearance",player);}
// 两条恢复均在同一进程；install/restore共用单调epoch分配器，故必须异于旧session。
// epoch不进入JSON或checkpoint，持久world、逻辑时钟与全部持久字节仍须完全相同。
void restored_exact(ResidentReplaySession& world,const WorldIdentity& prior,const V& state,const Bytes& bytes){const auto current=must(world.identity(),"restored identity");check(current.world_id==prior.world_id&&current.execution_epoch!=prior.execution_epoch&&current.tick==prior.tick&&current.revision==prior.revision,"restore keeps persistent identity and changes execution epoch");exact(state,must(world.export_state(),"restored full state"),"complete restore JSON including appearance RNG and IDs");check(must(world.checkpoint(),"restored checkpoint")==bytes,"restored typed checkpoint bytes unchanged");}
// 三族各一非默认组合在真实世界开局、同session恢复和新session冷装载走同一持久合同。
void roundtrip(const V& catalog,const V& request,const V& selected,const V& baseline,const StableId& player){
    auto world=session();auto raw=request;raw.as<V::Dict>()->emplace_back("appearance_request",tuple(selected));must(world->initialize_native(catalog,raw),"initialize actual native composition world");
    const auto initial=must(world->export_state(),"export actual composition world");const auto& profile=appearance(initial,player);using namespace boundary;
    fields(profile,{"format_version","generator_version","generation_seed","base_features","attire_style","source","parent_sources"},{},"appearance");
    check(get_int(profile,"format_version","appearance")==1&&get_string(profile,"generator_version","appearance")=="appearance-v1"&&get_string(profile,"generation_seed","appearance")=="not-sampled"&&get_string(profile,"source","appearance")=="custom"&&array(field(profile,"parent_sources","appearance"),"parent_sources").empty(),"original custom non-sampled appearance protocol");
    check(compare_boundary(field(profile,"base_features","appearance"),field(selected,"base_features","choice")).values_equal&&get_string(profile,"attire_style","appearance")==get_string(selected,"attire_style","choice"),"all nine selected slots and attire installed exactly");
    auto expected=baseline;*expected.find("actors")->find(player)->find("appearance")=profile;exact(expected,initial,"appearance selection changes no other world value or JSON order");
    const auto before=must(world->identity(),"before restore identity");const auto bytes=must(world->checkpoint(),"serialize actual world");const auto selection=must(world->installed_selection(),"actual installed selection");
    auto repeated=world->initialize_native(catalog,V{Integer{7}});check(!repeated.ok&&repeated.error.original.code=="AlreadyInstalled","installed-world guard retains priority over malformed new draft");unchanged(*world,before,initial,bytes,selection);
    auto damaged=bytes;check(!damaged.empty(),"nonempty checkpoint");damaged.back()^=1u;const auto rejected=world->restore(damaged);check(!rejected.ok&&rejected.error.original.code=="CheckpointDigestMismatch","changing final digest byte preserves CheckpointDigestMismatch first error, actual "+rejected.error.original.code);std::cout<<"rejected_corruption="<<rejected.error.original.code<<" path="<<rejected.error.original.path<<'\n';unchanged(*world,before,initial,bytes,selection);
    must(world->restore(bytes),"restore actual serialized bytes");restored_exact(*world,before,initial,bytes);
    const auto latest=must(world->identity(),"same-session restored identity");
    auto cold=session();must(cold->initialize_checkpoint(catalog,bytes),"fresh session initializes only from persisted checkpoint");restored_exact(*cold,before,initial,bytes);
    check(must(cold->identity(),"fresh-session restored identity").execution_epoch!=latest.execution_epoch,"fresh session also cannot reuse immediately preceding restored epoch");
    auto altered=catalog;*altered.find("content_digest")=V{"not-the-saved-content-digest"};auto refused=session();const auto mismatch=refused->initialize_checkpoint(altered,bytes);check(!mismatch.ok&&mismatch.error.original.code=="NativeCatalogIdentityMismatch","cold restore does not relax saved catalog digest");check(refused->identity().error.original.code=="NoWorld","catalog mismatch installs no world");
    std::cout<<"appearance_json="<<json(profile)<<" world_json_sha256="<<picosha2::hash256_hex_string(json(initial))<<" checkpoint_sha256="<<picosha2::hash256_hex_string(bytes)<<'\n';
}
}
// 参数必须指向本轮真实准备产物及其情景，缺输入不能以跳过代替通过。
int main(int argc,char** argv){try{
    check(argc==3,"Usage: test_resident_native_appearance_world_roundtrip PREPARED_CATALOG SCENARIO_ID");const auto catalog=read_catalog(argv[1]);const StableId scenario=argv[2];using namespace boundary;
    const auto& selected=field(field(field(catalog,"data","catalog"),"scenarios","data"),scenario,"scenarios");check(get_string(field(selected,"dynamic_world",scenario),"founder_policy","dynamic_world")=="fixed_actor_appearance_composition_v1","actual new composition policy required");const auto player=get_string(selected,"player_actor",scenario);const auto& choices=array(field(selected,"player_appearance_choices",scenario),"player_appearance_choices");check(choices.size()==192,"author table must contain all192 choices");
    const auto before=json(catalog);const auto request=obj({{"scenario_id",V{scenario}},{"world_seed",field(selected,"seed_text",scenario)},{"founder_selection",V{player}},{"expected_catalog_identity",identity(catalog,scenario)}});
    auto default_world=session();must(default_world->initialize_native(catalog,request),"initialize unchanged authored default world");const auto baseline=must(default_world->export_state(),"default full state");check(get_string(appearance(baseline,player),"source","appearance")=="fixed","no request preserves authored fixed appearance");
    for(const auto* face:{"medium_clear_long","female_round_broad","long_narrow"}){const V* chosen=nullptr;for(const auto& choice:choices){const auto& f=field(choice,"base_features","choice");if(get_string(f,"face_shape","")==face&&get_string(f,"eyes","")=="narrow"&&get_string(f,"brows","")=="straight"&&get_string(f,"nose","")=="round"&&get_string(f,"mouth","")=="smile")chosen=&choice;}check(chosen,"required authored family tuple exists");roundtrip(catalog,request,*chosen,baseline,player);}
    auto malformed=request;malformed.as<V::Dict>()->emplace_back("appearance_request",tuple(choices.front()));*malformed.find("appearance_request")->find("base_features")->find("eyes")=V{"not-covered"};reject_initial(catalog,malformed,"AppearanceChoiceNotAllowed");
    *malformed.find("founder_selection")=V{player+".outside"};reject_initial(catalog,malformed,"InvalidPlayableActor");
    malformed=request;*malformed.find("expected_catalog_identity")->find("artifact_sha256")=V{"wrong-artifact"};reject_initial(catalog,malformed,"NativeCatalogIdentityMismatch");
    malformed=request;malformed.as<V::Dict>()->emplace_back("appearance_request",tuple(choices.front()));malformed.find("appearance_request")->as<V::Dict>()->emplace_back("appearance_id",V{"forbidden"});reject_initial(catalog,malformed,"UnsupportedResidentField");
    check(json(catalog)==before,"all native creation and restore cases leave exact prepared catalog unchanged");std::cout<<"NATIVE_APPEARANCE_WORLD_ROUNDTRIP "<<checks<<" checks 0 failures; 3 authored family tuples, complete JSON/checkpoint restore, unchanged non-appearance world, strict digest and failed installation boundaries\n";return 0;
}catch(const boundary::DecodeFailure& error){std::cerr<<"NATIVE_APPEARANCE_WORLD_ROUNDTRIP FAIL "<<error.error.original.code<<" "<<error.error.original.path<<'\n';return 1;}catch(const std::exception& error){std::cerr<<"NATIVE_APPEARANCE_WORLD_ROUNDTRIP FAIL "<<error.what()<<'\n';return 1;}}
