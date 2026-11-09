// 正式1.8两位既有人物经真实新局与冷恢复；完整世界只允许控制者、创始人与自主调度三处差异。
#include "resident/kernel/boundary_dispatch.hpp"
#include "resident/kernel/native_initial_state.hpp"
#include "resident/kernel/world_codec.hpp"
#include "resident/storage/common_codec.hpp"
#include <picosha2.h>
#include <algorithm>
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
    if(get_string(dynamic,"founder_policy","dynamic_world")=="selected_existing_actor_v1")add("selected_existing_actor","1");
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
// 只删除合同明确排除的当前玩家自主日程；不得拿整份Director替换来掩盖其他人物变化。
void erase_schedule(V& state,const StableId& actor){
    auto* schedules=state.find("director")->find("schedules");check(schedules&&schedules->find(actor),"baseline contains selected NPC schedule");
    auto& rows=*schedules->as<V::Dict>();rows.erase(std::remove_if(rows.begin(),rows.end(),[&](const auto& row){return row.first==actor;}),rows.end());
}
// 两次新局共用同一作者世界；除三个有名差异外，完整字段、插入序与JSON字节均须相同。
void compare_starts(const V& baseline,const V& selected,const StableId& first,const StableId& second){
    using namespace boundary;
    check(get_string(baseline,"player_actor_id","state")==first&&get_string(selected,"player_actor_id","state")==second,"each requested existing actor becomes player");
    check(get_string(field(baseline,"family","state"),"founder_actor_id","family")==first&&get_string(field(selected,"family","state"),"founder_actor_id","family")==second,"family starts from explicitly selected person");
    check(!field(field(selected,"director","state"),"schedules","director").find(second),"selected Shen Heng has no autonomous review schedule");
    check(!field(field(selected,"director","state"),"plans","director").find(second),"selected Shen Heng has no autonomous plan");
    for(const auto* field_name:{"actors","wallets","inventories","relations","estates","roles","rng","request_sequences","organization","local_economy","dynamic_tasks"})
        exact(field(baseline,field_name,"state"),field(selected,field_name,"state"),StableId{"unchanged complete domain: "}+field_name);
    auto expected=baseline;*expected.find("player_actor_id")=V{second};*expected.find("family")->find("founder_actor_id")=V{second};erase_schedule(expected,second);
    exact(expected,selected,"only player metadata family founder and one Director schedule differ");
}
// 对应真初态核个人财产和九槽，不把玩家身份当作赠钱、领装备或重抽外貌的许可。
void authored_person(const V& catalog,const V& state,const StableId& scenario,const StableId& actor){
    using namespace boundary;
    const auto& definition=field(field(field(catalog,"data","catalog"),"scenarios","data"),scenario,"scenarios");
    const auto& initial=field(definition,"initial",scenario);const V* authored=nullptr;
    for(const auto& row:array(field(initial,"actors","initial"),"actors"))if(get_string(row,"actor_id","actor")==actor)authored=&row;
    check(authored,"selected actor has actual scenario snapshot");const auto& actual=field(field(state,"actors","state"),actor,"actors");
    exact(field(actual,"attributes",actor),field(*authored,"attributes",actor),"all selected actor attributes remain authored");
    exact(field(actual,"skills",actor),field(*authored,"skills",actor),"all selected actor skills remain authored");
    check(get_string(field(actual,"location",actor),"place_id","location")==get_string(*authored,"place_id",actor),"selected person remains at actual snapshot place");
    const auto& fixed=field(*authored,"appearance_definition",actor);const auto& profile=appearance(state,actor);
    // 作者JSON与typed初始外貌的规范槽序不同；跨DTO核全部九槽值，世界与冷恢复仍用exact逐字节核对。
    std::cout<<"AUTHOR_APPEARANCE_JSON="<<json(obj({{"actor_id",V{actor}},{"definition",fixed}}))<<'\n';
    std::cout<<"RUNTIME_APPEARANCE_JSON="<<json(obj({{"actor_id",V{actor}},{"profile",profile}}))<<'\n';
    check(compare_boundary(field(profile,"base_features","appearance"),field(fixed,"base_features","definition")).values_equal,"all nine fixed appearance slots remain authored across DTO ordering");
    check(get_string(profile,"source","appearance")=="fixed"&&get_string(profile,"attire_style","appearance")==get_string(fixed,"attire_style","definition"),"no appearance override keeps original fixed profile");
    const auto wallet_id=get_string(actual,"wallet_id",actor);const V* wallet=nullptr;
    for(const auto& row:array(field(initial,"wallets","initial"),"wallets"))if(get_string(row,"id","wallet")==wallet_id)wallet=&row;
    check(wallet,"selected personal wallet comes from authored initial table");
    check(get_int(field(field(state,"wallets","state"),wallet_id,"wallets"),"balance","wallet")==get_int(*wallet,"initial_balance","wallet"),"selected personal money remains authored");
}
// 将现有身份helper的完整结果转为同一生产目录准入结构，不重算或省略任何版本字段。
CatalogIdentity typed_identity(const V& value){
    using namespace boundary;CatalogIdentity result;
    result.content_id=get_string(value,"content_id","");result.content_version=get_string(value,"content_version","");result.content_digest=get_string(value,"content_digest","");result.ruleset_id=get_string(value,"ruleset_id","");result.schema_sha256=get_string(value,"schema_sha256","");result.artifact_sha256=get_string(value,"artifact_sha256","");
    if(const auto* digest=value.find("pack_set_digest"))result.pack_set_digest=string(*digest,"");
    for(const auto& row:array(field(value,"module_versions",""),""))result.module_versions.emplace_back(get_string(row,"name",""),get_string(row,"version",""));
    return result;
}
// 真实完整seed进入正式有效目录与ResidentWorld安装门；必须因指定控制链拒绝，不能借其他错误算通过。
void reject_cold_seed(const V& catalog,const StableId& scenario,const V& state,const StableId& code){
    const auto before=json(state);auto prepared=must(prepare_native_catalog(catalog,scenario,typed_identity(identity(catalog,scenario))),"prepare exact native catalog for cold seed");
    auto seed=must(decode_world_seed(state,*prepared),"decode complete authentic cold seed");ResidentWorld world;
    const auto denied=world.install(prepared,std::move(seed));check(!denied.ok&&denied.error.original.code==code,"cold seed rejection expected "+code+", actual "+denied.error.original.code+" "+denied.error.original.path);
    check(world.identity().error.original.code=="NoWorld","rejected cold seed installs no world");check(json(state)==before,"cold rejection preserves original input bytes");
}
// 一份沈蘅检查点覆盖原session恢复和新session冷装载，完整身份、全态与typed字节保持。
void shen_roundtrip(ResidentReplaySession& world,const V& catalog,const V& request,const V& state){
    const auto prior=must(world.identity(),"Shen initial identity");const auto checkpoint=must(world.checkpoint(),"Shen checkpoint");const auto selected=must(world.installed_selection(),"Shen installed selection");
    const auto denied=world.initialize_native(catalog,request);check(!denied.ok&&denied.error.original.code=="AlreadyInstalled","already installed world rejects another creation");unchanged(world,prior,state,checkpoint,selected);
    auto damaged=checkpoint;check(!damaged.empty(),"nonempty Shen checkpoint");damaged.back()^=1u;const auto bad=world.restore(damaged);check(!bad.ok&&bad.error.original.code=="CheckpointDigestMismatch","bad checkpoint retains exact digest rejection");unchanged(world,prior,state,checkpoint,selected);
    must(world.restore(checkpoint),"restore same-session Shen checkpoint");restored_exact(world,prior,state,checkpoint);
    auto cold=session();must(cold->initialize_checkpoint(catalog,checkpoint),"fresh-session restore of actual Shen checkpoint");restored_exact(*cold,prior,state,checkpoint);exact(selected,must(cold->installed_selection(),"cold Shen installed selection"),"cold restore retains explicit selected person");
    std::cout<<"shen_world_json_sha256="<<picosha2::hash256_hex_string(json(state))<<" checkpoint_sha256="<<picosha2::hash256_hex_string(checkpoint)<<'\n';
}
}
// 必须提供真实1.8运行目录及情景；两次初始化均无外貌覆盖，不执行额外活动或生成第三人物。
int main(int argc,char** argv){try{
    using namespace boundary;check(argc==3,"Usage: test_resident_native_existing_actor_world PREPARED_CATALOG SCENARIO_ID");const auto catalog=read_catalog(argv[1]);const StableId scenario=argv[2];const auto original=json(catalog);
    const auto& definition=field(field(field(catalog,"data","catalog"),"scenarios","data"),scenario,"scenarios");check(get_string(field(definition,"dynamic_world",scenario),"founder_policy","")=="selected_existing_actor_v1","actual two-actor policy required");
    const auto ids=strings(field(definition,"playable_actor_ids",scenario),"playable_actor_ids");check(ids==std::vector<StableId>{"society::actor.apprentice","society::actor.study_swordswoman"},"exact two authored Qingchuan people required");
    const auto first=ids[0],second=ids[1];auto request=obj({{"scenario_id",V{scenario}},{"world_seed",field(definition,"seed_text",scenario)},{"founder_selection",V{first}},{"expected_catalog_identity",identity(catalog,scenario)}});
    auto cheng=session();must(cheng->initialize_native(catalog,request),"initialize actual Cheng He once without appearance override");const auto baseline=must(cheng->export_state(),"Cheng complete world");authored_person(catalog,baseline,scenario,first);
    *request.find("founder_selection")=V{second};auto shen=session();must(shen->initialize_native(catalog,request),"initialize actual Shen Heng once without appearance override");const auto selected=must(shen->export_state(),"Shen complete world");authored_person(catalog,selected,scenario,second);compare_starts(baseline,selected,first,second);
    auto invalid=request;*invalid.find("founder_selection")=V{"society::actor.tutor"};reject_initial(catalog,invalid,"InvalidPlayableActor");
    auto tampered=selected;*tampered.find("family")->find("founder_actor_id")=V{"society::actor.tutor"};reject_cold_seed(catalog,scenario,tampered,"InvalidFamilyFounder");
    tampered=selected;*tampered.find("player_actor_id")=V{first};reject_cold_seed(catalog,scenario,tampered,"InvalidSuccessionPlayer");
    shen_roundtrip(*shen,catalog,invalid,selected);check(json(catalog)==original,"all consumers preserve actual prepared catalog bytes");
    std::cout<<"NATIVE_EXISTING_ACTOR_WORLD "<<checks<<" checks 0 failures; two authentic starts, complete authored world, exact Shen cold roundtrip, rejected third founder and unproven current player\n";return 0;
}catch(const boundary::DecodeFailure& error){std::cerr<<"NATIVE_EXISTING_ACTOR_WORLD FAIL "<<error.error.original.code<<" "<<error.error.original.path<<'\n';return 1;}catch(const std::exception& error){std::cerr<<"NATIVE_EXISTING_ACTOR_WORLD FAIL "<<error.what()<<'\n';return 1;}}
