// 只核S4明确邀学绑定；当面节点由S3真实规则证明，不把此组件门称作World授徒完成。
#include "resident/domains/growth/growth_teaching_requests.hpp"
#include "resident/storage/common_codec.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {int checks=0;void check(bool v,const char* why){++checks;if(!v)throw std::runtime_error(why);}}
int main(){try{
    auto shared=std::make_shared<CommonCatalog>();shared->actor_ids={"teacher","student","other"};shared->personal_skill_kinds.insert("heart","heart");shared->places.insert("school",{"school","town",{}});
    RoleDefinition role;role.id="learner";role.initial_actor="student";role.place_id="school";shared->roles.insert(role.id,role);
    CommonState seed;seed.identity={WorldId{7},2,4,10};seed.next_instance_id=20;
    for(const auto* id:{"teacher","student","other"}){ActorRecord actor;actor.id=id;actor.alive=true;actor.available=true;actor.hp=100;actor.location=SettlementLocation{"town","school"};seed.actors.insert(id,actor);}CommonStore common(seed,shared);
    InteractionCatalog interaction;interaction.common=shared;DialogueDefinition dialogue;dialogue.id="learning";dialogue.context_role="learner";dialogue.entry="ask";
    DialogueNode ask;ask.id="ask";ask.kind="say";dialogue.nodes.push_back(ask);DialogueNode other;other.id="other";other.kind="choice";dialogue.nodes.push_back(other);interaction.dialogues.push_back(dialogue);
    InteractionState state;InteractionSession session;session.id="meeting";session.actor_id="teacher";session.dialogue_id="learning";session.node_id="ask";session.phase="say";session.context={"school","student","learner"};state.interactions.push_back(session);
    InteractionAccess access;access.battle_occupied=[](const auto&){return false;};access.resolve_location=[&](const StableId& id){return common.actor(id)->location;};access.role=[](const auto&){return std::optional<RoleAssignmentView>{{"student",true}};};
    GrowthTeachingRequestCatalog catalog;catalog.enabled=true;catalog.definitions.insert("request.heart",{"request.heart","teacher","student","heart","learning","ask"});
    const auto before=encode_common_state(common.state());
    check(validate_growth_teaching_request_catalog(catalog,*shared,interaction).ok,"explicit teaching request catalog accepted");
    auto preview=[&](){return preview_growth_teaching_request(common,state,interaction,access,catalog,"teacher","student","heart");};
    auto quote=preview();check(quote.ok&&quote.value.agreement.request_id=="request.heart"&&quote.value.agreement.student_actor_id=="student","only the current declared student request resolves");
    check(revalidate_growth_teaching_request(common,state,interaction,access,catalog,quote.value).ok,"same source may be accepted before occupation");
    auto forged=quote.value;forged.agreement.skill_id="forged";check(!revalidate_growth_teaching_request(common,state,interaction,access,catalog,forged).ok,"cannot replace the requested skill in quote");
    forged=quote.value;forged.agreement.request_id="another";check(!revalidate_growth_teaching_request(common,state,interaction,access,catalog,forged).ok,"cannot swap request identity");
    check(!preview_growth_teaching_request(common,state,interaction,access,catalog,"other","student","heart").ok,"another teacher cannot borrow the meeting");
    check(!preview_growth_teaching_request(common,state,interaction,access,catalog,"teacher","other","heart").ok,"another student cannot be implied");
    check(!preview_growth_teaching_request(common,state,interaction,access,catalog,"teacher","student","unknown").ok,"unknown or undeclared skill does not create an invitation");
    auto second=session;second.id="second.meeting";second.actor_id="other";state.interactions.push_back(second);catalog.definitions.insert("other.request",{"other.request","other","student","heart","learning","ask"});
    auto second_quote=preview_growth_teaching_request(common,state,interaction,access,catalog,"other","student","heart");check(second_quote.ok,"second actual meeting resolves only its own teacher request");
    forged=quote.value;forged.agreement=second_quote.value.agreement;check(!revalidate_growth_teaching_request(common,state,interaction,access,catalog,forged).ok,"two individually valid sources cannot be spliced into one acceptance");state.interactions.pop_back();catalog.definitions.erase("other.request");
    state.interactions.front().node_id="other";state.interactions.front().phase="choice";check(!preview().ok,"ordinary dialogue node is not student willingness");check(!revalidate_growth_teaching_request(common,state,interaction,access,catalog,quote.value).ok,"previous request node cannot be accepted after leaving it");state.interactions.front()=session;
    ++common.state().identity.execution_epoch;check(!revalidate_growth_teaching_request(common,state,interaction,access,catalog,quote.value).ok,"old execution epoch cannot accept teaching");--common.state().identity.execution_epoch;
    auto old=catalog;catalog.enabled=false;check(!preview().ok,"disabled profile rejects table and invitation");catalog=old;
    catalog.definitions.insert("duplicate",{"duplicate","teacher","student","heart","learning","ask"});check(!validate_growth_teaching_request_catalog(catalog,*shared,interaction).ok,"same binding cannot select an ambiguous request");catalog=old;
    catalog.definitions.find("request.heart")->student_actor_id="teacher";check(!validate_growth_teaching_request_catalog(catalog,*shared,interaction).ok,"self teaching is not a student request");catalog=old;
    catalog.definitions.find("request.heart")->node_id="missing";check(!validate_growth_teaching_request_catalog(catalog,*shared,interaction).ok,"unused unknown node rejected at catalog boundary");catalog=old;
    auto agreement=quote.value.agreement;state.interactions.clear();++common.state().identity.execution_epoch;
    check(validate_growth_teaching_agreement_source(common.state(),*shared,interaction,catalog,agreement).ok,"permanent accepted source survives closed session and new epoch");
    agreement.context.encounter_id="orphan";check(!validate_growth_teaching_agreement_source(common.state(),*shared,interaction,catalog,agreement).ok,"cold source rejects orphan road authority");agreement=quote.value.agreement;
    agreement.context.place_id="another";check(!validate_growth_teaching_agreement_source(common.state(),*shared,interaction,catalog,agreement).ok,"cold source cannot move the historical meeting to an unrelated place");agreement=quote.value.agreement;
    agreement.accepted_tick=common.tick()+1;check(!validate_growth_teaching_agreement_source(common.state(),*shared,interaction,catalog,agreement).ok,"cold source cannot claim future agreement");agreement=quote.value.agreement;
    agreement.student_actor_id="other";check(!validate_growth_teaching_agreement_source(common.state(),*shared,interaction,catalog,agreement).ok,"cold source remains bound to declared student");agreement=quote.value.agreement;
    common.state().actors.find("student")->alive=false;check(validate_growth_teaching_agreement_source(common.state(),*shared,interaction,catalog,agreement).ok,"later death does not erase an accepted historical source");common.state().actors.find("student")->alive=true;--common.state().identity.execution_epoch;
    check(compare_boundary(before,encode_common_state(common.state())).exact()&&common.state().facts.empty(),"request preview and source validation do not teach or create facts");
    std::cout<<"growth_teaching_requests checks="<<checks<<" failures=0; source component only\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
