// 原Common/Family分项资产与来源的局部门；Known真实会话用显式只读桩，不冒World验收。
#include "resident/domains/family/voluntary_path_receipts.hpp"
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

 VoluntaryPathState state;
 auto publish=[&](const StableId& id,const StableId& kind,const StableId& successor,bool completed,Integer sequence){
  auto result=std::make_shared<CommitResult>();result->result_id=common.next_id("result").value;result->kind=kind;result->tick=common.tick();result->revision=++common.state().identity.revision;
  FamilyEffect effect;effect.success=true;effect.actor_id="parent";effect.fact_id=id;effect.player_actor_id=completed?successor:"parent";if(!successor.empty())effect.heir_actor_id=successor;
  result->details=std::make_shared<const ReceiptDetails>(ReceiptDetails{effect,{}});common.state().request_sequences.set("parent",sequence);common.state().results.set("parent:"+std::to_string(sequence),result);common.state().history.push_back({common.tick(),kind,"parent",result->result_id,{}});return result;
 };
 const auto original_identity=common.state().identity;const auto original_family=family;const auto empty=state;
 auto quote=preview_voluntary_path_consent(x,"parent","child","living");check(quote.ok,"actual source quote");
 auto copy_family=family;check(!accept_voluntary_path_in_candidate(common,copy_family,state,x,quote.value).ok,"callback cannot borrow another Family object");
 auto accepted=accept_voluntary_path_in_candidate(common,family,state,x,quote.value);if(!accepted.ok)std::cerr<<accepted.error.original.code<<'\n';check(accepted.ok,"accept writes same original Family ledger");
 check(accepted.value=="family:40"&&family.successions.empty()&&common.wallet("cash:parent")->balance==50,"accept allocates only original consent, no assets or control");
 check(!validate_voluntary_path_result_bindings(common,family,state).ok,"unbound acceptance cannot be published");
 auto first=publish(accepted.value,"family_voluntary_path_accept","child",false,1);
 check(bind_voluntary_path_result_in_candidate(common,original_identity,original_family,empty,family,state,"parent",1,*first).ok,"exact actual result binds original acceptance");
 check(read_voluntary_path_source_for_result(common,family,state,*first).value==accepted.value,"original cached result source exact read");
 auto copied=*first;check(!read_voluntary_path_source_for_result(common,family,state,copied).ok,"copied result identity refused");
 const auto accepted_state=state;const auto accepted_family=family;const auto accepted_snapshot=common.state();const auto accepted_common=encode_common_state(common.state());
 check(!preview_accepted_voluntary_path_assets(x,state,accepted.value).ok,"missing original stable qualification refuses completion");
 bool stable_called=false;x.stable_parties=[&](const StableId& a,const StableId& b){stable_called=true;return a=="parent"&&b=="child"?common_success():common_failure("FixtureWrongStableParties");};
 auto assets=preview_accepted_voluntary_path_assets(x,state,accepted.value);check(assets.ok&&stable_called,"complete reuses required original stable party read");
 const auto before_done_identity=common.state().identity;auto outer=common.begin_savepoint();check(outer.ok,"outer full candidate owns Common savepoint");
 bool fail_completion=true;x.player_before=[&](Integer,const StableId& fact)->Outcome<StableId>{return fail_completion&&fact!=accepted.value?Outcome<StableId>{false,{},common_failure("FixtureLateCompletionSource").error}:Outcome<StableId>{true,"parent",{}};};
 auto failed=complete_voluntary_path_in_candidate(common,family,state,x,accepted.value,assets.value);check(!failed.ok&&failed.error.original.code=="FixtureLateCompletionSource"&&common.wallet("cash:parent")->balance==47,"actual late source refusal occurs after original resource writer");
 check(common.rollback_savepoint(outer.value).ok,"outer candidate rolls back real Common writes");family=accepted_family;state=accepted_state;
 check(compare_boundary(accepted_common,encode_common_state(common.state())).exact()&&family.successions.empty(),"late source rollback restores money lots IDs and control");fail_completion=false;outer=common.begin_savepoint();check(outer.ok,"caller reopens original candidate point");
 auto done=complete_voluntary_path_in_candidate(common,family,state,x,accepted.value,assets.value);if(!done.ok)std::cerr<<done.error.original.code<<'\n';check(done.ok,"new path uses original Family writer without nested savepoint");
 check(family.successions==std::vector<StableId>{done.value}&&common.state().metadata.player_actor_id=="child"&&common.wallet("cash:parent")->balance==47&&common.wallet("cash:child")->balance==53,"single original control plus selected money moves");
 check(common.inventory("bag:parent")->lots.front().quantity==3&&common.inventory("bag:child")->lots.front().quantity==2,"original selected lot recipe moves exact amount");
 check(common.commit_savepoint(outer.value).ok,"caller alone commits completed Common point");
 auto second=publish(done.value,"family_voluntary_path_continue","child",true,2);
 check(bind_voluntary_path_result_in_candidate(common,before_done_identity,accepted_family,accepted_state,family,state,"parent",2,*second).ok,"completion stays attributed to original submitter");
 auto json=encode_voluntary_path_state(state);auto cold=decode_voluntary_path_state(json);check(cold.ok&&compare_boundary(json,encode_voluntary_path_state(cold.value)).exact()&&validate_voluntary_path_result_bindings(common,family,cold.value).ok,"JSON preserves original actual request/result sources");
 CodecVisitor::Buffer bytes;CodecVisitor writer(bytes);check(visit_voluntary_path_checkpoint(writer,cold.value).ok&&writer.finish().ok,"new independent typed tail writes");const auto& input=bytes;CodecVisitor read(input);VoluntaryPathState restored;check(visit_voluntary_path_checkpoint(read,restored).ok&&read.finish().ok&&compare_boundary(json,encode_voluntary_path_state(restored)).exact(),"new tail exact roundtrip");
 auto bad=state;bad.commits.find(done.value)->actor_id="child";check(!validate_voluntary_path_result_bindings(common,family,bad).ok,"new controlled actor cannot replace completion submitter");
 bad=state;--*bad.completion_revisions.find(done.value);check(!validate_voluntary_path_result_bindings(common,family,bad).ok,"before revision not inferred or movable");
 // Start an independent pending-plan fixture branch; direct test cache writes are outside Common savepoint tracking.
 common.state()=accepted_snapshot;family=accepted_family;state=accepted_state;
 // Same tick later result cannot be attached to the first retained old acceptance.
 common.state().history.clear();common.state().results={};const auto before_unrelated=common.state().identity;
 auto reused=publish(accepted.value,"family_voluntary_path_accept","child",false,2);
 check(!validate_voluntary_path_result_bindings(common,family,state).ok,"pruned cache cannot reattach old consent to new same tick result");
 common.state().results={};common.state().history.clear();check(validate_voluntary_path_result_bindings(common,family,state).ok,"legitimate cache pruning preserves permanent result binding");
 // An unrelated advance with no voluntary delta is not constrained to zero time.
 common.state().identity.tick++;auto unrelated=std::make_shared<CommitResult>();unrelated->kind="advance";unrelated->tick=common.tick();unrelated->revision=++common.state().identity.revision;
 check(bind_voluntary_path_result_in_candidate(common,before_unrelated,family,state,family,state,"parent",3,*unrelated).ok,"unrelated clock movement takes unchanged-source branch");
 // Provider's own withdrawal remains separate, then uses the original plan cancellation writer.
 m.source_identity=common.state().identity;m.node_id="living_no";
 auto withdrawal=preview_voluntary_path_withdrawal(x,state,accepted.value);check(withdrawal.ok,"actual provider withdrawal sentence");
 const auto before_cancel=common.state().identity;const auto pending_family=family;const auto pending_state=state;
 auto cancelled=cancel_voluntary_path_in_candidate(common,family,state,x,accepted.value,withdrawal.value);check(cancelled.ok,"original plan canceled with independent real cause");
 check(std::get<SuccessionConsentFact>(*family.facts.find(accepted.value)).conclusion->kind=="cancelled"&&family.successions.empty()&&common.wallet("cash:parent")->balance==50,"withdrawal changes no assets or control");
 auto third=publish(cancelled.value,"family_voluntary_path_withdraw",{},false,3);check(bind_voluntary_path_result_in_candidate(common,before_cancel,pending_family,pending_state,family,state,"parent",3,*third).ok,"actual withdrawal result binds cancellation cause");
 check(!preview_accepted_voluntary_path_assets(x,state,accepted.value).ok,"withdrawn consent cannot complete");
 std::cout<<"VOLUNTARY_PATH_STATE "<<checks<<" checks 0 failures\n";return 0;
}catch(const std::exception& e){std::cerr<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
