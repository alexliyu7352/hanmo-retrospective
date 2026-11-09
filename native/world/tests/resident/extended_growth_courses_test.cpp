// 六项新技艺复用正式课程、资源预留和共同人物；本门不冒充完整World银行或正式Main验收。
#include "resident/domains/growth/growth_internal.hpp"
#include "resident/domains/interaction/interaction_codec.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
Integer checks=0;
// Release仍核每项结果；正式规则首错不被测试默认值覆盖。
void check(bool value,const StableId& message){++checks;if(!value)throw std::runtime_error(message);}
// 失败直接保留业务码，不从失败Outcome取空世界或零经验继续。
template<class T>T take(Outcome<T> result){if(!result.ok)throw std::runtime_error(result.error.original.code+":"+result.error.original.path);return std::move(result.value);}
// 所有领域写入与checkpoint边界必须实际成功。
void good(Status result){check(result.ok,result.error.original.code+":"+result.error.original.path);}

// 真实师徒、钱包及岗位接触沿生产接口；没有挑战的基础课程不借未定义小游戏取得经验。
struct Course {
    StableId skill;
    std::shared_ptr<CommonCatalog> shared=std::make_shared<CommonCatalog>();
    CommonStore common{CommonState{},shared};
    GrowthCatalog catalog;GrowthState growth;SocialState social;InteractionState interaction;
    InteractionCatalog interactions;InteractionAccess contact;GrowthAccess access;
    explicit Course(const StableId& id,Integer student_xp=80,Integer teacher_xp=650):skill(id){
        shared->extended_skills=true;
        shared->places.insert("school",{"school","town",{}});
        shared->roles.insert("teacher.role",{"teacher.role","teacher","school",{},{},{"study_skill"},6});
        for(const auto& person:{StableId{"student"},StableId{"teacher"}}){
            ActorRecord actor;actor.id=person;actor.alive=true;actor.available=true;actor.hp=100;
            actor.wallet_id=person+".wallet";actor.inventory_id=person+".bag";actor.birth_tick=-22*720;
            actor.skills.extended=ActorExtendedSkills{};good(growth_set_skill(actor.skills,skill,person=="student"?student_xp:teacher_xp));
            actor.location=SettlementLocation{"town","school"};actor.attributes.administration=60;
            common.state().actors.insert(person,actor);
            common.state().wallets.insert(actor.wallet_id,{actor.wallet_id,{Owner::Kind::actor,person,{}},person=="student"?1000:0,{}});
        }
        common.state().roles.insert("teacher.role",{"teacher",true,{}});
        GrowthCourse course;course.skill_id=skill;course.mode="teacher";course.wallet_id="teacher.wallet";course.settlement_policy="per_lesson_v1";
        GrowthRole role;role.courses.push_back(course);catalog.roles.insert("teacher.role",role);catalog.cultivation=true;catalog.study_segments=true;
        interactions.common=shared;
        contact.resolve_location=[this](const StableId& actor){return take(common.resolved_location(actor));};
        contact.role=[this](const StableId& role_id)->std::optional<RoleAssignmentView>{const auto* role=common.state().roles.find(role_id);return role?std::optional<RoleAssignmentView>({role->actor_id,role->enabled}):std::nullopt;};
        contact.battle_occupied=[](const StableId&){return false;};
        access.contact=[this](const StableId& actor,const InteractionContext& context,const StableId& service,const StableId& own){return validate_interaction(common,interaction,interactions,contact,actor,context,service,own);};
    }
    // 当前经验只从共同人物读取，课程事实只用于证明其贡献。
    Integer xp()const{return growth_skill_xp(common.state().actors.find("student")->skills,skill);}
    // 基础课程请求只指向当面真实岗位；时长与支付均交正式preview。
    GrowthCommand request(Integer ticks=2)const{GrowthCommand command;command.kind="study_skill";command.skill_id=skill;command.ticks=ticks;command.context=InteractionContext{"school","teacher","teacher.role",{},{}};return command;}
    // 模拟共同kernel安装唯一行动头和占用后进入正式成长开课。
    StableId start(Integer ticks=2){
        auto command=request(ticks);auto quote=take(preview_growth(common,growth,catalog,*shared,social,access,"student",command));
        ActionCoreView action;action.id=take(common.next_id("action"));action.kind="study_skill";action.actor_id="student";action.participants={"student","teacher"};action.start_tick=common.tick();action.remaining_ticks=ticks;
        good(common.apply_action({ActionMutation::Kind::insert,action,{}}));
        for(const auto& actor:action.participants){ActorMutation mutation;mutation.actor_id=actor;mutation.action_id=action.id;good(common.apply_actor(mutation));}
        good(start_growth(common,growth,catalog,*shared,social,access,action,quote));good(validate_growth(common,growth,catalog,*shared,access));return action.id;
    }
    // 只释放此课程持有的师徒占用与剩余预留，教师仍保留本人全部能力和钱包。
    void release(const StableId& id){const auto action=*common.action(id);good(common.apply_resources({{ReleaseActionResources{id}}, {}}));for(const auto& person:action.participants){if(common.actor(person)->action_id!=id)continue;ActorMutation mutation;mutation.actor_id=person;mutation.action_id=StableId{};good(common.apply_actor(mutation));}growth.actions.erase(id);good(common.apply_action({ActionMutation::Kind::erase,action,{}}));}
    // 正式半日结果消费一次，世界半日随实际课时前进，完成后核所有永久课时证据。
    GrowthEffect step(const StableId& id){auto result=take(step_growth(common,growth,catalog,*shared,social,interaction,access,*common.action(id)));if(result.completed)release(id);++common.state().identity.tick;good(validate_growth(common,growth,catalog,*shared,access));return result.effect;}
    // 取消由正式即时命令结算已提供部分，不能退回已教课费或重复经验。
    GrowthEffect cancel(const StableId& id){GrowthCommand command;command.kind="study_cancel";command.action_id=id;auto quote=take(preview_growth(common,growth,catalog,*shared,social,access,"student",command));auto effect=take(apply_growth_instant(common,growth,catalog,*shared,social,access,"student",quote));release(id);good(validate_growth(common,growth,catalog,*shared,access));return effect;}
    // 同一候选的共同人物与课程都经typed checkpoint；显式扩展feature保全部六项及课中预留。
    void restore(){
        CodecVisitor::Buffer common_bytes,growth_bytes;CodecVisitor common_writer(common_bytes),growth_writer(growth_bytes);
        common_writer.schema_features=common_checkpoint::extended_actor_skills;
        good(visit_common_checkpoint(common_writer,common.state()));good(common_writer.finish());good(visit_growth_checkpoint(growth_writer,growth));good(growth_writer.finish());
        const auto common_before=encode_common_state(common.state()),growth_before=encode_growth_state(growth);
        CommonState restored_common;GrowthState restored_growth;const auto& common_input=common_bytes;const auto& growth_input=growth_bytes;
        CodecVisitor common_reader(common_input),growth_reader(growth_input);common_reader.schema_features=common_checkpoint::extended_actor_skills;
        good(visit_common_checkpoint(common_reader,restored_common));good(common_reader.finish());good(visit_growth_checkpoint(growth_reader,restored_growth));good(growth_reader.finish());
        check(compare_boundary(common_before,encode_common_state(restored_common)).exact(),"all sixteen skills and resource reservations restore exactly");
        check(compare_boundary(growth_before,encode_growth_state(restored_growth)).exact(),"course receipt and frozen cap restore exactly");
        common=CommonStore(std::move(restored_common),shared);growth=std::move(restored_growth);good(validate_growth(common,growth,catalog,*shared,access));
    }
};

