// 继位原首职纯来源门；typed组织变更夹具只测前界，不冒正式继位或自然死亡运行。
#include "resident/domains/politics/succession/political_succession_contract.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
Integer checks=0;
// 每个坏源断言具名保留，语法通过不等于此行为已经执行。
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
// 仅取原Common返回值，任何ID或追加失败都不伪造成功来源。
template<class T>T take(Outcome<T> value){if(!value.ok)throw std::runtime_error(value.error.original.code);return std::move(value.value);}
// 空资产的独立Office夹具，沿Common真实分配序形成原组织更职史。
struct Fixture {
 std::shared_ptr<CommonCatalog> cc=std::make_shared<CommonCatalog>();CommonStore common{CommonState{},cc};OrganizationCatalog catalog;OrganizationState state;
 PoliticalSuccessionPolicy policy{"succession","city","chief","hall","grant",4,8,"capital"};
 Fixture(){cc->places.insert("hall",{"hall","town",{}});for(const auto* id:{"leader","deputy","candidate"}){ActorRecord a;a.id=id;a.alive=a.available=true;common.state().actors.insert(id,a);cc->actor_ids.push_back(id);}OrganizationRecord org;org.id="city";org.kind="political";org.headquarters_site="hall";org.succession_policy_id="succession";org.office_ids={"chief"};catalog.enabled=true;catalog.organizations.insert(org.id,org);state.organizations=catalog.organizations;OfficeState office;office.id="chief";office.organization_id="city";office.holder_actor_id="leader";office.deputy_actor_id="deputy";office.grant_ids={"grant"};catalog.offices.insert(office.id,office);state.offices=catalog.offices;Grant g;g.id="grant";g.office_id="chief";g.allowed_command_kinds={political_succession_nominate_command,political_succession_withdraw_command};g.scopes={{ScopeRef::Kind::site,"hall"}};catalog.grants.insert(g.id,g);}
 // 原更职事实只描述当时原Office结果，空缺原因须未来完整继位验另核真实死亡。
 StableId change(const StableId& actor){auto id=take(common.next_id("organization"));take(common.append_fact({id,"organization_office_changed","organization"}));OrganizationFact f;f.id=id;f.kind="organization_office_changed";f.organization_id="city";f.office_id="chief";f.actor_id=actor;f.tick=common.tick();state.facts.insert(id,f);auto* o=state.offices.find("chief");++o->epoch;o->holder_actor_id=actor.empty()?std::nullopt:std::optional<StableId>{actor};o->status=actor.empty()?OfficeStatus::vacant:OfficeStatus::active;return id;}
 StableId boundary(){auto id=take(common.next_id("political_succession_nomination"));take(common.append_fact({id,"political_successor_nominated",political_succession_domain}));return id;}
};
}
int main(){try{
 Fixture f;PoliticalSuccessionCatalog catalog;catalog.policies.insert(f.policy.id,f.policy);check(validate_political_succession_catalog(f.common,*f.cc,f.catalog,catalog).ok,"explicit original office/catalog/Grant source");
 auto too_long=catalog;too_long.policies.find(f.policy.id)->response_ticks=2147483648LL;check(!validate_political_succession_catalog(f.common,*f.cc,f.catalog,too_long).ok,"native response window follows authored finite bound");too_long=catalog;too_long.policies.find(f.policy.id)->accession_ticks=2147483648LL;check(!validate_political_succession_catalog(f.common,*f.cc,f.catalog,too_long).ok,"native accession window follows authored finite bound");
 auto encoded=encode_political_succession_catalog(catalog);check(compare_boundary(encoded,encode_political_succession_catalog(take(decode_political_succession_catalog(encoded)))).exact(),"strict authored catalog roundtrip");
 auto source=take(read_political_succession_office(f.common,f.state,f.catalog,f.policy));check(source.original_leader_actor_id=="leader"&&source.holder_actor_id==std::optional<StableId>{"leader"}&&source.office_epoch==0&&!source.vacant,"initial source only borrows real Office");
 auto before=f.boundary();auto vacancy=f.change({});auto old=take(read_political_succession_office_before(f.common,f.state,f.catalog,f.policy,0,before));check(old.holder_actor_id==std::optional<StableId>{"leader"}&&old.office_epoch==0,"same-tick later vacancy preserves earlier signer");
 auto current=take(read_political_succession_office(f.common,f.state,f.catalog,f.policy));check(current.vacant&&current.office_epoch==1&&current.latest_office_fact_id==vacancy,"actual vacancy remains distinct from accession");
 auto order=f.common.state().facts;f.common.state().facts={};f.common.state().facts.insert(vacancy,*order.find(vacancy));f.common.state().facts.insert(before,*order.find(before));check(!read_political_succession_office_before(f.common,f.state,f.catalog,f.policy,0,before).ok,"reordered Common header cannot override instance allocation");f.common.state().facts=order;
 check(!read_political_succession_office_before(f.common,f.state,f.catalog,f.policy,0,vacancy).ok,"arbitrary true organization fact is not a succession boundary");check(!read_political_succession_office_before(f.common,f.state,f.catalog,f.policy,1,before).ok,"future tick is rejected");
 f.change("deputy");auto deputy=take(read_political_succession_office(f.common,f.state,f.catalog,f.policy));check(!deputy.vacant&&deputy.holder_actor_id==std::optional<StableId>{"deputy"}&&deputy.original_leader_actor_id=="leader","temporary deputy holding does not become original formal leader");
 auto wrong=f.policy;wrong.leader_office_id="other";check(!read_political_succession_office(f.common,f.state,f.catalog,wrong).ok,"wrong office cannot borrow succession source");f.state.offices.find("chief")->epoch=9;check(!read_political_succession_office(f.common,f.state,f.catalog,f.policy).ok,"current Office tail must equal real change sequence");
 auto bad=catalog;bad.policies.find("succession")->response_ticks=0;check(!validate_political_succession_catalog(f.common,*f.cc,f.catalog,bad).ok,"no implicit response deadline");f.catalog.grants.find("grant")->allowed_command_kinds={political_succession_nominate_command};check(!validate_political_succession_catalog(f.common,*f.cc,f.catalog,catalog).ok,"withdrawal needs same explicit original Grant");
 std::cout<<checks<<" checks; no World or succession application claimed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
