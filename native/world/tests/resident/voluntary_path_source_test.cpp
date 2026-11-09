// 原Common/Family分项资产与来源的局部门；Known真实会话用显式只读桩，不冒World验收。
#include "resident/domains/family/voluntary_path_source.hpp"
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
  seen_fact=fact;return !refused&&p.opening_fact_id==proof.opening_fact_id&&tick==proof.declaration.meeting.source_identity.tick?common_success():common_failure("FixtureWrongMeeting");
 }
};
}
int main(){try{
 auto shared=std::make_shared<CommonCatalog>();CommonState seed;FamilyState family;FamilyLineageCatalog lineage;
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
   [](Integer,const StableId&)->Outcome<StableId>{return {true,"parent",{}};}};
 const auto original=encode_common_state(common.state());auto made=preview_voluntary_path_consent(x,"parent","child","living");
 if(!made.ok)std::cerr<<made.error.original.code<<'\n';check(made.ok,"real living source with original selected private assets");const auto saved=made.value;
 check(saved.agreement.relation_fact_id.empty()&&saved.path.biological&&saved.path.biological->kind==BiologicalChildOriginKind::installed_lineage,"Installed ancestry never pretends to be fact");
 check(saved.agreement.personal.lots.size()==1&&saved.agreement.personal.lots[0].lot_id=="lot:2"&&saved.agreement.personal.money==3,"original exact lot and money retained");
 check(revalidate_voluntary_path_consent(x,saved).ok,"same current source revalidates");
 auto json=encode_voluntary_path_source(saved.path);auto restored=decode_voluntary_path_source(json);check(restored.ok&&voluntary_path_source_equal(saved.path,restored.value),"JSON source exact roundtrip");
 check(compare_boundary(json,encode_voluntary_path_source(restored.value)).exact(),"JSON field order preserved");
 CodecVisitor::Buffer bytes;auto copy=restored.value;CodecVisitor writer(bytes);check(visit_voluntary_path_source(writer,copy).ok&&writer.finish().ok,"explicit new typed source writes");
 const auto& input=bytes;CodecVisitor typed(input);VoluntaryPathSource decoded;check(visit_voluntary_path_source(typed,decoded).ok&&typed.finish().ok&&voluntary_path_source_equal(saved.path,decoded),"typed source restores exact original");
 auto wrong=json;*wrong.find("relation_kind")=BoundaryValue{"adoption"};check(!decode_voluntary_path_source(wrong).ok,"bio tag cannot relabel adoption");
 wrong=json;*wrong.find("statement")->find("declaration")->find("speaker_actor_id")=BoundaryValue{"parent"};check(!decode_voluntary_path_source(wrong).ok,"initiator cannot speak successor acceptance");
 check(!preview_voluntary_path_consent(x,"child","parent","living").ok,"NPC cannot submit as player");
 m.node_id="living_no";check(!preview_voluntary_path_consent(x,"parent","child","living").ok,"withdraw sentence cannot accept");m.node_id="living_yes";
 p.declaration.speaker_actor_id="parent";check(!preview_voluntary_path_consent(x,"parent","child","living").ok,"actual provider speaker is required");p.declaration.speaker_actor_id="child";
 reader.refused=true;check(!preview_voluntary_path_consent(x,"parent","child","living").ok,"real Known refusal propagated");reader.refused=false;
 m.source_identity.revision=3;check(!preview_voluntary_path_consent(x,"parent","child","living").ok,"stale actual meeting rejected");m.source_identity.revision=4;
 auto forged=saved;forged.path.biological->parent_actor_id="child";check(!revalidate_voluntary_path_consent(x,forged).ok,"forged ancestry cannot use same agreement");
 check(compare_boundary(original,encode_common_state(common.state())).exact(),"preview changes no Common balance ID or control");
 SuccessionConsentFact plan;plan.agreement=saved.agreement;family.facts.insert("family:10",plan);common.state().facts.insert("family:10",{"family:10","family_succession_consent","family"});
 check(validate_voluntary_path_consent_before_fact(x,saved.agreement,saved.path,"family:10").ok,"cold consumes original same Family consent");
 check(reader.seen_fact=="family:10","Known exact boundary passed through");
 auto no_player=x;no_player.player_before={};check(!validate_voluntary_path_consent_before_fact(no_player,saved.agreement,saved.path,"family:10").ok,"historical submitter cannot be guessed");
 common.state().metadata.player_actor_id="child";check(validate_voluntary_path_consent_before_fact(x,saved.agreement,saved.path,"family:10").ok,"later control does not erase earlier living acceptance");common.state().metadata.player_actor_id="parent";
 forged=saved;forged.agreement.personal.money=4;check(!validate_voluntary_path_consent_before_fact(x,forged.agreement,forged.path,"family:10").ok,"cannot replace original agreed money");
 common.state().actors.find("parent")->alive=false;common.state().facts.insert("military:12",{"military:12","military_casualties","military"});
 x.people.death_origin=[](const StableId& id)->Outcome<std::optional<FamilyDeathOrigin>>{return id=="parent"?Outcome<std::optional<FamilyDeathOrigin>>{true,FamilyDeathOrigin{id,"military:12","battle",10},{}}:Outcome<std::optional<FamilyDeathOrigin>>{true,{},{}};};
 check(validate_voluntary_path_consent_before_fact(x,saved.agreement,saved.path,"family:10").ok,"later death preserves real earlier living agreement");
 check(!preview_voluntary_path_consent(x,"parent","child","living").ok,"dead predecessor cannot seek new living succession");
 family.facts.insert("family:14",plan);common.state().facts.insert("family:14",{"family:14","family_succession_consent","family"});
 check(!validate_voluntary_path_consent_before_fact(x,saved.agreement,saved.path,"family:14").ok,"death cause before consent cannot borrow late family registration");
 std::cout<<"VOLUNTARY_PATH_SOURCE "<<checks<<" checks 0 failures\n";return 0;
}catch(const std::exception& e){std::cerr<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
