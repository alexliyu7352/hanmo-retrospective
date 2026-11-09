// 新目录只授原收件者具名私人SAY交接，绝不把普通narrative会面升级成岗位服务。
#include "resident/domains/tasks/personal_messages/task_personal_message_catalog.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {int checks=0;void require(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
struct Fixture {
 std::shared_ptr<CommonCatalog> common=std::make_shared<CommonCatalog>();TaskCatalog tasks;InteractionCatalog interaction;KnownPersonalDialogueCatalog meetings;TaskPersonalMessageCatalog catalog;
 Fixture(){common->actor_ids={"carrier","sender","recipient"};common->items.insert("letter",{"letter",1});interaction.common=common;
  TaskDefinition task;task.id="letter_offer";task.template_id="message";task.issuer={Owner::Kind::actor,"sender",{}};task.funding_wallet="sender_cash";task.accept_role="sender";task.report_role="sender";task.deadline_ticks=6;task.goal.message_id="letter";task.goal.receiver_actor="recipient";task.on_unavailable="wait_or_leave";tasks.offers.insert(task.id,task);
  DialogueNode say;say.id="receive_letter";say.kind="say";say.speaker="$provider";say.next="end";DialogueDefinition d;d.id="personal_receive";d.entry=say.id;d.nodes.push_back(say);DialogueNode end;end.id="end";end.kind="end";d.nodes.push_back(end);interaction.dialogues.push_back(d);
  meetings.templates.push_back({"known_recipient","personal_receive",{"installed_actor","carrier"},{"installed_actor","recipient"}});
  catalog.policies.push_back({"private_letter","letter_offer","known_recipient","receive_letter"});
 }
 Status validate(){return validate_task_personal_message_catalog(catalog,tasks,meetings,interaction);}
};}
int main(){try{Fixture f;require(f.validate().ok,"explicit known original private recipient");auto value=encode_task_personal_message_catalog(f.catalog);auto round=decode_task_personal_message_catalog(value,f.tasks,f.meetings,f.interaction);require(round.ok&&round.value.policies.size()==1,"strict directory round trip");
 {auto g=f;g.catalog.policies.push_back(g.catalog.policies.front());require(!g.validate().ok,"duplicate policy and offer reject");}
 {auto g=f;g.catalog.policies.front().personal_template_id="missing";require(!g.validate().ok,"missing real personal template");}
 {auto g=f;g.tasks.offers.find("letter_offer")->issuer.kind=Owner::Kind::organization;require(!g.validate().ok,"public letters do not acquire private authority");}
 {auto g=f;g.meetings.templates.front().provider.source_id="sender";require(!g.validate().ok,"real source must be original recipient");}
 {auto g=f;g.meetings.templates.front().provider.kind="born_prototype";require(!g.validate().ok,"static Task cannot substitute future dynamic recipient");}
 {auto g=f;g.interaction.dialogues.front().nodes.front().speaker="$player";require(!g.validate().ok,"carrier choice is not recipient statement");}
 {auto g=f;g.interaction.dialogues.front().nodes.front().speaker="recipient";require(!g.validate().ok,"Known1 requires its original provider marker");}
 {auto g=f;g.interaction.dialogues.front().nodes.front().kind="choice";require(!g.validate().ok,"choice cannot consent to taking a letter");}
 {auto g=f;g.interaction.dialogues.front().context_role="some_public_role";require(!g.validate().ok,"template is truly personal and roleless");}
 {auto g=f;g.catalog.version=2;require(!g.validate().ok,"unknown new module version");}
 {auto g=f;g.tasks.offers.find("letter_offer")->goal.receiver_actor="sender";require(!g.validate().ok,"round self-letter cannot create receipt");}
 {auto g=f;g.tasks.offers.find("letter_offer")->template_id="procurement";require(!g.validate().ok,"ordinary procurement not relabelled as personal letter");}
 auto duplicate=value;duplicate.as<BoundaryValue::Dict>()->push_back({"version",BoundaryValue{Integer{1}}});require(!decode_task_personal_message_catalog(duplicate,f.tasks,f.meetings,f.interaction).ok,"duplicate JSON version rejects before interpretation");
 std::cout<<checks<<" personal message catalog checks; original Task transfer is separate\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
