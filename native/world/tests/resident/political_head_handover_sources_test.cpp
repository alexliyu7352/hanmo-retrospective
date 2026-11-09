// 生前交接首纯源门只核作者/真实Office前界；typed事实不是已完成让职或World运行证据。
#include "resident/domains/politics/head_handover/political_head_handover_contract.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
Integer checks=0;
// 坏源失败必须来自具名边界，不把尚未执行的源码断言称为已运行通过。
void check(bool value,const char* label){++checks;if(!value)throw std::runtime_error(label);}
// 所有测试事实由原Common分配，不手工猜测共用计数器。
template<class T>T take(Outcome<T> result){if(!result.ok)throw std::runtime_error(result.error.original.code);return std::move(result.value);}
// 独立零资产原首职，原死亡继位和新生前让职分别配置专项Grant。
struct Fixture {
 std::shared_ptr<CommonCatalog> cc=std::make_shared<CommonCatalog>();CommonStore common{CommonState{},cc};OrganizationCatalog org;OrganizationState state;PoliticalSuccessionCatalog succession;PoliticalHeadHandoverCatalog handover;
 Fixture(){cc->places.insert("hall",{"hall","town",{}});for(const auto* id:{"leader","candidate"}){ActorRecord a;a.id=id;a.alive=a.available=true;common.state().actors.insert(id,a);cc->actor_ids.push_back(id);}OrganizationRecord o;o.id="city";o.kind="political";o.headquarters_site="hall";o.succession_policy_id="succession";o.office_ids={"chief"};org.enabled=true;org.organizations.insert(o.id,o);state.organizations=org.organizations;OfficeState office;office.id="chief";office.organization_id=o.id;office.holder_actor_id="leader";office.grant_ids={"nominate","handover"};org.offices.insert(office.id,office);state.offices=org.offices;Grant g;g.id="nominate";g.office_id=office.id;g.allowed_command_kinds={political_succession_nominate_command,political_succession_withdraw_command};g.scopes={{ScopeRef::Kind::site,"hall"}};org.grants.insert(g.id,g);g.id="handover";g.allowed_command_kinds={political_head_handover_offer_command,political_head_handover_withdraw_command};org.grants.insert(g.id,g);succession.policies.insert("succession",{"succession","city","chief","hall","nominate",4,8,"capital"});handover.policies.insert("handover",{"handover","succession","handover",2});}
 // 实际当前任职变化同时保原组织载荷和Common头，后续薄读仍复核两种顺序。
 StableId change(){auto id=take(common.next_id("organization"));take(common.append_fact({id,"organization_office_changed","organization"}));OrganizationFact f;f.id=id;f.kind="organization_office_changed";f.organization_id="city";f.office_id="chief";f.actor_id="candidate";f.tick=common.tick();state.facts.insert(id,f);auto* office=state.offices.find("chief");++office->epoch;office->holder_actor_id="candidate";return id;}
 StableId boundary(){auto id=take(common.next_id("political_head_handover_offer"));take(common.append_fact({id,"political_head_handover_offered",political_head_handover_domain}));return id;}
};
}
int main(){try{
 Fixture f;check(validate_political_head_handover_catalog(f.common,*f.cc,f.org,f.succession,f.handover).ok,"new explicit zero-budget Grant is accepted");auto encoded=encode_political_head_handover_catalog(f.handover);check(compare_boundary(encoded,encode_political_head_handover_catalog(take(decode_political_head_handover_catalog(encoded)))).exact(),"new catalog has strict separate roundtrip");
 auto bad=f.handover;bad.policies.find("handover")->handover_grant_id="nominate";check(!validate_political_head_handover_catalog(f.common,*f.cc,f.org,f.succession,bad).ok,"old nomination Grant cannot sign live handover");bad=f.handover;bad.policies.find("handover")->response_ticks=2147483648LL;check(!validate_political_head_handover_catalog(f.common,*f.cc,f.org,f.succession,bad).ok,"finite response window is bounded");
 f.org.grants.find("handover")->wallet_ids={"public"};check(!validate_political_head_handover_catalog(f.common,*f.cc,f.org,f.succession,f.handover).ok,"handover authority carries no budget source");f.org.grants.find("handover")->wallet_ids.clear();
 const auto boundary=f.boundary();const auto change=f.change();const auto& policy=*f.succession.policies.find("succession");auto source=take(read_political_head_handover_office_before(f.common,f.state,f.org,policy,0,boundary));check(source.holder_actor_id==std::optional<StableId>{"leader"}&&source.office_epoch==0&&source.before_fact_id==boundary,"later same-tick replacement does not rewrite original signer");
 check(!read_political_succession_office_before(f.common,f.state,f.org,policy,0,boundary).ok,"old death succession boundary whitelist is unchanged");check(!read_political_head_handover_office_before(f.common,f.state,f.org,policy,0,change).ok,"arbitrary organization fact cannot become handover boundary");check(!read_political_head_handover_office_before(f.common,f.state,f.org,policy,1,boundary).ok,"future observation tick is denied");
 const auto order=f.common.state().facts;f.common.state().facts={};f.common.state().facts.insert(change,*order.find(change));f.common.state().facts.insert(boundary,*order.find(boundary));check(!read_political_head_handover_office_before(f.common,f.state,f.org,policy,0,boundary).ok,"same-tick reordered Office header cannot cross allocation boundary");f.common.state().facts=order;
 f.common.state().facts.find(boundary)->domain="political_successions";check(!political_head_handover_fact_sequence(f.common,boundary),"new head requires exact new domain");
 std::cout<<checks<<" source checks; no World claim\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
