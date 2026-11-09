// 原Common/Family分项资产与来源的局部门；Known真实会话用显式只读桩，不冒World验收。
#include "resident/domains/family/voluntary_path_views.hpp"
#include "resident/domains/family/voluntary_path_control.hpp"
#include "resident/kernel/command_types.hpp"
#include "resident/storage/common_codec.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
Integer checks=0;void check(bool yes,const char* why){++checks;if(!yes)throw std::runtime_error(why);}
struct Reader final:KnownPersonalDialogueRead {
 KnownPersonalDialogueProof proof;bool refused=false;mutable StableId seen_fact;
 Outcome<KnownPersonalDialogueProof> known_personal_dialogue(const StableId& a,const StableId& b,const StableId& t)const override {
  if(refused||a!=proof.declaration.meeting.actor_id||b!=proof.declaration.meeting.provider_actor_id||t!=proof.template_id)return {false,{},common_failure("FixtureNoActualMeeting").error};return {true,proof,{}};
 }
 Status known_personal_dialogue_before_fact(const KnownPersonalDialogueProof& p,Integer tick,const StableId& fact)const override {
  seen_fact=fact;return !refused&&p.opening_fact_id==proof.opening_fact_id&&tick==p.declaration.meeting.source_identity.tick?common_success():common_failure("FixtureWrongMeeting");
 }
};
}
int main(){try{
 auto shared=std::make_shared<CommonCatalog>();CommonState seed;FamilyState family;family.founder_actor_id="parent";FamilyLineageCatalog lineage;
 seed.identity.world_id[0]=1;seed.identity.execution_epoch=2;seed.identity.revision=4;seed.identity.tick=10;
 seed.next_instance_id=40;seed.metadata.player_actor_id="parent";seed.metadata.scenario_id="scene";
 shared->places.insert("home",{"home","town",{}});shared->items.insert("grain",{"grain",1,1,false,false});
 for(const auto* id:{"parent","child"}){shared->actor_ids.push_back(id);ActorRecord a;a.id=id;a.alive=a.available=true;a.hp=100;a.birth_tick=StableId{id}=="parent"?-40*720:-20*720;a.location=SettlementLocation{"town","home"};a.wallet_id=StableId{"cash:"}+id;a.inventory_id=StableId{"bag:"}+id;shared->actor_initial_birth.insert(id,*a.birth_tick);seed.actors.insert(id,a);Owner owner{Owner::Kind::actor,id,{}};seed.wallets.insert(a.wallet_id,{a.wallet_id,owner,50,{},{}});seed.inventories.insert(a.inventory_id,{a.inventory_id,owner,100,0,{},{}});}
 seed.inventories.find("bag:parent")->lots.push_back({"lot:2","grain",{},5,{Owner::Kind::actor,"parent",{}},{}});
 lineage.enabled=true;lineage.endowment.families.insert("insight",{"insight",{"rites"}});lineage.origins.insert("child",{"child",{"parent"}});
 seed.facts.insert("known_personal_dialogue_opened:4",{"known_personal_dialogue_opened:4","known_personal_dialogue_opened","known_personal_dialogues"});
 CommonStore common(seed,shared);InteractionCatalog interactions;interactions.common=shared;DialogueDefinition talk;talk.id="talk";
 for(const auto* id:{"living_yes","living_no"}){DialogueNode n;n.id=id;n.kind="say";n.speaker="$provider";talk.nodes.push_back(n);}interactions.dialogues.push_back(talk);
 VoluntaryPathCatalog catalog;catalog.enabled=true;VoluntaryPathDefinition d;d.id="living";d.template_id="template";d.accept_node_id="living_yes";d.withdraw_node_id="living_no";d.relation_kind=VoluntaryRelationKind::biological_child;d.personal.money=3;d.personal.goods.push_back({"grain",2,{}});catalog.definitions.insert("living",d);SuccessionConsentCatalog old;
 KnownPersonalDialogueTemplate t{"template","talk",{"installed_actor","parent"},{"installed_actor","child"}};Reader reader;auto& p=reader.proof;
 p.opening_fact_id="known_personal_dialogue_opened:4";p.template_id="template";p.listener_origin={"installed_actor","parent",{},{},{}};p.provider_origin={"installed_actor","child",{},{},{}};p.declaration.speaker_actor_id="child";auto& m=p.declaration.meeting;m.source_identity=seed.identity;m.actor_id="parent";m.provider_actor_id="child";m.session_id="interaction:5";m.dialogue_id="talk";m.node_id="living_yes";m.node_kind="say";m.context={"home","child",""};m.context.kind="narrative";
 FamilyDeathOriginRead deaths=[](const StableId&)->Outcome<std::optional<FamilyDeathOrigin>>{return {true,{},{}};};
 VoluntaryPathContext x{{common,*shared,family,&lineage,nullptr,nullptr,nullptr,deaths},catalog,old,interactions,
   [&](const StableId&)->Outcome<KnownPersonalDialogueTemplate>{return {true,t,{}};},
   [](const StableId&,const StableId&)->Outcome<bool>{return {true,false,{}};},&reader,
   [&](Integer tick,const StableId& fact){return read_voluntary_player_before_fact(common,family,nullptr,tick,fact);}};

 VoluntaryPathState state;InteractionState interaction_state;InteractionAccess interaction_access;
 InteractionSession current;current.actor_id="parent";current.context=m.context;current.phase="say";current.dialogue_id=m.dialogue_id;current.node_id=m.node_id;interaction_state.interactions.push_back(current);
 auto view=query_voluntary_path_page(x,state,interaction_state,interaction_access,0);check(view.ok&&boundary::get_int(view.value,"total","")==1&&view.value.find("rows")->as<BoundaryValue::Array>()->size()==1,"finite page only current actual provider offer");reader.refused=true;view=query_voluntary_path_page(x,state,interaction_state,interaction_access,0);check(view.ok&&boundary::get_int(view.value,"total","")==0,"no actual private say yields no hidden relation candidate");reader.refused=false;
 check(!query_voluntary_path_page(x,state,interaction_state,interaction_access,-1).ok,"negative page rejected");
 AdultAdoptionFact adopted;adopted.agreement.adopter_actor_id="parent";adopted.agreement.adoptee_actor_id="child";adopted.agreement.accepted_tick=8;family.facts.insert("family:6",adopted);common.state().facts.insert("family:6",{"family:6","family_adult_adoption","family"});catalog.definitions.find("living")->relation_kind=VoluntaryRelationKind::adoption;
 view=query_voluntary_path_page(x,state,interaction_state,interaction_access,0);check(view.ok&&boundary::get_int(view.value,"total","")==1,"real adopted pair offered with exact original relationship");
 const auto& offered=view.value.find("rows")->as<BoundaryValue::Array>()->front();const auto& act=offered.find("actions")->as<BoundaryValue::Array>()->front();check(boundary::get_string(*act.find("params"),"relation_fact_id","")=="family:6","page emits original adoption fact in command");
 adopted.agreement.adoptee_actor_id="unrelated";family.facts.set("family:6",adopted);view=query_voluntary_path_page(x,state,interaction_state,interaction_access,0);check(view.ok&&boundary::get_int(view.value,"total","")==0,"other relationship cannot offer unseen candidate");
 family.facts.erase("family:6");common.state().facts.erase("family:6");catalog.definitions.find("living")->relation_kind=VoluntaryRelationKind::biological_child;


 auto bad=decode_voluntary_path_command("family_voluntary_path_unknown",boundary::obj({}));check(!bad.ok,"unknown prefix rejected");
 auto parsed=decode_voluntary_path_command("family_voluntary_path_accept",boundary::obj({{"request_id",BoundaryValue{"living"}},{"successor_actor_id",BoundaryValue{"child"}}}));check(parsed.ok,"strict actual accept command");
 check(!decode_voluntary_path_command("family_voluntary_path_accept",boundary::obj({{"request_id",BoundaryValue{"living"}},{"successor_actor_id",BoundaryValue{"child"}},{"scope",BoundaryValue{"all"}}})).ok,"unexpected asset scope rejected");
 check(!preview_voluntary_path_command(x,state,"child",parsed.value).ok,"provider NPC cannot invoke player command");
 auto quote=preview_voluntary_path_command(x,state,"parent",parsed.value);check(quote.ok,"actual provider acceptance preview");auto stale=quote.value;stale.consent->agreement.personal.money++;
 const auto before=common.state().identity;const auto before_family=family;const auto before_state=state;auto original=encode_common_state(common.state());
 check(!apply_voluntary_path_command(common,family,state,x,"parent",parsed.value,stale).ok&&compare_boundary(original,encode_common_state(common.state())).exact(),"changed acceptance terms refuse without writes");
 auto accepted=apply_voluntary_path_command(common,family,state,x,"parent",parsed.value,quote.value);check(accepted.ok&&accepted.value.actor_id==std::optional<StableId>{"parent"}&&accepted.value.heir_actor_id==std::optional<StableId>{"child"},"actual accept uses original Family only");
 auto publish=[&](const StableId& kind,const FamilyEffect& effect,Integer sequence){auto r=std::make_shared<CommitResult>();r->result_id=common.next_id("result").value;r->kind=kind;r->tick=common.tick();r->revision=++common.state().identity.revision;r->details=std::make_shared<const ReceiptDetails>(ReceiptDetails{effect,{}});common.state().request_sequences.set("parent",sequence);common.state().results.set("parent:"+std::to_string(sequence),r);common.state().history.push_back({common.tick(),kind,"parent",r->result_id,{}});return r;};
 auto actual=publish(parsed.value.kind,accepted.value,1);check(bind_voluntary_path_result_in_candidate(common,before,before_family,before_state,family,state,"parent",1,*actual).ok,"actual accepted command result binds");
 check(!preview_voluntary_path_command(x,state,"parent",parsed.value).ok,"same declaration not reusable while plan pending");
 auto saved=encode_common_state(common.state());view=query_voluntary_path_page(x,state,interaction_state,interaction_access,0);check(view.ok&&boundary::get_int(view.value,"total","")==1&&compare_boundary(saved,encode_common_state(common.state())).exact(),"accepted plan page is pure and offers no duplicate consent");

 auto continuation=decode_voluntary_path_command("family_voluntary_path_continue",boundary::obj({{"plan_id",BoundaryValue{*accepted.value.fact_id}}}));check(continuation.ok,"exact continue names old plan");
 check(!preview_voluntary_path_command(x,state,"parent",continuation.value).ok,"original stable read required before changing control");
 x.stable_parties=[](const StableId&,const StableId&){return common_success();};auto finish=preview_voluntary_path_command(x,state,"parent",continuation.value);check(finish.ok,"explicit stable original party read permits quote");
 const auto saved_common=encode_common_state(common.state());const auto saved_family=encode_family_state(family);const auto saved_state=encode_voluntary_path_state(state);const auto* family_address=&family;
 x.player_before=[&](Integer,const StableId& id)->Outcome<StableId>{return id==*accepted.value.fact_id?Outcome<StableId>{true,"parent",{}}:Outcome<StableId>{false,{},common_failure("FixtureLateOriginalControlFailure").error};};
 auto failed=apply_voluntary_path_command(common,family,state,x,"parent",continuation.value,finish.value);
 check(!failed.ok&&failed.error.original.code=="FixtureLateOriginalControlFailure","late original source refusal reaches public command");
 check(compare_boundary(saved_common,encode_common_state(common.state())).exact()&&compare_boundary(saved_family,encode_family_state(family)).exact()&&compare_boundary(saved_state,encode_voluntary_path_state(state)).exact()&&family_address==&x.people.family,"public rollback restores Common IDs lots control and same borrowed Family/state");
 x.player_before=[&](Integer tick,const StableId& fact){return read_voluntary_player_before_fact(common,family,nullptr,tick,fact);};const auto before_done=common.state().identity;const auto f_before=family;const auto s_before=state;
 auto done=apply_voluntary_path_command(common,family,state,x,"parent",continuation.value,finish.value);check(done.ok&&done.value.player_actor_id==std::optional<StableId>{"child"}&&common.wallet("cash:parent")->balance==47,"one original completion transfers selected assets and control");
 actual=publish(continuation.value.kind,done.value,2);check(bind_voluntary_path_result_in_candidate(common,before_done,f_before,s_before,family,state,"parent",2,*actual).ok,"new controller does not replace original submitter");
 check(!preview_voluntary_path_command(x,state,"parent",continuation.value).ok,"former player cannot submit next control action");
 auto before_control=read_voluntary_player_before_fact(common,family,nullptr,common.tick(),*done.value.fact_id);check(before_control.ok&&before_control.value=="parent","exact completion head excludes its own control change");
 check(!read_voluntary_player_before_fact(common,family,nullptr,common.tick()+1,*done.value.fact_id).ok,"future observation refused");
 check(!read_voluntary_player_before_fact(common,family,nullptr,common.tick(),"known_personal_dialogue_opened:4").ok,"non-voluntary boundary refused");
 auto old_head=*common.state().facts.find(*accepted.value.fact_id);common.state().facts.erase(*accepted.value.fact_id);common.state().facts.insert(*accepted.value.fact_id,old_head);
 check(!read_voluntary_player_before_fact(common,family,nullptr,common.tick(),*accepted.value.fact_id).ok,"same-tick shifted acceptance cannot move across actual control");

 std::cout<<"VOLUNTARY_PATH_VIEWS "<<checks<<" checks 0 failures\n";return 0;
}catch(const std::exception& e){std::cerr<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
