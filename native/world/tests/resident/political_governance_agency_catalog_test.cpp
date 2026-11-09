// 仅检查独立代理目录和精确来源头，未签发代理、Task或钱粮；复用原真实人物夹具。
#define POLITICAL_OFFICE_RECEPTION_FIXTURE_ONLY
#include "political_office_reception_source_test.cpp"
#include "resident/domains/politics/governance_agencies/political_governance_agency_contract.hpp"
namespace {
struct AgencyCatalogFixture:ReceptionFixture {
 PoliticsCatalog political;PoliticsGovernanceCatalog governance;PoliticalGovernanceAgencyCatalog agencies;
 // 原治理财权与新的代理签字权分别显式作者，单独may_delegate不构成代理制度。
 AgencyCatalogFixture(){political.enabled=true;governance.enabled=true;Jurisdiction j;j.id="district";j.office_id="chief";j.political_organization_id="city";j.controlling_organization_id="city";j.seat_place_id="hall";j.public_wallet_id="treasury";j.public_inventory_ids={"granary"};political.jurisdictions.insert(j.id,j);governance.appeasement_rules.insert("calm",{"calm","1","calm.name","grain",6,120,10,3,100});governance.commission_policies.insert("commission",{"commission","calm","relief",4,12});Grant a;a.id="agency";a.office_id="chief";a.allowed_command_kinds={political_governance_agency_offer_command,political_governance_agency_revoke_command};a.scopes={{ScopeRef::Kind::site,"hall"}};catalog.grants.insert(a.id,a);Grant g;g.id="governance";g.office_id="chief";g.allowed_command_kinds={"political_commission_governance"};g.scopes=a.scopes;g.wallet_ids={"treasury"};g.inventory_ids={"granary"};g.amount_limit=120;g.may_delegate=true;catalog.grants.insert(g.id,g);catalog.offices.find("chief")->grant_ids.push_back(a.id);catalog.offices.find("chief")->grant_ids.push_back(g.id);state.offices.find("chief")->grant_ids=catalog.offices.find("chief")->grant_ids;agencies.policies.insert("agency.policy",{"agency.policy","district","chief",a.id,g.id,"commission","granary",4,12,2});}
 Status valid(){return validate_political_governance_agency_catalog(common,political,governance,catalog,agencies);}
};
}
#ifndef POLITICAL_GOVERNANCE_AGENCY_CATALOG_FIXTURE_ONLY
int main(){try{
 AgencyCatalogFixture f;check(f.valid().ok,"explicit original finance and separate signature delegation grants accepted");auto original=f.catalog.grants.find("agency")->allowed_command_kinds;f.catalog.grants.find("agency")->allowed_command_kinds={"political_commission_governance"};check(!f.valid().ok,"old work delegation cannot grant agency signature");f.catalog.grants.find("agency")->allowed_command_kinds=original;
 f.catalog.grants.find("agency")->wallet_ids={"treasury"};check(!f.valid().ok,"appointment Grant cannot gain direct budget scope");f.catalog.grants.find("agency")->wallet_ids.clear();f.catalog.grants.find("governance")->amount_limit=119;check(!f.valid().ok,"single underlying Grant must cover whole original commitment");f.catalog.grants.find("governance")->amount_limit=120;
 f.agencies.policies.find("agency.policy")->public_inventory_id="other";check(!f.valid().ok,"agency cannot redirect original public inventory");f.agencies.policies.find("agency.policy")->public_inventory_id="granary";f.agencies.policies.find("agency.policy")->maximum_issues=0;check(!f.valid().ok,"zero or implicit unlimited agency rejected");f.agencies.policies.find("agency.policy")->maximum_issues=2;
 f.catalog.offices.find("chief")->deputy_actor_id="candidate";check(!f.valid().ok,"legacy automatic deputy is a different authority contract");f.catalog.offices.find("chief")->deputy_actor_id.reset();check(f.valid().ok,"catalog restored without mutating current Office state");
 auto id=take(f.common.next_id("political_governance_agency_offer"));take(f.common.append_fact({id,"political_governance_agency_offered",political_governance_agency_domain}));check(political_governance_agency_fact_sequence(f.common,id,"political_governance_agency_offered").has_value(),"exact canonical source accepted");check(!political_governance_agency_fact_sequence(f.common,id,"political_governance_agency_accepted"),"offer cannot impersonate personal acceptance");check(!political_governance_agency_fact_sequence(f.common,id,"governance_authority_issued"),"thin agency source cannot authorize original governance by header");
 std::cout<<checks<<" governance agency catalog assertions; not run\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

#endif
