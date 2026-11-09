// 师承实际请求绑定与裁剪攻防；只构造本域原事件/缓存外壳，不冒真实会面或全 World。
#include "resident/domains/family/apprenticeship_receipts.hpp"
#include "resident/kernel/command_types.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
int checks=0;void check(bool value,const char* text){++checks;if(!value)throw std::runtime_error(text);}
struct Fixture {
    std::shared_ptr<CommonCatalog> catalog=std::make_shared<CommonCatalog>();CommonStore c;FamilyApprenticeshipState state;
    Fixture():c(CommonState{},catalog){c.state().identity.tick=5;c.state().identity.revision=2;c.state().next_instance_id=100;ActorRecord a;a.id="student";c.state().actors.insert(a.id,a);c.state().request_sequences.insert(a.id,0);}
    void proposed(const StableId& id,Integer revision){ApprenticeshipProposal p;auto& s=p.source;s.statement_id="original";s.speaker_actor_id=s.teacher_actor_id="teacher";s.listener_actor_id=s.student_actor_id="student";s.skill_id="commerce";s.dialogue_id="dialogue";s.node_id="say";s.node_kind="say";s.session_id="interaction:1";s.scenario_id="original";s.world_id[0]=1;s.context={"home","teacher","role"};s.tick=5;s.revision=revision;state.proposals.insert(id,p);c.state().facts.insert(id,{id,p.kind,"family_apprenticeship"});}
    std::shared_ptr<CommitResult> result(const StableId& id,const StableId& fact,Integer sequence){FamilyEffect effect;effect.success=true;effect.actor_id=effect.player_actor_id="student";effect.fact_id=fact;auto out=std::make_shared<CommitResult>();out->kind="family_apprenticeship_receive";out->result_id=id;out->tick=5;out->revision=c.revision();out->details=std::make_shared<const ReceiptDetails>(ReceiptDetails{effect,{}});c.state().request_sequences.set("student",sequence);c.state().results.set("student:"+std::to_string(sequence),out);c.state().history.push_back({5,out->kind,"student",id,{}});return out;}
};
}
int main(){try{
    Fixture f;const auto before=f.state;f.proposed("apprenticeship:3",1);auto first=f.result("result:4","apprenticeship:3",1);
    check(!validate_apprenticeship_result_bindings(f.c,f.state).ok,"unbound new event cannot publish");check(bind_apprenticeship_result_in_candidate(f.c,before,f.state,"student",1,*first).ok,"exact actual cache binds new received event");check(read_apprenticeship_source_for_result(f.c,f.state,*first).value=="apprenticeship:3","original actual result reads source");
    auto copied=*first;check(!read_apprenticeship_source_for_result(f.c,f.state,copied).ok,"same-value copied result is not actual cached request");
    f.c.state().history.clear();f.c.state().identity.revision=3;auto later=f.result("result:8","apprenticeship:3",2);check(!read_apprenticeship_source_for_result(f.c,f.state,*later).ok,"first retained same-tick history cannot relabel old proposal");check(read_apprenticeship_source_for_result(f.c,f.state,*first).ok,"later invalid relabel leaves original binding valid");
    auto corrupt=f.state;corrupt.commits.find("apprenticeship:3")->result_id="result:8";check(!validate_apprenticeship_result_bindings(f.c,corrupt).ok,"result ID transplant rejected");
    f.c.state().results={};f.c.state().history.clear();check(validate_apprenticeship_result_bindings(f.c,f.state).ok,"normal bounded caches may expire without deleting original relation binding");
    Fixture late;const auto old=late.state;late.proposed("apprenticeship:5",1);auto too_early=late.result("result:4","apprenticeship:5",1);check(!bind_apprenticeship_result_in_candidate(late.c,old,late.state,"student",1,*too_early).ok,"late impossible allocation order rejected");check(late.state.commits.empty(),"late failure retracts all new binding fields");
    Fixture edited;auto empty=edited.state;edited.proposed("apprenticeship:2",1);auto original=edited.result("result:3","apprenticeship:2",1);check(bind_apprenticeship_result_in_candidate(edited.c,empty,edited.state,"student",1,*original).ok,"second independent fixture exact result");const auto previous=edited.state;edited.c.state().identity.revision=3;edited.state.proposals.find("apprenticeship:2")->source.skill_id="naval";auto unrelated=edited.result("result:7","apprenticeship:2",2);check(!bind_apprenticeship_result_in_candidate(edited.c,previous,edited.state,"student",2,*unrelated).ok,"candidate cannot alter prior immutable proposal");
    Fixture prebound;const auto no_source=prebound.state;prebound.proposed("apprenticeship:2",1);auto value=prebound.result("result:3","apprenticeship:2",1);prebound.state.commits.insert("apprenticeship:2",{"result:3","student","family_apprenticeship_receive",1,2,{}});check(!bind_apprenticeship_result_in_candidate(prebound.c,no_source,prebound.state,"student",1,*value).ok,"caller-supplied prebinding is rejected without exception");
    auto json=encode_family_apprenticeship(f.state);auto roundtrip=decode_family_apprenticeship(json);check(roundtrip.ok&&validate_apprenticeship_result_bindings(f.c,roundtrip.value).ok,"permanent request binding survives exact JSON");check(f.c.state().next_instance_id==100,"binding creates no extra Common ID or assets");
    std::cout<<"APPRENTICESHIP_RECEIPTS "<<checks<<" checks 0 failures\n";return 0;
}catch(const std::exception& e){std::cerr<<"APPRENTICESHIP_RECEIPTS "<<checks<<" checks 1 failure: "<<e.what()<<'\n';return 1;}}
