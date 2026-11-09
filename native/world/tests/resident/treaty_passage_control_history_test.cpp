// 两次真实占领/登记证明条约签署时的辖区权源；后来的同tick易手不重写旧签署。
#define main borrowed_siege_resource_main
#include "../test_siege_world_resources.cpp"
#undef main
#include "resident/domains/politics/takeover/politics_takeover_capture.hpp"
#include "resident/domains/politics/takeover/politics_takeover_original_access.hpp"
#include "resident/domains/politics/treaty/treaty_passage_access.hpp"
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
 auto fact=[&](const char* prefix,const char* kind){auto id=c.next_id(prefix);require(id,"allocate original population boundary");require(c.append_fact({id.value,kind,"politics"}),"append actual signature boundary");return id.value;};
 const auto before=fact("treaty_passage_signature","treaty_passage_signed");
 LocalEconomyCatalog ec;LocalEconomyState economy;TaskState tasks;TaskCatalog tc;OrganizationCatalog oc;oc.enabled=true;OrganizationState org;PoliticsCatalog pc;pc.enabled=true;
 for(auto id:{"old","new"}){OrganizationRecord o;o.id=id;o.headquarters_site="gate";oc.organizations.insert(id,o);OfficeState office;office.id=StableId(id)+"-authority";office.organization_id=id;office.holder_actor_id="commander";Grant grant;grant.id=StableId(id)+"-grant";grant.office_id=office.id;grant.allowed_command_kinds={political_takeover_command_kind};grant.scopes={{ScopeRef::Kind::site,"gate"}};grant.wallet_ids={"public-wallet"};grant.inventory_ids={"public-store"};office.grant_ids={grant.id};oc.grants.insert(grant.id,grant);oc.offices.insert(office.id,office);}
 org.organizations=oc.organizations;org.offices=oc.offices;Jurisdiction j;j.id="district";j.seat_place_id="gate";j.political_organization_id="old";j.controlling_organization_id="old";j.public_wallet_id="public-wallet";j.public_inventory_ids={"public-store"};pc.jurisdictions.insert(j.id,j);
 PoliticalTakeoverCatalog catalog;catalog.enabled=true;for(auto id:{"old","new"}){PoliticalTakeoverRule r;r.id=StableId(id)+"-rule";r.jurisdiction_id=j.id;r.site_id="gate";r.stronghold_id="fort";r.garrison_id="camp";r.successor_office_id=StableId(id)+"-authority";r.occupation_rule={r.id,1,20,5000};catalog.rules.insert(r.id,r);}
 PoliticalTakeoverState state;PoliticsState politics;politics.jurisdictions=pc.jurisdictions;
 auto apply=[&](MilitaryOccupationFact& occupation,const StableId& rule){auto id=c.next_id("military");require(id,"allocate actual occupation");occupation.id=id.value;require(c.append_fact({id.value,"military_occupation","military"}),"append actual occupation");auto capture=capture_political_takeover_responsibilities(c,economy,ec,nullptr,nullptr,nullptr,tasks,tc,nullptr,nullptr,f.state,id.value);require(capture,"capture original public assets");auto applied=apply_military_occupation_with_source(c,f.state,f.catalog,f.access,c.state().identity,id.value);require(applied,"actual S2 occupation apply");check(applied.value.source.has_value(),"actual source returned");auto registered=register_political_takeover_in_transaction(c,state,catalog,f.state,capture.value,*applied.value.source,rule);require(registered,"actual political pending registration");auto* current=politics.jurisdictions.find(j.id);current->controlling_organization_id=occupation.to_organization_id;++current->revision;return id.value;};
 const auto first=apply(f.state.occupation_facts.front(),"new-rule");
 PoliticalTakeoverOriginalAccess originals{{c,f.state,f.catalog,f.access,economy,ec,nullptr,nullptr,nullptr,tasks,tc,nullptr,nullptr}};PoliticalTakeoverContext context{c,*f.common,org,oc,pc,catalog,state,f.state,f.catalog,f.access,originals};TreatyPassageTerritoryAccess reader(c,politics,pc,&context);
 auto old=reader.treaty_control_before_signature("district",before,10);require(old,"signature before actual occupation");check(old.value.control.organization_id=="old"&&old.value.control.revision==0,"before occupation preserves original control");
 const auto first_sign=fact("treaty_passage_signature","treaty_passage_signed");auto captured=reader.treaty_control_before_signature("district",first_sign,10);require(captured,"signature after actual first occupation");check(captured.value.control.organization_id=="new"&&captured.value.control.occupation_fact_id==first&&captured.value.control.revision==1,"first controlled signature source");
 c.state().identity.tick=30;
 auto army=f.state.armies.front();army.id="retake-army";army.organization_id="old";army.order_id="retake-campaign";army.unit_ids={"retake-unit"};auto unit=f.state.military_units.front();unit.id="retake-unit";unit.army_id=army.id;unit.organization_id="old";unit.fit={"retake-soldier"};f.state.military_units.push_back(unit);f.state.armies.push_back(army);auto campaign=f.state.campaigns.front();campaign.id="retake-campaign";campaign.army_id=army.id;f.state.campaigns.push_back(campaign);auto siege=f.state.sieges.front();siege.id="retake-siege";siege.campaign_id=campaign.id;siege.armies={{army.id,0}};siege.result->tick=30;f.state.sieges.push_back(siege);f.state.strongholds.front().siege_id=siege.id;
 auto next=f.state.occupation_facts.front();next.id.clear();next.tick=30;next.siege_id=siege.id;next.campaign_id=campaign.id;next.army_id=army.id;next.previous_occupation_id=first;next.from_organization_id="new";next.to_organization_id="old";next.assets_applied=false;next.asset_manifest.reset();f.state.occupation_facts.push_back(next);const auto second=apply(f.state.occupation_facts.back(),"old-rule");
 auto historical=reader.treaty_control_before_signature("district",first_sign,10);require(historical,"signature before later same-world occupation");check(historical.value.control.organization_id=="new"&&historical.value.control.occupation_fact_id==first&&historical.value.control.revision==1,"later occupation cannot replace prior grantor history");
 const auto second_sign=fact("treaty_passage_signature","treaty_passage_signed");auto current=reader.treaty_control_before_signature("district",second_sign,30);require(current,"signature after true retake");check(current.value.control.organization_id=="old"&&current.value.control.occupation_fact_id==second&&current.value.control.revision==2,"new signature uses actual latest control");
 check(!reader.treaty_control_before_signature("district",second,30).ok,"military fact is not signature boundary");check(!reader.treaty_control_before_signature("district",second_sign,31).ok,"future boundary rejected");
 auto saved=c.state();auto* head=c.state().facts.find(second_sign);head->domain="tasks";check(!reader.treaty_control_before_signature("district",second_sign,30).ok,"wrong domain rejected");c.state()=saved;
 auto* jurisdiction=politics.jurisdictions.find("district");jurisdiction->controlling_organization_id="new";check(!reader.treaty_control_now("district").ok,"current political state must match actual military chain");jurisdiction->controlling_organization_id="old";
 const auto snapshot=encode_common_state(c.state());require(reader.treaty_control_now("district"),"current source read");check(compare_boundary(snapshot,encode_common_state(c.state())).exact(),"read leaves money lots IDs and facts unchanged");check(c.wallet("public-wallet")->balance==200&&c.inventory("public-store")->lots[0].quantity==5&&c.inventory("public-store")->lots[1].quantity==3,"two transfers conserve public cash and mixed goods");
 std::cout<<"treaty control history checks="<<checks<<" failures=0\n";
}
