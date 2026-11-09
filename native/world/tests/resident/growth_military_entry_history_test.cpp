// 新机从固定5566源重建的陆战资格必要门；不冒遗失e2de测试或完整课程/军战World验收。
#include "resident/domains/growth/growth_military_entry_history.hpp"
#include "resident/domains/family/family_birth.hpp"
#include "resident/storage/common_codec.hpp"
#include <iostream>
#include <stdexcept>
#include <type_traits>
using namespace hanmo::resident;
namespace {
Integer checks=0;
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
// typed最小课程只用于纯读边界；调用方完整Growth校验仍是产品前置。
StudyFact lesson(Integer tick,Integer before,Integer gain){StudyFact f;f.actor_id="commander";f.start_tick=tick-1;f.phase="completed";f.completed_tick=tick;f.terms.student_id=f.actor_id;f.terms.skill_id="infantry";f.terms.xp_before=before;f.terms.xp_limit=1200;f.terms.settlement_policy="per_lesson_v1";StudyLesson l;l.xp=gain;l.segment=StudySegment{1,tick,before,10000,gain,false,{},{}};f.lessons.push_back(l);return f;}
using CurrentSignature=Outcome<GrowthMilitaryEntryQualification>(*)(const CommonStore&,const GrowthMilitaryEntryContext&,const GrowthState&,const StableId&,const StableId&,const DynamicStepInput*);
using BeforeSignature=Outcome<GrowthMilitaryEntryQualification>(*)(const CommonStore&,const GrowthMilitaryEntryContext&,const GrowthState&,const StableId&,const StableId&,Integer,const StableId&,const DynamicStepInput*);
static_assert(std::is_same_v<decltype(&read_growth_military_entry_qualification),CurrentSignature>);
static_assert(std::is_same_v<decltype(&read_growth_military_entry_qualification_before_fact),BeforeSignature>);
}
int main(){try{
    auto shared=std::make_shared<CommonCatalog>();shared->actor_ids={"commander"};shared->actor_initial_birth.insert("commander",-14400);
    FamilyCatalog family_catalog;IdentityPrototype original;original.attributes.leadership=73;original.skills.infantry=290;original.skills.archery=100;original.skills.cavalry=650;family_catalog.prototypes.insert("commander",original);
    FamilyState family;FamilyBirthState births;GrowthMilitaryEntryContext context{*shared,family_catalog,family,&births};
    CommonState seed;seed.identity.world_id[0]=1;seed.identity.tick=4;seed.next_instance_id=200;ActorRecord person;person.id="commander";person.attributes=original.attributes;person.skills=original.skills;person.skills.infantry=335;seed.actors.insert(person.id,person);CommonStore common(seed,shared);GrowthState growth;
    auto first=lesson(3,290,10);first.lessons[0].segment->adjustment=StudyPerformanceAdjustment{3,300,15000,5,{}};
    growth.facts.insert("action:80",first);common.state().facts.insert("action:80",{"action:80","skill_study","cultivation"});
    common.state().facts.insert("military_rout_entry:7",{"military_rout_entry:7","military_rout_entry","military"});
    growth.facts.insert("action:10",lesson(4,305,30));common.state().facts.insert("action:10",{"action:10","skill_study","cultivation"});
    const auto untouched=encode_common_state(common.state());
    auto now=read_growth_military_entry_qualification(common,context,growth,"commander","infantry");
    check(now.ok&&now.value.experience==335&&now.value.level==2&&now.value.leadership==73,"current exact XP and immutable leadership");
    check(now.value.causal_fact_ids==std::vector<StableId>{"action:80","action:10"},"actual Common order instead of numeric instance order");
    auto old=read_growth_military_entry_qualification_before_fact(common,context,growth,"commander","infantry",3,"military_rout_entry:7");
    check(old.ok&&old.value.experience==300&&old.value.causal_fact_ids==std::vector<StableId>{"action:80"},"same tick lesson before contact but late answer after");
    check(read_growth_military_entry_qualification(common,context,growth,"commander","archery").value.level==1,"original archery with no invented credit");
    check(read_growth_military_entry_qualification(common,context,growth,"commander","cavalry").value.level==3,"original cavalry");
    check(!read_growth_military_entry_qualification(common,context,growth,"commander","commerce").ok,"unselected skill rejected");
    check(compare_boundary(untouched,encode_common_state(common.state())).exact(),"reader did not write Common");
    auto* actor=common.state().actors.find("commander");actor->attributes.leadership=74;
    check(!read_growth_military_entry_qualification(common,context,growth,"commander","infantry").ok,"today leadership cannot fill an unknown source");actor->attributes.leadership=73;
    actor->skills.infantry=336;check(!read_growth_military_entry_qualification(common,context,growth,"commander","infantry").ok,"unknown XP delta rejected");actor->skills.infantry=335;
    common.state().facts.find("military_rout_entry:7")->domain="military_naval";check(!read_growth_military_entry_qualification_before_fact(common,context,growth,"commander","infantry",3,"military_rout_entry:7").ok,"wrong boundary domain rejected");common.state().facts.find("military_rout_entry:7")->domain="military";
    check(!read_growth_military_entry_qualification_before_fact(common,context,growth,"commander","infantry",3,{}).ok,"empty history boundary rejected");
    common.state().facts.insert("action:190",{"action:190","skill_study","cultivation"});check(!read_growth_military_entry_qualification(common,context,growth,"commander","infantry").ok,"missing permanent study payload rejected");common.state().facts.erase("action:190");
    auto* second=std::get_if<StudyFact>(growth.facts.find("action:10"));second->lessons[0].segment->xp_before=306;
    check(!read_growth_military_entry_qualification(common,context,growth,"commander","infantry").ok,"credit gap rejected");second->lessons[0].segment->xp_before=305;
    CommonState hot_seed=seed;hot_seed.identity.tick=2;hot_seed.actors.find("commander")->skills.infantry=300;CommonStore hot(hot_seed,shared);GrowthState hot_growth;
    hot_growth.facts.insert("action:80",lesson(3,290,10));hot.state().facts.insert("action:80",{"action:80","skill_study","cultivation"});hot.state().facts.insert("military_rout_entry:3",{"military_rout_entry:3","military_rout_entry","military"});
    DynamicStepInput phase;phase.from_tick=2;phase.to_tick=3;auto preclock=read_growth_military_entry_qualification_before_fact(hot,context,hot_growth,"commander","infantry",3,"military_rout_entry:3",&phase);
    check(preclock.ok&&preclock.value.source_identity.tick==2&&preclock.value.observed_tick==3&&preclock.value.experience==300,"strict candidate phase retains identity from and observation to");
    check(!read_growth_military_entry_qualification_before_fact(hot,context,hot_growth,"commander","infantry",3,"military_rout_entry:3").ok,"stable read rejects future entry");
    phase.to_tick=4;check(!read_growth_military_entry_qualification(hot,context,hot_growth,"commander","infantry",&phase).ok,"non-single phase rejected");phase.to_tick=3;
    auto* hot_fact=std::get_if<StudyFact>(hot_growth.facts.find("action:80"));hot_fact->lessons[0].segment->adjustment=StudyPerformanceAdjustment{3,300,15000,5,{}};hot.state().actors.find("commander")->skills.infantry=305;
    check(!read_growth_military_entry_qualification(hot,context,hot_growth,"commander","infantry",&phase).ok,"future zero-time answer cannot already exist in contact phase");
    // 先开的课后结算：最终因果列表按原Common头序，不按净经验结算排序。
    CommonStore reordered(seed,shared);GrowthState reordered_growth;auto later=lesson(4,300,35);later.start_tick=0;auto earlier=lesson(3,290,10);earlier.start_tick=1;
    reordered_growth.facts.insert("action:90",later);reordered_growth.facts.insert("action:20",earlier);reordered.state().facts.insert("action:90",{"action:90","skill_study","cultivation"});reordered.state().facts.insert("action:20",{"action:20","skill_study","cultivation"});reordered.state().facts.insert("military_rout_entry:5",{"military_rout_entry:5","military_rout_entry","military"});
    auto ordered=read_growth_military_entry_qualification_before_fact(reordered,context,reordered_growth,"commander","infantry",4,"military_rout_entry:5");check(ordered.ok&&ordered.value.causal_fact_ids==std::vector<StableId>{"action:90","action:20"},"Common source order is independent from lesson settlement order");
    // 本人创建必须取原草案；原型与今日实例的巧合不能替代草案来源。
    ActorRecord created;created.id="actor:150";created.prototype_id="prototype";created.created_tick=0;created.attributes.leadership=55;created.skills.infantry=400;common.state().actors.insert(created.id,created);
    IdentityPrototype prototype;prototype.attributes.leadership=22;family_catalog.prototypes.insert("prototype",prototype);CharacterCreationFact creation;creation.actor_id=created.id;creation.prototype_actor_id="prototype";creation.scenario_id=common.state().metadata.scenario_id;creation.draft.attributes.leadership=55;creation.draft.skills.infantry=400;
    family.facts.insert("family:149",creation);family.facts.insert(created.id,IdentityCreationFact{"prototype",created.id,0,-14400,"Created","family:149",{}});common.state().facts.insert("family:149",{"family:149","family_character_creation","family"});common.state().facts.insert(created.id,{created.id,"actor_creation","family"});
    auto made=read_growth_military_entry_qualification(common,context,growth,created.id,"infantry");check(made.ok&&made.value.experience==400&&made.value.leadership==55&&made.value.authored_actor_id.empty()&&made.value.causal_fact_ids==std::vector<StableId>{"family:149",created.id},"actual personal draft overrides original prototype");
    common.state().actors.find(created.id)->attributes.leadership=22;check(!read_growth_military_entry_qualification(common,context,growth,created.id,"infantry").ok,"prototype cannot replace original personal draft");
    // 出生初始技能只取实际出生所用原型；完整Birth/Family验证由调用World先完成。
    ActorRecord born;born.id="actor:180";born.prototype_id="prototype";born.created_tick=1;born.attributes.leadership=22;common.state().actors.insert(born.id,born);FamilyBirthRecord birth;birth.actor_id=born.id;birth.prototype_id="prototype";birth.tick=1;births.births.insert("family_birth:179",birth);common.state().facts.insert("family_birth:179",{"family_birth:179","family_birth_recorded","family_birth"});common.state().facts.insert(born.id,{born.id,"actor_creation","family_birth"});
    auto newborn=read_growth_military_entry_qualification(common,context,growth,born.id,"infantry");check(newborn.ok&&newborn.value.experience==0&&newborn.value.leadership==22,"actual birth original prototype");
    GrowthMilitaryEntryContext missing_birth{*shared,family_catalog,family,nullptr};check(!read_growth_military_entry_qualification(common,missing_birth,growth,born.id,"infantry").ok,"dynamic birth has no static fallback");
    births.births.find("family_birth:179")->tick=2;check(!read_growth_military_entry_qualification(common,context,growth,born.id,"infantry").ok,"changed birth date rejected");
    std::cout<<"GROWTH_MILITARY_ENTRY_HISTORY "<<checks<<" checks 0 failures\n";return 0;
}catch(const std::exception& e){std::cerr<<"GROWTH_MILITARY_ENTRY_HISTORY "<<checks<<" checks 1 failure: "<<e.what()<<'\n';return 1;}}
