// 只核回书作者声明与原私人送信定义；不预置去程、回书、结酬或NPC同意事实。
#include "resident/domains/tasks/message_returns/task_message_return_catalog.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
int checks=0;void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
struct Fixture {
 TaskCatalog tasks;InteractionCatalog interactions;TaskMessageReturnCatalog catalog;
 Fixture(){auto common=std::make_shared<CommonCatalog>();common->actor_ids={"sender","recipient","carrier"};common->items.insert("letter",{"letter",1});common->items.insert("reply",{"reply",1});common->places.insert("door",{"door","town",{}});
  RoleDefinition role;role.id="recipient.role";role.initial_actor="recipient";role.place_id="door";role.dialogue_id="reply.talk";role.services={"deliver_message"};common->roles.insert(role.id,role);
  DialogueDefinition dialogue;dialogue.id="reply.talk";dialogue.context_role=role.id;DialogueNode node;node.id="entrust";node.kind="say";node.speaker="$provider";dialogue.nodes.push_back(node);interactions.dialogues.push_back(dialogue);interactions.common=common;
  RoleDefinition report;report.id="sender.role";report.initial_actor="sender";report.place_id="door";report.services={"report_offer"};report.dialogue_id="sender.talk";common->roles.insert(report.id,report);DialogueDefinition sender_talk;sender_talk.id="sender.talk";sender_talk.context_role=report.id;DialogueNode sender_say;sender_say.id="report";sender_say.kind="say";sender_say.speaker="$provider";sender_talk.nodes.push_back(sender_say);interactions.dialogues.push_back(sender_talk);
  TaskDefinition task;task.id="round_trip";task.template_id="message";task.issuer={Owner::Kind::actor,"sender",{}};task.goal.message_id="letter";task.goal.receiver_actor="recipient";task.deadline_ticks=10;task.report_role="sender.role";tasks.offers.insert(task.id,task);
  catalog.policies.push_back({"round_trip.reply",task.id,"reply",role.id,dialogue.id,node.id});
 }
};
}
int main(){try{
 Fixture f;check(validate_task_message_return_catalog(f.catalog,f.tasks,f.interactions).ok,"original private recipient's explicit provider SAY permits authored return terms");
 auto raw=encode_task_message_return_catalog(f.catalog);auto decoded=decode_task_message_return_catalog(raw,f.tasks,f.interactions);check(decoded.ok&&compare_boundary(raw,encode_task_message_return_catalog(decoded.value)).exact(),"return policy JSON is exact and independent");
 auto bad=f.catalog;bad.policies.front().reply_item_id="letter";check(!validate_task_message_return_catalog(bad,f.tasks,f.interactions).ok,"outbound original letter cannot stand in for a distinct return");
 bad=f.catalog;bad.policies.push_back(bad.policies.front());check(!validate_task_message_return_catalog(bad,f.tasks,f.interactions).ok,"one original Offer cannot declare duplicate return policies");
 f.tasks.offers.find("round_trip")->issuer.kind=Owner::Kind::organization;check(!validate_task_message_return_catalog(f.catalog,f.tasks,f.interactions).ok,"public or delegated issuer is outside private return profile");f.tasks.offers.find("round_trip")->issuer.kind=Owner::Kind::actor;
 f.interactions.dialogues.front().nodes.front().speaker="sender";check(!validate_task_message_return_catalog(f.catalog,f.tasks,f.interactions).ok,"sender or unrelated voice cannot declare recipient's reply");f.interactions.dialogues.front().nodes.front().speaker="$provider";
 f.interactions.dialogues.front().nodes.front().kind="choice";check(!validate_task_message_return_catalog(f.catalog,f.tasks,f.interactions).ok,"player choice cannot replace provider SAY");f.interactions.dialogues.front().nodes.front().kind="say";
 auto common=std::make_shared<CommonCatalog>(*f.interactions.common);common->roles.find("recipient.role")->initial_actor="carrier";f.interactions.common=common;check(!validate_task_message_return_catalog(f.catalog,f.tasks,f.interactions).ok,"current or substitute role actor cannot replace immutable original recipient");
 std::cout<<checks<<" return catalog checks; actual Task chain deferred\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
