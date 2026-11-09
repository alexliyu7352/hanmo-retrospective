// 原Common与typed课程的定点边界门；完整Growth/Family先验和实际Personal2入场另由World闭合。
#include "resident/domains/growth/growth_personal_entry_history.hpp"
#include "resident/domains/family/family_birth.hpp"
#include "resident/storage/common_codec.hpp"
#include <iostream>
#include <stdexcept>
#include <type_traits>
using namespace hanmo::resident;
namespace {
Integer checks=0;
void check(bool x,const char* message){++checks;if(!x)throw std::runtime_error(message);}
StudyFact lesson(const StableId& skill,Integer tick,Integer before,Integer gain){
    StudyFact f;f.actor_id="fighter";f.start_tick=tick-1;f.phase="completed";f.completed_tick=tick;f.terms.student_id=f.actor_id;f.terms.skill_id=skill;f.terms.xp_before=before;f.terms.xp_limit=1200;f.terms.settlement_policy="per_lesson_v1";
    StudyLesson l;l.xp=gain;l.segment=StudySegment{1,tick,before,10000,gain,false,{},{}};f.lessons.push_back(l);return f;
}
struct Fixture {
    std::shared_ptr<CommonCatalog> shared=std::make_shared<CommonCatalog>();FamilyCatalog catalog;FamilyState family;FamilyBirthState births;CommonStore common;GrowthState growth;
    Fixture():common(seed(),shared){shared->actor_ids={"fighter"};shared->actor_initial_birth.insert("fighter",-14400);IdentityPrototype p;p.attributes.martial=81;p.skills.martial=290;p.skills.archery=100;catalog.prototypes.insert("fighter",p);}
    static CommonState seed(){CommonState s;s.identity.world_id[0]=1;s.identity.tick=3;s.next_instance_id=200;ActorRecord p;p.id="fighter";p.attributes.martial=81;p.skills.martial=290;p.skills.archery=100;s.actors.insert("fighter",p);return s;}
    GrowthPersonalEntryContext context()const{return {*shared,catalog,family,&births};}
    void entry(const StableId& id="personal_entry:90"){common.state().facts.insert(id,{id,"personal_combat_entered","personal"});}
    void study(const StableId& id,StudyFact f){growth.facts.insert(id,std::move(f));common.state().facts.insert(id,{id,"skill_study","cultivation"});}
    auto now(){return read_growth_personal_entry_qualification(common,context(),growth,"fighter");}
    auto before(Integer tick=3,const StableId& id="personal_entry:90"){return read_growth_personal_entry_qualification_before_fact(common,context(),growth,"fighter",tick,id);}
};
using Current=Outcome<GrowthPersonalEntryQualification>(*)(const CommonStore&,const GrowthPersonalEntryContext&,const GrowthState&,const StableId&);
using Before=Outcome<GrowthPersonalEntryQualification>(*)(const CommonStore&,const GrowthPersonalEntryContext&,const GrowthState&,const StableId&,Integer,const StableId&);
static_assert(std::is_same_v<decltype(&read_growth_personal_entry_qualification),Current>);
static_assert(std::is_same_v<decltype(&read_growth_personal_entry_qualification_before_fact),Before>);
}
int main(){try{
    Fixture f;f.common.state().identity.tick=0;f.entry();auto initial=f.before(0);
    check(initial.ok&&initial.value.base_martial==81&&initial.value.martial_experience==290&&initial.value.archery_level==1,"tick zero actual original qualification");
    check(initial.value.authored_actor_id=="fighter"&&initial.value.causal_fact_ids.empty(),"original catalog identity has no invented fact");
    check(f.now().ok&&f.now().value.before_fact_id.empty(),"stable current no arbitrary future phase");
    check(!f.before(1).ok,"future entry rejected");check(!f.before(0,{}).ok,"empty historical boundary rejected");
    f.common.state().facts.find("personal_entry:90")->domain="military";check(!f.before(0).ok,"wrong domain rejected");f.common.state().facts.find("personal_entry:90")->domain="personal";
    f.common.state().actors.find("fighter")->attributes.martial=82;check(!f.now().ok,"today martial cannot substitute unknown attribute growth");f.common.state().actors.find("fighter")->attributes.martial=81;
    f.common.state().actors.find("fighter")->skills.archery=101;check(!f.now().ok,"unknown archery growth rejected");f.common.state().actors.find("fighter")->skills.archery=100;
    Fixture auto_credit;auto_credit.study("action:10",lesson("martial",3,290,20));auto_credit.study("action:20",lesson("archery",3,100,15));auto_credit.entry();auto_credit.common.state().actors.find("fighter")->skills.martial=310;auto_credit.common.state().actors.find("fighter")->skills.archery=115;
    auto snapshot=encode_common_state(auto_credit.common.state());auto earned=auto_credit.before();check(earned.ok&&earned.value.martial_experience==310&&earned.value.martial_level==2&&earned.value.archery_experience==115,"automatic settled same tick included before zero-time entry");
    check(earned.value.causal_fact_ids==std::vector<StableId>{"action:10","action:20"},"unique union in actual Common order");check(compare_boundary(snapshot,encode_common_state(auto_credit.common.state())).exact(),"pure reader leaves all Common bytes unchanged");
    auto* credit=std::get_if<StudyFact>(auto_credit.growth.facts.find("action:10"));credit->lessons[0].segment->adjustment=StudyPerformanceAdjustment{3,310,15000,10,{}};auto_credit.common.state().actors.find("fighter")->skills.martial=320;
    check(!auto_credit.before().ok&&auto_credit.before().error.original.code=="GrowthPersonalEntryZeroTimeHistoryUnavailable","same tick late answer cannot be placed before entry");check(!auto_credit.now().ok,"current has same strict same-tick qualification rule");
    auto_credit.common.state().identity.tick=4;check(auto_credit.now().ok&&auto_credit.now().value.martial_experience==320,"next tick permits fully sourced late answer");check(!auto_credit.before().ok,"later current does not rewrite ambiguous old entry");auto_credit.entry("personal_entry:100");check(auto_credit.before(4,"personal_entry:100").ok,"later actual boundary can consume old settled answer");
    Fixture cancelled;auto c=lesson("martial",3,290,20);c.terms.settlement_policy.reset();c.lessons[0].segment.reset();c.code="StudyCancelled";cancelled.study("action:10",c);cancelled.entry();cancelled.common.state().actors.find("fighter")->skills.martial=310;
    check(!cancelled.now().ok&&!cancelled.before().ok,"legacy cancellation positive XP is zero-time ambiguous");
    Fixture pending;auto p=lesson("martial",2,290,20);p.phase="started";p.completed_tick=-1;p.terms.settlement_policy.reset();p.lessons[0].segment.reset();pending.study("action:10",p);
    check(!pending.now().ok&&pending.now().error.original.code=="GrowthPersonalEntryUnsettledStudy","pending legacy cancellation cannot invalidate a previously allowed entry");
    Fixture delayed;auto d=lesson("martial",3,290,20);d.phase="started";d.completed_tick=-1;d.lessons[0].segment->performance_pending=true;delayed.study("action:10",d);delayed.common.state().actors.find("fighter")->skills.martial=310;
    check(!delayed.now().ok&&delayed.now().error.original.code=="GrowthPersonalEntryUnsettledStudy","outstanding actual lesson correction blocks admission");
    Fixture empty_cancel;empty_cancel.entry();auto zero=lesson("martial",3,290,0);zero.start_tick=3;zero.terms.settlement_policy.reset();zero.lessons.clear();zero.code="StudyCancelled";empty_cancel.study("action:100",zero);
    check(empty_cancel.before().ok&&empty_cancel.before().value.causal_fact_ids.empty(),"later same-tick zero-gain cancellation preserves legitimate old entry");
    Fixture zero_adjust;auto za=lesson("martial",2,290,20);za.lessons[0].segment->adjustment=StudyPerformanceAdjustment{3,310,10000,0,{}};zero_adjust.study("action:10",za);zero_adjust.entry();zero_adjust.common.state().actors.find("fighter")->skills.martial=310;check(zero_adjust.before().ok,"zero correction contributes no ambiguous XP");
    Fixture wrong_order;wrong_order.entry();wrong_order.study("action:10",lesson("martial",3,290,20));wrong_order.common.state().actors.find("fighter")->skills.martial=310;check(!wrong_order.before().ok,"credit start cannot move after actual entry");
    Fixture wrong_id;wrong_id.study("action:100",lesson("martial",3,290,20));wrong_id.entry();wrong_id.common.state().actors.find("fighter")->skills.martial=310;check(!wrong_id.before().ok,"Common order cannot hide later globally allocated study head");
    Fixture missing;missing.common.state().facts.insert("action:10",{"action:10","skill_study","cultivation"});check(!missing.now().ok,"missing permanent study payload rejected");
    Fixture dynamic;IdentityPrototype prototype;prototype.attributes.martial=11;dynamic.catalog.prototypes.insert("prototype",prototype);ActorRecord person;person.id="actor:20";person.prototype_id="prototype";person.created_tick=3;person.attributes.martial=67;person.skills.martial=400;person.skills.archery=250;dynamic.common.state().actors.insert("actor:20",person);
    CharacterCreationFact made;made.actor_id=person.id;made.prototype_actor_id="prototype";made.created_tick=3;made.scenario_id=dynamic.common.state().metadata.scenario_id;made.draft.attributes.martial=67;made.draft.skills=person.skills;dynamic.family.facts.insert("family:19",made);dynamic.family.facts.insert(person.id,IdentityCreationFact{"prototype",person.id,3,-14400,"Created","family:19",{}});dynamic.common.state().facts.insert("family:19",{"family:19","family_character_creation","family"});dynamic.common.state().facts.insert(person.id,{person.id,"actor_creation","family"});dynamic.entry();
    auto created=read_growth_personal_entry_qualification_before_fact(dynamic.common,dynamic.context(),dynamic.growth,person.id,3,"personal_entry:90");check(created.ok&&created.value.base_martial==67&&created.value.martial_experience==400&&created.value.authored_actor_id.empty(),"same-tick real personal creation prior to entry uses actual draft");check(created.value.causal_fact_ids==std::vector<StableId>{"family:19","actor:20"},"creation original causes retained");dynamic.common.state().actors.find(person.id)->attributes.martial=11;check(!read_growth_personal_entry_qualification(dynamic.common,dynamic.context(),dynamic.growth,person.id).ok,"prototype martial cannot replace actual draft");
    Fixture born;IdentityPrototype baby;baby.attributes.martial=55;born.catalog.prototypes.insert("child",baby);ActorRecord child;child.id="actor:30";child.prototype_id="child";child.created_tick=1;child.attributes.martial=55;born.common.state().actors.insert(child.id,child);FamilyBirthRecord birth;birth.actor_id=child.id;birth.prototype_id="child";birth.tick=1;born.births.births.insert("family_birth:29",birth);born.common.state().facts.insert("family_birth:29",{"family_birth:29","family_birth_recorded","family_birth"});born.common.state().facts.insert(child.id,{child.id,"actor_creation","family_birth"});born.entry();auto b=read_growth_personal_entry_qualification_before_fact(born.common,born.context(),born.growth,child.id,3,"personal_entry:90");check(b.ok&&b.value.base_martial==55&&b.value.martial_experience==0,"birth source only establishes actual skills, not adult combat eligibility");auto absent=born.context();absent.births=nullptr;check(!read_growth_personal_entry_qualification(born.common,absent,born.growth,child.id).ok,"missing birth source has no author fallback");
    std::cout<<"GROWTH_PERSONAL_ENTRY_HISTORY "<<checks<<" checks 0 failures\n";return 0;
}catch(const std::exception& e){std::cerr<<"GROWTH_PERSONAL_ENTRY_HISTORY "<<checks<<" checks 1 failure: "<<e.what()<<'\n';return 1;}}
