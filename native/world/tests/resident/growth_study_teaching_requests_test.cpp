// 新学生请求只取得实际课程报价与接受来源，不开课、扣费或写人物经验。
#include "resident/domains/growth/growth_study_teaching_requests.hpp"
#include "resident/storage/common_store.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
int checks=0;
void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
struct Fixture {
    std::shared_ptr<CommonCatalog> shared=std::make_shared<CommonCatalog>();
    CommonStore common{CommonState{},shared};GrowthState growth;GrowthCatalog catalog;SocialState social;
    InteractionState interaction;InteractionCatalog dialogue;InteractionAccess contact;GrowthAccess access;
    GrowthStudyTeachingRequestCatalog requests;
    Fixture(){
        shared->extended_skills=true;shared->actor_ids={"teacher","student","other"};shared->places.insert("school",{"school","town",{}});
        shared->roles.insert("teacher.role",{"teacher.role","teacher","school",{},{},{"study_skill"},6});
        shared->roles.insert("student.role",{"student.role","student","school",{},{},{},6});
        common.state().identity={WorldId{23},4,8,20};common.state().metadata.player_actor_id="teacher";
        for(const StableId person:{"teacher","student","other"}){
            ActorRecord actor;actor.id=person;actor.alive=true;actor.available=true;actor.hp=100;actor.birth_tick=-22*720;
            actor.location=SettlementLocation{"town","school"};actor.wallet_id=person+".wallet";actor.skills.extended=ActorExtendedSkills{};
            actor.skills.extended->medicine=person=="student"?80:650;common.state().actors.insert(person,actor);
            common.state().wallets.insert(actor.wallet_id,{actor.wallet_id,{Owner::Kind::actor,person,{}},person=="student"?1000:0,{}});
        }
        common.state().roles.insert("teacher.role",{"teacher",true,{}});common.state().roles.insert("student.role",{"student",true,{}});
        GrowthCourse course;course.skill_id="medicine";course.mode="teacher";course.wallet_id="teacher.wallet";
        GrowthRole role;role.courses.push_back(course);catalog.roles.insert("teacher.role",role);catalog.cultivation=true;
        dialogue.common=shared;DialogueDefinition definition;definition.id="request";definition.context_role="student.role";definition.entry="ask";
        DialogueNode node;node.id="ask";node.kind="say";definition.nodes.push_back(node);node.id="other";definition.nodes.push_back(node);dialogue.dialogues.push_back(definition);
        InteractionSession session;session.id="meeting";session.actor_id="teacher";session.dialogue_id="request";session.node_id="ask";session.phase="say";session.context={"school","student","student.role"};interaction.interactions.push_back(session);
        contact.resolve_location=[this](const StableId& actor){return common.actor(actor)->location;};contact.battle_occupied=[](const auto&){return false;};
        contact.role=[this](const StableId& id)->std::optional<RoleAssignmentView>{const auto* r=common.state().roles.find(id);return r?std::optional<RoleAssignmentView>{{r->actor_id,r->enabled}}:std::nullopt;};
        access.contact=[this](const StableId& actor,const InteractionContext& context,const StableId& service,const StableId& own){return validate_interaction(common,interaction,dialogue,contact,actor,context,service,own);};
        requests.enabled=true;requests.definitions.insert("request.medicine",{"request.medicine","teacher","student","medicine","teacher.role","request","ask",2,"student_personal_tuition_v1","ordinary_next_level_v1"});
    }
    GrowthStudyTeachingRead read()const{return {common,growth,catalog,*shared,social,access,interaction,dialogue,contact};}
    Outcome<GrowthStudyTeachingQuote> preview()const{return preview_growth_study_teaching_request(read(),requests,"teacher","student","medicine",2);}
};
}
int main(){try{
    Fixture f;const auto before=encode_common_state(f.common.state());const auto quote=f.preview();
    check(quote.ok,"declared adult student request must resolve the original medicine course");
    const auto* terms=std::get_if<StudyTerms>(&quote.value.student_course.data);
    check(terms&&terms->student_id=="student"&&terms->teacher_id=="teacher"&&terms->fee==60&&terms->payer_wallet_id=="student.wallet"&&terms->recipient_wallet_id=="teacher.wallet","fees and parties come from original study quote");
    check(quote.value.source.provider_actor_id=="student"&&terms->context.provider_actor_id=="teacher","student request and teacher course retain distinct real contexts");
    check(revalidate_growth_study_teaching_request(f.read(),f.requests,quote.value).ok,"same current source and complete original quote revalidate");
    auto forged=quote.value;std::get<StudyTerms>(forged.student_course.data).fee=1;
    check(!revalidate_growth_study_teaching_request(f.read(),f.requests,forged).ok,"a valid source cannot authorize forged tuition");
    forged=quote.value;forged.agreement.duration=6;check(!revalidate_growth_study_teaching_request(f.read(),f.requests,forged).ok,"request cannot silently expand agreed time");
    f.common.state().actors.find("student")->skills.extended->medicine=300;
    check(!revalidate_growth_study_teaching_request(f.read(),f.requests,quote.value).ok,"actual student level and tuition change require a new complete quote");f.common.state().actors.find("student")->skills.extended->medicine=80;
    f.requests.enabled=false;check(!f.preview().ok,"ordinary teaching requires its explicit enabled profile");f.requests.enabled=true;
    f.interaction.interactions.front().node_id="other";check(!f.preview().ok&&!revalidate_growth_study_teaching_request(f.read(),f.requests,quote.value).ok,"general dialogue or an old request node gives no new consent");f.interaction.interactions.front().node_id="ask";
    ++f.common.state().identity.execution_epoch;check(!revalidate_growth_study_teaching_request(f.read(),f.requests,quote.value).ok,"old execution epoch cannot charge a student");--f.common.state().identity.execution_epoch;
    f.common.state().wallets.find("student.wallet")->balance=59;check(!f.preview().ok,"actual personal funds remain required");f.common.state().wallets.find("student.wallet")->balance=1000;
    f.common.state().actors.find("student")->birth_tick=20-17*720;check(f.preview().ok,"ordinary medicine course retains original study eligibility without borrowing office adulthood");f.common.state().actors.find("student")->birth_tick=-22*720;
    f.common.state().roles.find("teacher.role")->actor_id="other";check(!f.preview().ok,"teacher cannot borrow another actor teaching post");f.common.state().roles.find("teacher.role")->actor_id="teacher";
    f.requests.definitions.find("request.medicine")->tariff="arbitrary";check(!f.preview().ok,"unknown payment terms fail closed");f.requests.definitions.find("request.medicine")->tariff="ordinary_next_level_v1";
    auto saved=f.interaction;f.interaction.interactions.clear();++f.common.state().identity.execution_epoch;
    check(validate_growth_study_teaching_agreement(f.common,*f.shared,f.catalog,f.dialogue,f.requests,quote.value.agreement,*terms).ok,"accepted history does not need the old session to remain open");f.interaction=saved;--f.common.state().identity.execution_epoch;
    check(compare_boundary(before,encode_common_state(f.common.state())).exact()&&f.growth.facts.empty(),"preview and source checks do not teach, reserve or spend");
    Fixture two;two.shared->roles.insert("other.teacher",{"other.teacher","other","school",{},{},{"study_skill"},6});two.common.state().roles.insert("other.teacher",{"other",true,{}});
    GrowthRole other_role;auto other_course=two.catalog.roles.find("teacher.role")->courses.front();other_course.wallet_id="other.wallet";other_role.courses.push_back(other_course);two.catalog.roles.insert("other.teacher",other_role);
    auto other_session=two.interaction.interactions.front();other_session.actor_id="other";other_session.id="other.meeting";two.interaction.interactions.push_back(other_session);
    two.requests.definitions.insert("request.other",{"request.other","other","student","medicine","other.teacher","request","ask",2,"student_personal_tuition_v1","ordinary_next_level_v1"});
    const auto a=two.preview();const auto b=preview_growth_study_teaching_request(two.read(),two.requests,"other","student","medicine",2);
    check(a.ok&&b.ok,"two actual distinct teacher sessions and real course wallets each resolve");
    auto spliced=a.value;spliced.agreement=b.value.agreement;spliced.student_course=b.value.student_course;
    check(!revalidate_growth_study_teaching_request(two.read(),two.requests,spliced).ok,"two valid sources cannot be combined to authorize the other teacher and wallet");
    std::cout<<"GROWTH_STUDY_TEACHING_REQUESTS "<<checks<<" checks 0 failures; source component only\n";return 0;
}catch(const std::exception& e){std::cerr<<"GROWTH_STUDY_TEACHING_REQUESTS "<<checks<<" checks 1 failure: "<<e.what()<<'\n';return 1;}}
