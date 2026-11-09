// 原sender明确接听销约才开放离岗办理；收件声明、旁人SAY或普通岗位不代此同意。
#include "resident/domains/tasks/personal_cancellations/task_personal_cancellation_catalog.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {int checks=0;void require(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
struct Fixture {
 std::shared_ptr<CommonCatalog> common=std::make_shared<CommonCatalog>();TaskCatalog tasks;InteractionCatalog interaction;KnownPersonalDialogueCatalog meetings;TaskPersonalMessageCatalog deliveries;TaskPersonalCancellationCatalog catalog;
 Fixture(){common->actor_ids={"carrier","sender","recipient","other"};common->items.insert("letter",{"letter",1});interaction.common=common;TaskDefinition t;t.id="letter_offer";t.template_id="message";t.issuer={Owner::Kind::actor,"sender",{}};t.funding_wallet="sender_cash";t.accept_role="sender_role";t.report_role="sender_role";t.deadline_ticks=6;t.goal.message_id="letter";t.goal.receiver_actor="recipient";t.on_unavailable="wait_or_leave";tasks.offers.insert(t.id,t);
  for(const auto* id:{"recipient_dialogue","sender_dialogue"}){DialogueNode say;say.id="accept";say.kind="say";say.speaker="$provider";say.next="end";DialogueDefinition d;d.id=id;d.entry="accept";d.nodes.push_back(say);DialogueNode end;end.id="end";end.kind="end";d.nodes.push_back(end);interaction.dialogues.push_back(d);}
  meetings.templates.push_back({"known_recipient","recipient_dialogue",{"installed_actor","carrier"},{"installed_actor","recipient"}});meetings.templates.push_back({"known_sender","sender_dialogue",{"installed_actor","carrier"},{"installed_actor","sender"}});
  deliveries.policies.push_back({"delivery","letter_offer","known_recipient","accept"});catalog.policies.push_back({"report","letter_offer","known_sender","accept"});
 }
 Status validate(){return validate_task_personal_cancellation_catalog(catalog,tasks,deliveries,meetings,interaction);}
};}
int main(){try{Fixture f;require(f.validate().ok,"exact original sender cancellation statement");auto encoded=encode_task_personal_cancellation_catalog(f.catalog);auto round=decode_task_personal_cancellation_catalog(encoded,f.tasks,f.deliveries,f.meetings,f.interaction);require(round.ok&&compare_boundary(encoded,encode_task_personal_cancellation_catalog(round.value)).exact(),"directory exact round trip");
 {auto g=f;g.deliveries.policies.clear();require(!g.validate().ok,"missing genuine delivery profile rejects");}
 {auto g=f;g.catalog.policies.front().personal_template_id="known_recipient";require(!g.validate().ok,"receipt permission is not sender cancellation permission");}
 {auto g=f;g.meetings.templates.back().provider.source_id="other";require(!g.validate().ok,"unrelated listener does not become original sender");}
 {auto g=f;g.meetings.templates.back().provider.kind="born_prototype";require(!g.validate().ok,"static sender cannot become future born person");}
 {auto g=f;g.interaction.dialogues.back().nodes.front().speaker="$player";require(!g.validate().ok,"carrier statement not sender consent");}
 {auto g=f;g.interaction.dialogues.back().nodes.front().kind="choice";require(!g.validate().ok,"choice does not replace original provider say");}
 {auto g=f;g.interaction.dialogues.back().context_role="sender_role";require(!g.validate().ok,"ordinary role dialogue is not Known personal source");}
 {auto g=f;g.catalog.policies.push_back(g.catalog.policies.front());require(!g.validate().ok,"one original offer has one cancellation policy");}
 {auto g=f;g.catalog.version=2;require(!g.validate().ok,"unknown cancellation module version rejects");}
 {auto g=f;g.tasks.offers.find("letter_offer")->issuer.kind=Owner::Kind::organization;require(!g.validate().ok,"public funding cannot borrow private sender source");}
 {auto g=f;g.catalog.policies.front().offer_id="missing";require(!g.validate().ok,"unknown original task definition rejects");}
 auto bad=encoded;bad.as<BoundaryValue::Dict>()->push_back({"version",BoundaryValue{Integer{1}}});require(!decode_task_personal_cancellation_catalog(bad,f.tasks,f.deliveries,f.meetings,f.interaction).ok,"duplicate JSON key rejects");
 std::cout<<checks<<" private sender catalog cases; original cancellation behavior separate\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
