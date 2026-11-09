// 真实成长报价、授课与卡片自动登记的组合门；需同一新GrowthState ABI库，不能链接旧archive。
#include "resident/domains/growth/growth_internal.hpp"
#include "resident/domains/personal/personal_model.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
Integer checks=0;
// 所有断言保留正式规则错误，失败结果不能被读成默认成功。
void check(bool value,const StableId& label){++checks;if(!value)throw std::runtime_error(label);}
template<class T>T take(Outcome<T> result){if(!result.ok)throw std::runtime_error(result.error.original.code+":"+result.error.original.path);return std::move(result.value);}
void good(Status result){check(result.ok,result.error.original.code+":"+result.error.original.path);}
// 正式共同人物、岗位、个人本领与接触接口，不直接构造完成的TrainingFact。
struct Lesson {
    std::shared_ptr<CommonCatalog> shared=std::make_shared<CommonCatalog>();CommonStore common{CommonState{},shared};
    GrowthCatalog catalog;GrowthState growth;PersonalSkill ability;SocialState social;InteractionState interaction;
    InteractionCatalog contacts;InteractionAccess contact;GrowthAccess access;
    explicit Lesson(bool teaching=false){
        shared->extended_skills=true;shared->places.insert("school",{"school","town",{}});common.state().metadata.scenario_id="cards.lesson";common.state().metadata.player_actor_id=teaching?"teacher":"student";
        ability.id="ability.heart";ability.kind="heart";ability.practice_ticks=20;ability.required_skill_level=PersonalSkillRequirement{"medicine",1,{}};
        shared->personal_skill_kinds.insert(ability.id,ability.kind);
        for(const auto* id:{"student","teacher"}){ActorRecord person;person.id=id;person.alive=true;person.available=true;person.hp=100;person.birth_tick=-25*720;person.location=SettlementLocation{"town","school"};person.wallet_id=person.id+".wallet";person.inventory_id=person.id+".bag";person.skills.extended.emplace();person.skills.extended->medicine=100;
            if(person.id=="teacher")person.combat.learned.insert(ability.id,100);common.state().actors.insert(person.id,person);common.state().wallets.insert(person.wallet_id,{person.wallet_id,{Owner::Kind::actor,person.id,{}},100,{}});
            RoleDefinition role;role.id=person.id+".role";role.initial_actor=person.id;role.place_id="school";role.services={"learn_skill"};shared->roles.insert(role.id,role);common.state().roles.insert(role.id,{person.id,true,{}});
        }
        GrowthRole role;role.lessons.push_back({ability.id,"teacher.wallet",0,2,20});catalog.roles.insert("teacher.role",role);catalog.cultivation=true;
        GrowthCardDefinition definition;definition.id="card.heart";definition.personal_skill_id=ability.id;definition.category=GrowthCardCategory::heart;definition.name_key="card.name";definition.description_key="card.description";definition.hint_key="card.hint";
        shared->texts.insert("card.name","养气");shared->texts.insert("card.description","静心运转");shared->texts.insert("card.hint","向师傅求学");catalog.cards.enabled=true;catalog.cards.definitions.insert(definition.id,definition);catalog.cards.initial_grants.push_back({"teacher",definition.id,{}});
        good(validate_growth_card_catalog(catalog.cards,*shared));good(initialize_growth_card_state(common.state(),growth,catalog));contacts.common=shared;
        contact.resolve_location=[this](const StableId& person){return take(common.resolved_location(person));};contact.role=[this](const StableId& id)->std::optional<RoleAssignmentView>{auto* value=common.state().roles.find(id);return value?std::optional<RoleAssignmentView>{{value->actor_id,value->enabled}}:std::nullopt;};contact.battle_occupied=[](const auto&){return false;};
        access.contact=[this](const StableId& actor,const InteractionContext& context,const StableId& service,const StableId& own){return validate_interaction(common,interaction,contacts,contact,actor,context,service,own);};access.battle_occupied=[](const auto&){return false;};
        access.personal_skill=[this](const StableId& id)->std::optional<GrowthPersonalSkill>{return id==ability.id?std::optional<GrowthPersonalSkill>{{ability.kind,ability.practice_ticks}}:std::nullopt;};access.skill_eligible=[this](const ActorRecord& actor,const StableId&){return personal_skill_eligible(actor,ability);};
    }
    // 行动头和双方占用按共同kernel合同安装，然后交正式领域开课和收费。
    StableId start(const StableId& kind){GrowthCommand command;command.kind=kind;command.skill_id=ability.id;command.ticks=1;const StableId actor=kind=="teach_skill"?"teacher":"student";if(kind!="practice_skill")command.context=InteractionContext{"school",kind=="teach_skill"?"student":"teacher",kind=="teach_skill"?"student.role":"teacher.role",{}, {}};
        auto quote=take(preview_growth(common,growth,catalog,*shared,social,access,actor,command));ActionCoreView action;action.id=take(common.next_id("action"));action.kind=kind;action.actor_id=actor;action.start_tick=common.tick();action.remaining_ticks=quote.duration;action.participants=kind=="practice_skill"?std::vector<StableId>{actor}:std::vector<StableId>{actor,kind=="teach_skill"?"student":"teacher"};good(common.apply_action({ActionMutation::Kind::insert,action,{}}));for(const auto& id:action.participants){ActorMutation mutation;mutation.actor_id=id;mutation.action_id=action.id;good(common.apply_actor(mutation));}good(start_growth(common,growth,catalog,*shared,social,access,action,quote));good(validate_growth(common,growth,catalog,*shared,access));return action.id;
    }
    // 正式半日结算后再释放占用；卡片必须由step_growth自动记录，而非测试主动补发。
    GrowthEffect step(const StableId& id){const auto action=*common.action(id);auto result=take(step_growth(common,growth,catalog,*shared,social,interaction,access,action));if(result.completed){for(const auto& actor:action.participants){ActorMutation mutation;mutation.actor_id=actor;mutation.action_id="";good(common.apply_actor(mutation));}growth.actions.erase(id);good(common.apply_action({ActionMutation::Kind::erase,action,{}}));}++common.state().identity.tick;good(validate_growth(common,growth,catalog,*shared,access));return result.effect;}
    // 正式共同人物与新growth块同步中途冷读，不重放初态取得或教学费用。
    void cold(){CodecVisitor::Buffer common_bytes,growth_bytes;CodecVisitor cw(common_bytes),gw(growth_bytes);cw.schema_features=common_checkpoint::extended_actor_skills;good(visit_common_checkpoint(cw,common.state()));good(cw.finish());good(visit_growth_checkpoint(gw,growth,false,true));good(gw.finish());CommonState recovered_common;GrowthState recovered_growth;const auto& ci=common_bytes;const auto& gi=growth_bytes;CodecVisitor cr(ci),gr(gi);cr.schema_features=common_checkpoint::extended_actor_skills;good(visit_common_checkpoint(cr,recovered_common));good(cr.finish());good(visit_growth_checkpoint(gr,recovered_growth,false,true));good(gr.finish());common=CommonStore(std::move(recovered_common),shared);growth=std::move(recovered_growth);good(validate_growth(common,growth,catalog,*shared,access));}
    bool student_card()const{const auto* row=growth.cards->owned.find("student");return row&&row->find("card.heart");}
};
// 真正完成后取得并可配置，练习和存读保留原首次来源、不重复扣课程钱。
void learning_and_configuration(){Lesson lesson;auto id=lesson.start("learn_skill");lesson.step(id);check(!lesson.student_card(),"unfinished real lesson has no card");lesson.cold();auto effect=lesson.step(id);check(effect.success&&lesson.student_card(),"real completed lesson automatically records card");check(lesson.common.wallet("student.wallet")->balance==80&&lesson.common.wallet("teacher.wallet")->balance==120,"card hook does not duplicate actual learning fee");check(lesson.growth.cards->owned.find("student")->find("card.heart")->source_id==id,"acquisition points to actual completed action");
    GrowthCommand command;command.kind="configure_heart";command.skill_id=lesson.ability.id;auto quote=take(preview_growth(lesson.common,lesson.growth,lesson.catalog,*lesson.shared,lesson.social,lesson.access,"student",command));take(apply_growth_instant(lesson.common,lesson.growth,lesson.catalog,*lesson.shared,lesson.social,lesson.access,"student",quote));check(lesson.common.state().actors.find("student")->combat.heart_id==lesson.ability.id,"original heart configuration uses actual learned ability");
    const auto practice=lesson.start("practice_skill");lesson.step(practice);check(lesson.growth.cards->owned.find("student")->find("card.heart")->source_id==id,"practice does not replace or duplicate first source");lesson.cold();check(lesson.student_card()&&lesson.common.state().actors.find("student")->combat.heart_id==lesson.ability.id,"cold continuation preserves card and actual heart configuration");
}
// 教师失效不发卡，真正传授给学生仍登记学生的来源；现有六艺前置保持有效。
void interruptions_and_teaching(){Lesson interrupted;const auto id=interrupted.start("learn_skill");interrupted.step(id);ActorMutation mutation;mutation.actor_id="teacher";mutation.available=false;good(interrupted.common.apply_actor(mutation));auto effect=interrupted.step(id);check(!effect.success&&!interrupted.student_card(),"interrupted real learning cannot award card");
    Lesson teaching(true);auto lesson=teaching.start("teach_skill");teaching.step(lesson);teaching.cold();teaching.step(lesson);check(teaching.student_card()&&teaching.growth.cards->owned.find("student")->find("card.heart")->source_id==lesson,"real teaching records student acquisition");
    Lesson unqualified;unqualified.common.state().actors.find("student")->skills.extended->medicine=0;GrowthCommand command;command.kind="learn_skill";command.skill_id=unqualified.ability.id;command.context=InteractionContext{"school","teacher","teacher.role",{}, {}};auto quote=preview_growth(unqualified.common,unqualified.growth,unqualified.catalog,*unqualified.shared,unqualified.social,unqualified.access,"student",command);check(!quote.ok&&!unqualified.student_card()&&unqualified.common.wallet("student.wallet")->balance==100,"card source cannot bypass actual six-skill ability prerequisite");
}
}
// 完整执行必须由同源新库提供生产依赖；本文件单独语法通过不等于授课门已运行。
int main(){try{learning_and_configuration();interruptions_and_teaching();std::cout<<"GROWTH_CARDS_LEARNING "<<checks<<" checks 0 failures\n";return 0;}catch(const std::exception& error){std::cerr<<"GROWTH_CARDS_LEARNING FAIL "<<error.what()<<" after "<<checks<<" checks\n";return 1;}}