// 每项新技艺都有真实报价、两人占用、逐课成长及中间态恢复，其他技艺不能随课一起增长。
void all_six_courses(){for(const auto* skill:{"naval","espionage","fortification","agriculture","medicine","forging"}){
    Course course(skill);course.common.state().actors.find("student")->fatigue=38;
    const auto skills=take(growth_skills_view(course.common,course.growth,course.catalog,*course.shared,"student",skill));
    check(boundary::array(boundary::field(skills,"skills","view"),"view/skills").size()==16&&boundary::get_string(skills,"selected_skill_id","view")==skill,"view exposes actual sixteen-skill profile");
    auto command=course.request();const auto quote=take(preview_growth(course.common,course.growth,course.catalog,*course.shared,course.social,course.access,"student",command));
    check(quote.cost==60&&quote.duration==2,"new skills use original course fees and duration");
    const auto id=course.start();check(course.xp()==80&&growth::reserved(course.common.wallet("student.wallet"),id)==60,"start reserves fee without immediate XP");
    course.step(id);check(course.xp()==100&&course.common.wallet("student.wallet")->balance==970&&course.common.wallet("teacher.wallet")->balance==30,"first actual lesson pays30 and gains20");
    course.restore();const auto result=course.step(id);check(course.xp()==118&&result.xp_gained==38&&result.fee_paid==60,"fatigue rounding and completion preserve original totals");
    for(const auto& other:growth_skill_ids(true))if(other!=skill)check(growth_skill_xp(course.common.state().actors.find("student")->skills,other)==0,"lesson never awards unrelated skills");
    course.restore();check(course.common.wallet("student.wallet")->balance+course.common.wallet("teacher.wallet")->balance==1000,"cold continuation conserves money");
}}

// 取消及教师失效只保真实首课，冻结教师上限不因保存、重看或结课再次发奖。
void interrupted_and_capped(){
    Course cancelled("medicine");auto id=cancelled.start(3);cancelled.step(id);cancelled.restore();auto result=cancelled.cancel(id);check(cancelled.xp()==100&&result.fee_paid==30&&result.unprovided_released==60&&cancelled.common.wallet("student.wallet")->balance==970,"cancel retains one lesson and releases only unprovided fees");
    Course unavailable("agriculture");id=unavailable.start(3);unavailable.step(id);ActorMutation mutation;mutation.actor_id="teacher";mutation.available=false;good(unavailable.common.apply_actor(mutation));result=unavailable.step(id);check(!result.success&&unavailable.xp()==100&&unavailable.common.wallet("student.wallet")->balance==970&&!unavailable.common.action(id),"unavailable teacher does not finish phantom lessons");
    Course capped("forging",290,300);id=capped.start();capped.step(id);capped.restore();result=capped.step(id);check(capped.xp()==300&&result.xp_gained==10,"teacher cap freezes all new skill credit");
    Course wrong_profile("naval");wrong_profile.shared->extended_skills=false;const auto denied=preview_growth(wrong_profile.common,wrong_profile.growth,wrong_profile.catalog,*wrong_profile.shared,wrong_profile.social,wrong_profile.access,"student",wrong_profile.request());check(!denied.ok&&denied.error.original.code=="InvalidStudyRequest"&&wrong_profile.xp()==80,"old profile cannot start new skill course");
}
}
// 本门只证明新技艺复用成长与共同冷字段；整世界module身份和真实Main留给中央组合节点。
int main(){try{all_six_courses();interrupted_and_capped();std::cout<<"EXTENDED_GROWTH_COURSES "<<checks<<" checks 0 failures\n";return 0;}catch(const std::exception& error){std::cerr<<"EXTENDED_GROWTH_COURSES FAIL "<<error.what()<<" after "<<checks<<" checks\n";return 1;}}
