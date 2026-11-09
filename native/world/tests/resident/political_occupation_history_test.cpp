// 两次实际军事 apply 与政治登记的小组件，按原开始/完成头核历史，绝不将夹具胜利称自然旅程。
#define main borrowed_siege_resource_main
#include "../test_siege_world_resources.cpp"
#undef main
#include "resident/domains/politics/takeover/politics_takeover_capture.hpp"
#include "resident/domains/politics/takeover/politics_takeover_original_access.hpp"
#include "resident/domains/politics/takeover/politics_occupation_access.hpp"
#include "resident/domains/military/occupation_applied_source.hpp"
#include "resident/storage/common_codec.hpp"
namespace {
int checks=0;
void check(bool ok,const char* label){++checks;if(!ok){std::cerr<<"FAIL "<<label<<'\n';std::exit(1);}}
template<class T> void require(const Outcome<T>& value,const char* label){if(!value.ok)std::cerr<<value.error.original.code<<'\n';check(value.ok,label);}
}
// 原窗口结束同刻再次易手，旧完成仍合法，新开始必须受新窗口约束。
int main(){
 Fixture f;f.catalog.military=true;f.initial.identity.world_id[0]=1;f.common->places.insert("gate",{"gate","target",{}});CommonStore c(f.initial,f.common);f.access.fact_exists=[&](const StableId& id){return c.state().facts.find(id)!=nullptr;};
 auto fact=[&](const char* prefix,const char* kind){auto id=c.next_id(prefix);require(id,"allocate original population boundary");require(c.append_fact({id.value,kind,"population_military"}),"append original population boundary");return id.value;};
 const auto before=fact("action","population_military_action");
 LocalEconomyCatalog ec;LocalEconomyState economy;TaskState tasks;TaskCatalog tc;OrganizationCatalog oc;oc.enabled=true;OrganizationState org;PoliticsCatalog pc;pc.enabled=true;
 for(auto id:{"old","new"}){OrganizationRecord o;o.id=id;o.headquarters_site="gate";oc.organizations.insert(id,o);OfficeState office;office.id=StableId(id)+"-authority";office.organization_id=id;office.holder_actor_id="commander";Grant grant;grant.id=StableId(id)+"-grant";grant.office_id=office.id;grant.allowed_command_kinds={political_takeover_command_kind};grant.scopes={{ScopeRef::Kind::site,"gate"}};grant.wallet_ids={"public-wallet"};grant.inventory_ids={"public-store"};office.grant_ids={grant.id};oc.grants.insert(grant.id,grant);oc.offices.insert(office.id,office);}
 org.organizations=oc.organizations;org.offices=oc.offices;Jurisdiction j;j.id="district";j.seat_place_id="gate";j.political_organization_id="old";j.controlling_organization_id="old";j.public_wallet_id="public-wallet";j.public_inventory_ids={"public-store"};pc.jurisdictions.insert(j.id,j);
 PoliticalTakeoverCatalog catalog;catalog.enabled=true;for(auto id:{"old","new"}){PoliticalTakeoverRule r;r.id=StableId(id)+"-rule";r.jurisdiction_id=j.id;r.site_id="gate";r.stronghold_id="fort";r.garrison_id="camp";r.successor_office_id=StableId(id)+"-authority";r.occupation_rule={r.id,1,20,5000};catalog.rules.insert(r.id,r);}
 PoliticalTakeoverState state;
 auto apply=[&](MilitaryOccupationFact& occupation,const StableId& rule){auto id=c.next_id("military");require(id,"allocate actual occupation");occupation.id=id.value;require(c.append_fact({id.value,"military_occupation","military"}),"append actual occupation");auto capture=capture_political_takeover_responsibilities(c,economy,ec,nullptr,nullptr,nullptr,tasks,tc,nullptr,nullptr,f.state,id.value);require(capture,"capture original public assets");auto applied=apply_military_occupation_with_source(c,f.state,f.catalog,f.access,c.state().identity,id.value);require(applied,"actual S2 occupation apply");check(applied.value.source.has_value(),"actual source returned");auto registered=register_political_takeover_in_transaction(c,state,catalog,f.state,capture.value,*applied.value.source,rule);require(registered,"actual political pending registration");return id.value;};
 const auto first=apply(f.state.occupation_facts.front(),"new-rule");
 PoliticalTakeoverOriginalAccess originals{{c,f.state,f.catalog,f.access,economy,ec,nullptr,nullptr,nullptr,tasks,tc,nullptr,nullptr}};PoliticalTakeoverContext context{c,*f.common,org,oc,pc,catalog,state,f.state,f.catalog,f.access,originals};PoliticalOccupationAccess reader(context);
 const auto original=c.state();auto pre=reader.recruitment_occupation_before("camp",10,before);require(pre,"pre-occupation start history");check(!pre.value,"old start before original occupation has no new ban");
 c.state().identity.tick=29;const auto start=fact("action","population_military_action");auto early=reader.recruitment_occupation_before("camp",29,start);require(early,"last-day real start boundary");check(early.value&&early.value->blocked&&early.value->occupation_fact_id==first,"start before expiry is blocked");
 c.state().identity.tick=30;const auto complete=fact("population_military","population_enrolled");auto done=reader.recruitment_occupation_before("camp",30,complete);require(done,"real completion at expiry");check(done.value&&!done.value->blocked,"completion expiry cannot authorize its earlier start");
 auto army=f.state.armies.front();army.id="retake-army";army.organization_id="old";army.order_id="retake-campaign";army.unit_ids={"retake-unit"};auto unit=f.state.military_units.front();unit.id="retake-unit";unit.army_id=army.id;unit.organization_id="old";unit.fit={"retake-soldier"};f.state.military_units.push_back(unit);f.state.armies.push_back(army);auto campaign=f.state.campaigns.front();campaign.id="retake-campaign";campaign.army_id=army.id;f.state.campaigns.push_back(campaign);auto siege=f.state.sieges.front();siege.id="retake-siege";siege.campaign_id=campaign.id;siege.armies={{army.id,0}};siege.result->tick=30;f.state.sieges.push_back(siege);f.state.strongholds.front().siege_id=siege.id;
 auto next=f.state.occupation_facts.front();next.id.clear();next.tick=30;next.siege_id=siege.id;next.campaign_id=campaign.id;next.army_id=army.id;next.previous_occupation_id=first;next.from_organization_id="new";next.to_organization_id="old";next.assets_applied=false;next.asset_manifest.reset();f.state.occupation_facts.push_back(next);const auto second=apply(f.state.occupation_facts.back(),"old-rule");
 auto historical=reader.recruitment_occupation_before("camp",30,complete);require(historical,"same tick old completion after later retake");check(historical.value&&!historical.value->blocked&&historical.value->occupation_fact_id==first,"later same-tick occupation does not rewrite earlier result");
 const auto restarted=fact("action","population_military_action");auto latest=reader.recruitment_occupation_before("camp",30,restarted);require(latest,"real new start after retake");check(latest.value&&latest.value->blocked&&latest.value->occupation_fact_id==second&&latest.value->integration_until_tick==50,"choose latest eligible occupation rather than oldest chain entry");
 c.state().identity.tick=50;const auto end=fact("population_military","population_enrolled");auto expired=reader.recruitment_occupation_before("camp",50,end);require(expired,"second actual window expiry");check(expired.value&&!expired.value->blocked&&expired.value->occupation_fact_id==second,"second boundary expires exactly");
 check(!reader.recruitment_occupation_before("camp",51,end).ok,"future historical tick rejected");check(!reader.recruitment_occupation_before("camp",50,second).ok,"military header is not population authority");
 const auto snapshot=encode_common_state(c.state());reader.recruitment_occupation_before("camp",30,complete);check(compare_boundary(snapshot,encode_common_state(c.state())).exact(),"history does not mutate money lots ID or facts");check(c.wallet("public-wallet")->balance==200&&c.inventory("public-store")->lots[0].quantity==5&&c.inventory("public-store")->lots[1].quantity==3,"two true transfers preserve money and public/private goods");
 std::cout<<"recruitment occupation history checks="<<checks<<" failures=0\n";
}
