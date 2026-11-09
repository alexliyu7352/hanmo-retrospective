// 新接续账本的资产/事务/坏来源组件门；夹具不冒充正式作者、World或玩家入口验收。
#include "resident/domains/family/voluntary_path_original_commands.hpp"
#include "resident/domains/family/voluntary_family_readiness.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include "resident/kernel/command_types.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
int checks=0;
// 保首个真实失败点，避免后续结果覆盖损坏现场。
void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
// 仅保存本门所需的已接受来源，完整交互取得同意留正式同代World门。
struct Fixture {
    std::shared_ptr<CommonCatalog> shared=std::make_shared<CommonCatalog>();
    CommonState seed;FamilyState family;InteractionCatalog interaction;SuccessionConsentCatalog catalog;
    Fixture(){
        seed.identity.world_id[0]=7;seed.identity.execution_epoch=3;seed.identity.revision=5;seed.identity.tick=20;seed.next_instance_id=100;seed.metadata.player_actor_id="former";seed.metadata.scenario_id="scene";family.founder_actor_id="former";
        shared->actor_ids={"former","next"};shared->places.insert("home",{"home","town",{}});shared->items.insert("grain",{"grain",1,1,false,false});
        RoleDefinition role;role.id="consent";role.initial_actor="next";role.place_id="home";shared->roles.insert(role.id,role);seed.roles.insert(role.id,{"next",true,{}});
        for(const auto* name:{"former","next"}){ActorRecord actor;actor.id=name;actor.alive=true;actor.available=true;actor.hp=100;actor.birth_tick=-30*720;actor.location=SettlementLocation{"town","home"};actor.wallet_id=StableId("cash:")+name;actor.inventory_id=StableId("bag:")+name;seed.actors.insert(name,actor);Owner owner{Owner::Kind::actor,name,{}};seed.wallets.insert(actor.wallet_id,{actor.wallet_id,owner,0,{},{}});seed.inventories.insert(actor.inventory_id,{actor.inventory_id,owner,100,0,{},{}});}
        seed.wallets.find("cash:former")->balance=80;seed.wallets.find("cash:next")->balance=2;seed.inventories.find("bag:former")->lots.push_back({"lot:7","grain",{},5,{Owner::Kind::actor,"former",{}},{}});
        AdultAdoptionFact adoption;adoption.agreement.adopter_actor_id="former";adoption.agreement.adoptee_actor_id="next";adoption.agreement.accepted_tick=0;family.facts.insert("family:1",adoption);family.children.insert("next","family:1");seed.facts.insert("family:1",{"family:1","family_adult_adoption","family",{}});
        DialogueDefinition dialogue;dialogue.id="dialogue";dialogue.context_role="consent";dialogue.entry="yes";DialogueNode node;node.id="yes";node.kind="say";node.speaker="$provider";dialogue.nodes.push_back(node);interaction.dialogues.push_back(dialogue);interaction.common=shared;
        SuccessionConsentDefinition definition;definition.id="accept";definition.predecessor_actor_id="former";definition.successor_actor_id="next";definition.dialogue_id="dialogue";definition.node_id="yes";definition.personal.money=10;definition.personal.goods.push_back({"grain",2,{}});catalog.enabled=true;catalog.definitions.insert(definition.id,definition);
        CommonStore common(seed,shared);auto assets=preview_succession_personal_assets(common,"former","next",definition.personal);check(assets.ok,"fixture private terms");SuccessionConsentFact plan;auto& a=plan.agreement;a.request_id="accept";a.relation_fact_id="family:1";a.dialogue_id="dialogue";a.node_id="yes";a.node_kind="say";a.session_id="interaction:3";a.scenario_id="scene";a.world_id=seed.identity.world_id;a.context={"home","next","consent"};a.accepted_tick=10;a.accepted_revision=2;a.predecessor_birth_tick=-30*720;a.successor_birth_tick=-30*720;a.personal=assets.value.terms;family.facts.insert("family:2",plan);seed.facts.insert("family:2",{"family:2","family_succession_consent","family",{}});
    }
};
// 组件保真比较完整Common与所有计划/结果字段，不只比较最终钱包。
BoundaryValue ledger(const FamilyState& family){using namespace boundary;BoundaryValue::Dict facts;for(const auto& row:family.facts.rows()){if(const auto* plan=std::get_if<SuccessionConsentFact>(&row.second))facts.emplace_back(row.first,encode_succession_consent_fact(*plan));else if(const auto* done=std::get_if<VoluntarySuccessionFact>(&row.second))facts.emplace_back(row.first,encode_voluntary_succession_fact(*done));}return obj({{"facts",BoundaryValue{std::move(facts)}},{"successions",string_array(family.successions)}});}
}
int main(){try{
 Fixture f;for(const auto& row:f.seed.actors.rows())f.shared->actor_initial_birth.insert(row.first,*row.second.birth_tick);f.seed.request_sequences.insert("former",1);CommonStore c(f.seed,f.shared);VoluntaryPathState state;state.commits.insert("family:2",{"result:3","former","family_agree_succession",1,3,{}});
 InteractionState interaction;InteractionAccess access;TaskState tasks;EstateState estates;FamilyAccess family_access;family_access.interaction=&interaction;family_access.interaction_access=&access;family_access.tasks=&tasks;family_access.military_or_office_pending=[](const StableId&,bool){return false;};
 VoluntaryPathCatalog policies;policies.enabled=true;VoluntaryPathDefinition d;d.id="known_request";d.template_id="template";d.accept_node_id="yes";d.withdraw_node_id="no";policies.definitions.insert(d.id,d);
 DialogueDefinition private_talk;private_talk.id="private";for(const auto* id:{"yes","no"}){DialogueNode n;n.id=id;n.kind="say";n.speaker="$provider";private_talk.nodes.push_back(n);}f.interaction.dialogues.push_back(private_talk);KnownPersonalDialogueTemplate t{"template","private",{"installed_actor","former"},{"installed_actor","next"}};
 FamilyDeathOriginRead deaths=[](const StableId&)->Outcome<std::optional<FamilyDeathOrigin>>{return {true,{},{}};};
 VoluntaryPathContext x{{c,*f.shared,f.family,nullptr,nullptr,nullptr,nullptr,deaths},policies,f.catalog,f.interaction,[&](const StableId&)->Outcome<KnownPersonalDialogueTemplate>{return {true,t,{}};},[](const StableId&,const StableId&)->Outcome<bool>{return {true,false,{}};},nullptr,[](Integer,const StableId&)->Outcome<StableId>{return {true,"former",{}};},[&](const StableId& a,const StableId& b){return validate_voluntary_family_parties_ready(c,f.family,estates,family_access,a,b);}};
 check(validate_voluntary_path_history(x,state).ok&&validate_voluntary_path_result_bindings(c,f.family,state).ok,"old actual accepted source remains usable within explicit new profile");
 FamilyCommand command;command.kind="family_continue_as";command.plan_id="family:2";auto quote=preview_original_voluntary_path_command(x,state,interaction,access,"former",command);if(!quote.ok)std::cerr<<quote.error.original.code<<'\n';check(quote.ok,"old plan uses same readiness and actual private asset read");
 FamilyCatalog selected;selected.voluntary_source_mode=FamilyVoluntarySourceMode::living_paths;FamilyCommand retire;retire.kind="family_retire";EstateCatalog ec;
 auto bypass=validate_voluntary_family_command_route(selected,retire.kind);check(!bypass.ok&&bypass.error.original.code=="VoluntaryAcceptedPlanRequired","new profile prevents unilateral legacy retirement");
 auto old_direct=validate_voluntary_family_command_route(selected,command.kind);check(!old_direct.ok&&old_direct.error.original.code=="VoluntaryPathActualResultAdapterRequired","old direct path cannot omit actual result tail");
 selected.voluntary_source_mode=FamilyVoluntarySourceMode::authored_roles;check(validate_voluntary_family_command_route(selected,retire.kind).ok&&validate_voluntary_family_command_route(selected,command.kind).ok,"original format retains original command routing");
 const auto identity=c.state().identity;const auto before_family=f.family;const auto before_state=state;auto done=apply_original_voluntary_path_command(c,f.family,state,x,interaction,access,"former",command,quote.value);check(done.ok&&done.value.player_actor_id==std::optional<StableId>{"next"}&&!done.value.actor_id,"old result effect shape preserved");
 auto publish=[&](const FamilyEffect& effect,const StableId& kind){auto r=std::make_shared<CommitResult>();r->kind=kind;r->result_id=c.next_id("result").value;r->tick=c.tick();r->revision=++c.state().identity.revision;r->details=std::make_shared<const ReceiptDetails>(ReceiptDetails{effect,{}});c.state().results.set("former:2",r);c.state().request_sequences.set("former",2);c.state().history.push_back({c.tick(),kind,"former",r->result_id,{}});return r;};
 auto result=publish(done.value,command.kind);check(bind_voluntary_path_result_in_candidate(c,identity,before_family,before_state,f.family,state,"former",2,*result).ok,"old completion actual result binds original submitter");check(c.wallet("cash:former")->balance==70&&c.wallet("cash:next")->balance==12&&state.completion_revisions.size()==1,"one old asset writer, one new result source");
 c.state()=f.seed;f.family=before_family;state=before_state;command.kind="family_cancel_succession";quote=preview_original_voluntary_path_command(x,state,interaction,access,"former",command);check(quote.ok,"old pending cancel available");auto cancelled=apply_original_voluntary_path_command(c,f.family,state,x,interaction,access,"former",command,quote.value);check(cancelled.ok&&cancelled.value.fact_id==std::optional<StableId>{"family:2"}&&state.cancellations.size()==1,"old cancel keeps original effect and true new cause");result=publish(cancelled.value,command.kind);check(bind_voluntary_path_result_in_candidate(c,identity,before_family,before_state,f.family,state,"former",2,*result).ok,"old cancel cause bound to exact old command result");check(c.state().metadata.player_actor_id=="former"&&c.wallet("cash:former")->balance==80,"cancel leaves old assets and control untouched");
 std::cout<<"VOLUNTARY_ORIGINAL_ADAPTER "<<checks<<" checks 0 failures\n";return 0;}catch(const std::exception& e){std::cerr<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
