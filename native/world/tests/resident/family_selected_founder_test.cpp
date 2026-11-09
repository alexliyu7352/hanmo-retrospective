// 新选人政策只扩作者创始人准入；保持原家庭持久字节及完整继承链，不能直接换当前玩家。
#include "resident/domains/family/family_model.hpp"
#include "resident/storage/common_codec.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
using V=BoundaryValue;
using Bytes=CodecVisitor::Buffer;
Integer checks=0;
// Release同样执行断言，报告原领域错误而不接受未验证候选。
void check(bool condition,const StableId& label){++checks;if(!condition)throw std::runtime_error(label);}
// 作者基表只供真实family decoder读取，两位可选人物之外仍有普通第三人。
V input(){
    using namespace boundary;auto attributes=obj({{"administration",V{Integer{20}}},{"charisma",V{Integer{20}}},{"intellect",V{Integer{20}}},{"leadership",V{Integer{20}}},{"martial",V{Integer{20}}}});
    V::Dict skills;for(const auto* name:{"archery","cavalry","commerce","debate","infantry","martial","negotiation","rites","tactics","transport"})skills.emplace_back(name,V{Integer{0}});
    V::Dict actors;for(const auto* name:{"hero","peer","third"})actors.emplace_back(name,obj({{"attributes",attributes},{"skills",V{skills}}}));
    return obj({{"actors",V{std::move(actors)}},{"roles",V{V::Dict{}}},{"texts",V{V::Dict{}}},{"required_capabilities",V{V::Array{}}},{"scenarios",obj({{"scenario",obj({{"player_actor",V{"hero"}},{"playable_actor_ids",string_array({"hero","peer"})},{"dynamic_world",obj({{"founder_policy",V{"selected_existing_actor_v1"}}})}})}})}});
}
// 三位既有人物保原钱包和属性；这里只跑共同的family验证，不安装世界或执行动作。
CommonState seed(const StableId& player){CommonState state;state.metadata.player_actor_id=player;state.next_instance_id=1;for(const auto* id:{"hero","peer","third"}){ActorRecord actor;actor.id=id;actor.alive=true;actor.available=true;state.actors.insert(id,actor);}return state;}
// 真typed家族codec不含目录允许表；每次验证前后比较全部家族持久字节。
Bytes checkpoint(FamilyState family){Bytes bytes;CodecVisitor writer(bytes);check(visit_family_checkpoint(writer,family).ok&&writer.finish().ok,"family typed checkpoint writes");return bytes;}
// 恢复只读原字节；没有重新建立创始人或重新生成继承事实。
FamilyState restore_family(const Bytes& bytes){FamilyState restored;CodecVisitor reader(bytes);check(visit_family_checkpoint(reader,restored).ok&&reader.finish().ok,"family typed checkpoint reads");return restored;}
// 新旧目录都走同一validate_family，失败不改当前玩家或家族原字节。
void validation(const FamilyCatalog& catalog,const StableId& founder,const StableId& player,const StableId& code={}){
    auto common_catalog=std::make_shared<CommonCatalog>();CommonStore common(seed(player),common_catalog);FamilyState family;family.founder_actor_id=founder;EstateState estates;EstateCatalog ec;
    const auto before=checkpoint(family);const auto result=validate_family(common,family,catalog,*common_catalog,estates,ec);
    check(code.empty()?result.ok:!result.ok&&result.error.original.code==code,"family result expected "+code+", got "+result.error.original.code);
    check(common.state().metadata.player_actor_id==player&&common.state().next_instance_id==1&&checkpoint(family)==before,"validation preserves player ID allocator and family bytes");
    if(result.ok){auto restored=restore_family(before);check(validate_family(common,restored,catalog,*common_catalog,estates,ec).ok&&checkpoint(restored)==before,"restored family passes same unchanged catalog guard");}
}
// 作者列表严格只属于新政策，坏名单不能悄悄变回旧单创始人默认。
void catalog_and_tamper_guards(){
    auto data=input();const auto before=render_boundary_json(data);check(before.ok,"author data serializes");auto decoded=decode_family_catalog(data,"scenario");check(decoded.ok,"real new catalog decodes");
    check(decoded.value.allowed_founder_actor_ids==std::vector<StableId>{"hero","peer"}&&decoded.value.founder_actor_id=="hero","catalog records only original authored founders");
    check(render_boundary_json(data).value==before.value,"catalog decoding preserves complete author input");
    validation(decoded.value,"hero","hero");validation(decoded.value,"peer","peer");validation(decoded.value,"third","third","InvalidFamilyFounder");
    validation(decoded.value,"hero","peer","InvalidSuccessionPlayer");validation(decoded.value,"peer","third","InvalidSuccessionPlayer");
    for(const auto& ids:std::vector<std::vector<StableId>>{{},{"hero"},{"hero","peer","third"},{"hero","hero"},{"hero",""},{"peer","third"},{"hero","missing"}}){
        auto bad=data;*bad.find("scenarios")->find("scenario")->find("playable_actor_ids")=boundary::string_array(ids);const auto snapshot=render_boundary_json(bad).value;const auto result=decode_family_catalog(bad,"scenario");
        check(!result.ok&&result.error.original.code=="InvalidPlayableActors","new-policy malformed founder list rejected");check(render_boundary_json(bad).value==snapshot,"failed author decode preserves bytes");
    }
    for(const auto* policy:{"fixed_actor","fixed_actor_appearance_v1","fixed_actor_appearance_composition_v1"}){
        auto old=data;*old.find("scenarios")->find("scenario")->find("dynamic_world")->find("founder_policy")=V{policy};const auto result=decode_family_catalog(old,"scenario");check(result.ok&&result.value.allowed_founder_actor_ids.empty(),"old policy keeps empty author allowlist");validation(result.value,"hero","hero");validation(result.value,"peer","peer","InvalidFamilyFounder");validation(result.value,"hero","third","InvalidSuccessionPlayer");
    }
}
// 复用原家事领域真实收养、指定及退休流程；被选原founder不是默认actor，生成继承人不在初始两人表。
void legitimate_succession(bool full_sixteen=false){
    auto common_catalog=std::make_shared<CommonCatalog>();common_catalog->places.insert("hall",{"hall","town",{}});RoleDefinition role;role.id="adopter";role.initial_actor="provider";role.place_id="hall";role.services={"family_adopt"};common_catalog->roles.insert(role.id,role);common_catalog->texts.insert("child_name","阿平");
    CommonState state;state.identity.tick=0;state.next_instance_id=1;state.metadata.player_actor_id="guardian";state.rng.insert("world",{1,1,{}});FamilyCatalog catalog;catalog.enabled=true;catalog.founder_actor_id="other";catalog.allowed_founder_actor_ids={"other","guardian"};
    for(const auto* id:{"guardian","provider","other"}){ActorRecord a;a.id=id;a.alive=true;a.available=true;a.hp=100;a.birth_tick=-40*720;a.created_tick=0;a.wallet_id="cash:"+a.id;a.inventory_id="bag:"+a.id;a.location=SettlementLocation{"town","hall"};state.actors.insert(id,a);state.request_sequences.insert(id,0);state.wallets.insert(a.wallet_id,{a.wallet_id,{Owner::Kind::actor,id,{}},100,{}});state.inventories.insert(a.inventory_id,{a.inventory_id,{Owner::Kind::actor,id,{}},20,0,{}});catalog.prototypes.insert(id,{});}
    IdentityPrototype prototype;prototype.attributes={20,20,20,20,20};catalog.prototypes.insert("child_prototype",prototype);FamilyConfig config;config.valid=true;config.prototype_id="child_prototype";config.child_name_key="child_name";config.wallet_id="cash:provider";config.fee=10;config.adoption_ticks=2;config.policy={17,18,18,40,{},{}};catalog.roles.insert("adopter",config);
    if(full_sixteen){common_catalog->extended_skills=true;catalog.extended_skills=true;catalog.founder_selection=FamilyFounderSelection::configured_roster;catalog.allowed_founder_actor_ids.push_back("provider");for(auto& row:state.actors.rows())row.second.skills.extended.emplace();auto& skills=catalog.prototypes.find("child_prototype")->skills;skills.extended.emplace();skills.extended->naval=50;skills.extended->espionage=60;skills.extended->fortification=70;skills.extended->agriculture=80;skills.extended->medicine=90;skills.extended->forging=100;}
    CommonStore common(std::move(state),common_catalog);FamilyState family;family.founder_actor_id="guardian";EstateState estates;EstateCatalog ec;InteractionState interaction;InteractionCatalog ic;ic.common=common_catalog;InteractionAccess ia;ia.battle_occupied=[](const auto&){return false;};ia.resolve_location=[&](const StableId& id){return common.actor(id)->location;};ia.role=[](const auto&){return std::optional<RoleAssignmentView>{{"provider",true}};};TaskState tasks;SocialState social;FamilyAccess access;access.interaction=&interaction;access.interaction_catalog=&ic;access.interaction_access=&ia;access.tasks=&tasks;access.military_or_office_pending=[](const auto&,bool){return false;};access.initialize_schedule=[](const auto&,const auto&,bool enabled){return enabled?common_failure("UnexpectedTravelSchedule"):common_success();};
    FamilyCommand command;command.kind="family_adopt";command.context={"hall","provider","adopter"};auto quote=preview_family(common,family,catalog,*common_catalog,estates,ec,access,"guardian",command);check(quote.ok&&quote.value.cost==10,"selected founder retains original adoption terms");
    if(full_sixteen){auto missing=catalog;missing.prototypes.find("child_prototype")->skills.extended.reset();auto denied=preview_family(common,family,missing,*common_catalog,estates,ec,access,"guardian",command);check(!denied.ok&&denied.error.original.code=="FamilyPrototypeSkillsRequired"&&common.state().next_instance_id==1&&common.wallet("cash:guardian")->balance==100,"full16 source missing extension fails before money or identity allocation");}
    auto id=common.next_id("action");check(id.ok,"same domain allocates one adoption action");ActionCoreView action{id.value,"family_adopt","guardian",{"guardian","provider"},0,2};ActionMutation insert;insert.kind=ActionMutation::Kind::insert;insert.action=action;check(common.apply_action(insert).ok,"adoption action installed");
    for(const auto* person:{"guardian","provider"}){ActorMutation mutation;mutation.actor_id=person;mutation.action_id=action.id;check(common.apply_actor(mutation).ok,"participants remain original people");}
    check(start_family_action(common,family,catalog,*common_catalog,estates,ec,access,action,command,quote.value).ok,"real adoption start");common.state().identity.tick=2;action.remaining_ticks=0;ActionMutation progress;progress.action=action;progress.remaining_ticks=0;check(common.apply_action(progress).ok,"adoption duration reached in fixture");
    auto completed=finish_family_action(common,family,interaction,social,catalog,*common_catalog,estates,ec,access,action);check(completed.ok&&completed.value.success==true&&completed.value.actor_id,"real child created from original adoption fact");const auto child=*completed.value.actor_id;
    for(const auto* person:{"guardian","provider"}){ActorMutation mutation;mutation.actor_id=person;mutation.action_id="";check(common.apply_actor(mutation).ok,"original participant released");}ActionMutation erased;erased.kind=ActionMutation::Kind::erase;erased.action=action;check(common.apply_action(erased).ok,"completed adoption head released");family.actions.erase(action.id);
    if(full_sixteen){const auto* born=common.state().actors.find(child);check(born&&born->skills.extended&&born->skills.extended->naval==50&&born->skills.extended->forging==100,"actual adoption copies explicit six-skill prototype values");Bytes bytes;CodecVisitor writer(bytes);writer.schema_features=common_checkpoint::extended_actor_skills;check(visit_common_checkpoint(writer,common.state()).ok&&writer.finish().ok,"actual generated child common checkpoint writes");CommonState recovered;const auto& input=bytes;CodecVisitor reader(input);reader.schema_features=common_checkpoint::extended_actor_skills;check(visit_common_checkpoint(reader,recovered).ok&&reader.finish().ok,"actual generated child common checkpoint reads");check(compare_boundary(encode_common_state(common.state()),encode_common_state(recovered)).exact(),"six-skill generated child retains complete common JSON through typed cold");common=CommonStore(std::move(recovered),common_catalog);}
    common.state().identity.tick=722;command.kind="family_designate";command.heir_actor_id=child;quote=preview_family(common,family,catalog,*common_catalog,estates,ec,access,"guardian",command);check(quote.ok,"adult generated heir can be designated");auto result=apply_family_instant(common,family,estates,catalog,*common_catalog,ec,access,"guardian",command,quote.value);check(result.ok,"real designation fact persists");
    command.kind="family_retire";quote=preview_family(common,family,catalog,*common_catalog,estates,ec,access,"guardian",command);check(quote.ok,"selected founder can retire through existing checks");result=apply_family_instant(common,family,estates,catalog,*common_catalog,ec,access,"guardian",command,quote.value);check(result.ok&&common.state().metadata.player_actor_id==child,"actual succession controls generated heir");
    check(std::find(catalog.allowed_founder_actor_ids.begin(),catalog.allowed_founder_actor_ids.end(),child)==catalog.allowed_founder_actor_ids.end()&&family.founder_actor_id=="guardian","persistent original founder is unchanged and heir is outside initial two");check(common.wallet("cash:guardian")->balance==0&&common.wallet("personal:"+child)->balance==90,"existing succession money settlement unchanged");
    const auto bytes=checkpoint(family);auto restored=restore_family(bytes);check(validate_family(common,restored,catalog,*common_catalog,estates,ec).ok&&checkpoint(restored)==bytes,"legitimate heir and original founder survive exact typed family restore");
    common.state().metadata.player_actor_id="provider";auto invalid=validate_family(common,restored,catalog,*common_catalog,estates,ec);check(!invalid.ok&&invalid.error.original.code=="InvalidSuccessionPlayer","third current player without chain rejected");common.state().metadata.player_actor_id=child;
    restored.founder_actor_id=full_sixteen?"unknown":"provider";invalid=validate_family(common,restored,catalog,*common_catalog,estates,ec);check(!invalid.ok&&invalid.error.original.code=="InvalidFamilyFounder","third original founder rejected even with otherwise valid chain");
}
}
// 单门包含新准入、原固定政策与真实继承链，不重跑选择器矩阵或触碰玩家物理银行。
int main(int argc,char** argv){try{if(argc==2&&StableId{argv[1]}=="--extended-adoption"){legitimate_succession(true);std::cout<<"FAMILY_EXTENDED_ADOPTION "<<checks<<" checks 0 failures\n";return 0;}catalog_and_tamper_guards();legitimate_succession();std::cout<<"FAMILY_SELECTED_FOUNDER "<<checks<<" checks 0 failures\n";return 0;}catch(const std::exception& error){std::cerr<<"FAMILY_SELECTED_FOUNDER FAIL "<<error.what()<<'\n';return 1;}}
