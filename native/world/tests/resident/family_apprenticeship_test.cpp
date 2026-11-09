// 师承提议、接受、退出与外层事务必要源码门；不冒完整 World 或实际学习验收。
#include "resident/domains/family/family_apprenticeship.hpp"
#include "resident/domains/family/family_birth.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {int checks=0;void check(bool condition,const char* message){++checks;if(!condition)throw std::runtime_error(message);}}
int main(){try{
    auto catalog=std::make_shared<CommonCatalog>();catalog->actor_ids={"teacher","student"};catalog->places.insert("home",{"home","town",{}});RoleDefinition role;role.id="teacher_role";role.initial_actor="teacher";role.place_id="home";catalog->roles.insert(role.id,role);
    CommonState initial;initial.identity.world_id[0]=7;initial.identity.execution_epoch=3;initial.identity.revision=5;initial.identity.tick=20;initial.metadata.player_actor_id="student";initial.metadata.scenario_id="apprenticeship";initial.next_instance_id=100;initial.roles.insert(role.id,{"teacher",true,{}});
    for(const auto* id:{"teacher","student"}){ActorRecord a;a.id=id;a.birth_tick=20-18*720;a.alive=a.available=true;a.hp=100;a.location=SettlementLocation{"town","home"};initial.actors.insert(id,a);}
    CommonStore common(initial,catalog);
    InteractionCatalog interactions;interactions.common=catalog;DialogueDefinition dialogue;dialogue.id="offer";dialogue.context_role=role.id;dialogue.entry="say";DialogueNode node;node.id="say";node.kind="say";node.speaker="$provider";dialogue.nodes.push_back(node);node.id="end";dialogue.nodes.push_back(node);interactions.dialogues.push_back(dialogue);
    InteractionState meeting;InteractionSession session;session.id="interaction:3";session.actor_id="student";session.dialogue_id="offer";session.node_id="say";session.phase="say";session.context={"home","teacher",role.id};meeting.interactions.push_back(session);
    InteractionAccess access;access.battle_occupied=[](const StableId&){return false;};access.resolve_location=[&](const StableId& actor){return common.actor(actor)->location;};access.role=[&](const StableId& name)->std::optional<RoleAssignmentView>{const auto* r=common.state().roles.find(name);return r?std::optional<RoleAssignmentView>{{r->actor_id,r->enabled}}:std::nullopt;};
    ApprenticeshipCatalog definitions;definitions.enabled=true;ApprenticeshipStatementDefinition d;d.id="offer_commerce";d.speaker_actor_id="teacher";d.speaker_side="teacher";d.skill_id="commerce";d.dialogue_id="offer";d.node_id="say";definitions.statements.insert(d.id,d);
    ApprenticeshipIdentityRead people;people.player_before=[](Integer,const StableId&)->Outcome<StableId>{return {true,"student",{}};};people.personally_available=[](const StableId&,Integer,const StableId&){return common_success();};
    FamilyApprenticeshipState state;
    auto quote=preview_apprenticeship_statement(common,meeting,interactions,access,definitions,people,"student",d.id);check(quote.ok,"actual source quote");
    auto mark=common.begin_savepoint();check(mark.ok,"caller owns only Common savepoint");
    auto proposal=receive_apprenticeship_proposal_in_candidate(common,meeting,interactions,access,definitions,state,people,quote.value);check(proposal.ok,"receive inside existing savepoint without nested failure");check(common.commit_savepoint(mark.value).ok,"caller commits source");
    auto timeline=validate_family_apprenticeship(common,interactions,definitions,state,people);check(timeline.ok&&timeline.value.bonds.empty(),"receiving statement grants no relation or XP");
    check(!receive_apprenticeship_proposal_in_candidate(common,meeting,interactions,access,definitions,state,people,quote.value).ok,"duplicate pending pair rejected without writes");
    common.state().identity.revision++;
    auto confirm=preview_apprenticeship_statement(common,meeting,interactions,access,definitions,people,"student",d.id);check(confirm.ok,"listener separately confirms current real statement");
    mark=common.begin_savepoint();check(mark.ok,"accept caller savepoint");auto accepted=reply_apprenticeship_proposal_in_candidate(common,meeting,interactions,access,definitions,state,people,proposal.value,"accepted",confirm.value);check(accepted.ok,"same candidate accept");check(common.commit_savepoint(mark.value).ok,"accept outer commit");timeline=validate_family_apprenticeship(common,interactions,definitions,state,people);check(timeline.ok&&timeline.value.bonds.size()==1&&timeline.value.bonds.find(accepted.value)->student_actor_id=="student","one real relationship uses existing student");
    const auto before_exit=encode_common_state(common.state());const auto state_before=state;
    common.state().facts.insert("source:2",{"source:2","original_boundary","test"});auto known=read_apprenticeship_bond_before_fact(common,state,accepted.value,20,"source:2");check(known.ok&&known.value&&!known.value->exit_fact_id,"original accepted relationship at actual boundary");
    check(!reply_apprenticeship_proposal_in_candidate(common,meeting,interactions,access,definitions,state,people,proposal.value,"refused",std::nullopt).ok,"terminal reply cannot repeat or reverse");
    // 晚历史门失败发生在真实 append 后，由原外层点恢复 Common 和本账；不能提交调用方点。
    const auto snapshot=encode_common_state(common.state());const auto backup=state;const auto initial_next=common.state().next_instance_id;
    auto late=people;late.personally_available=[&](const StableId&,Integer,const StableId& boundary){const auto* head=common.state().facts.find(boundary);return head&&head->kind=="family_apprenticeship_exited"?common_failure("FixtureLateSourceFailure"):common_success();};
    mark=common.begin_savepoint();check(mark.ok,"late caller savepoint");auto rejected=exit_apprenticeship_in_candidate(common,interactions,definitions,state,late,accepted.value);check(!rejected.ok&&rejected.error.original.code=="FixtureLateSourceFailure","actual late dependency failure");state=backup;check(common.rollback_savepoint(mark.value).ok,"caller can rollback its still-owned point");check(compare_boundary(snapshot,encode_common_state(common.state())).exact()&&state.exits.empty()&&common.state().next_instance_id==initial_next,"late failure leaves no relation closure or ID allocation");
    common.state().identity.revision++;
    mark=common.begin_savepoint();auto exited=exit_apprenticeship_in_candidate(common,interactions,definitions,state,people,accepted.value);check(exited.ok&&common.commit_savepoint(mark.value).ok,"participant may end personal relationship");
    timeline=validate_family_apprenticeship(common,interactions,definitions,state,people);check(timeline.ok&&timeline.value.bonds.find(accepted.value)->exit_fact_id==std::optional<StableId>{exited.value},"history retains exact ended relationship");
    known=read_apprenticeship_bond_before_fact(common,state,accepted.value,20,"source:2");check(known.ok&&known.value&&!known.value->exit_fact_id,"later same tick exit cannot alter old boundary");
    common.state().facts.insert("source:3",{"source:3","original_boundary","test"});known=read_apprenticeship_bond_before_fact(common,state,accepted.value,20,"source:3");check(known.ok&&known.value&&known.value->exit_fact_id.has_value(),"later query retains exit source");check(!exit_apprenticeship_in_candidate(common,interactions,definitions,state,people,accepted.value).ok,"cannot exit same bond twice");
    auto json=encode_family_apprenticeship(state);auto cold=decode_family_apprenticeship(json);check(cold.ok&&validate_family_apprenticeship(common,interactions,definitions,cold.value,people).ok,"complete ledger cold semantics");
    using V=BoundaryValue;json.find("replies")->find(accepted.value)->as<V::Dict>()->emplace_back("decision",V{"refused"});check(!decode_family_apprenticeship(json).ok,"duplicate decision cannot hide accepted payload");
    auto broken=state;broken.replies.find(accepted.value)->confirmation.reset();check(!validate_family_apprenticeship(common,interactions,definitions,broken,people).ok,"header alone cannot supply speaker acceptance");
    broken=state;broken.exits.find(exited.value)->actor_id="outsider";check(!validate_family_apprenticeship(common,interactions,definitions,broken,people).ok,"third party cannot end relationship");
    check(common.state().actors.find("student")->skills.commerce==0&&common.state().actors.find("teacher")->skills.commerce==0,"relationship does not mint skill XP");
    std::cout<<"APPRENTICESHIP_LEDGER "<<checks<<" checks 0 failures\n";return 0;
}catch(const std::exception& e){std::cerr<<"APPRENTICESHIP_LEDGER "<<checks<<" checks 1 failure: "<<e.what()<<'\n';return 1;}}
