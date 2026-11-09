// 仅原册结构与真实Common截面；不以此夹具证明占领、接册签权或完整法院行为。
#include "resident/domains/politics/licensing/political_shop_registry_sources.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
int checks=0;void check(bool ok,const char* what){++checks;if(!ok)throw std::runtime_error(what);}
struct Fixture {
 CommonStore common{CommonState{},std::make_shared<CommonCatalog>()};PoliticalShopLicensingCatalog catalog;PoliticalShopLicensingState state;
 Fixture(){common.state().identity.tick=4;common.state().next_instance_id=10;PoliticalShopCourt court;court.id="court";catalog.courts.insert(court.id,court);
  state.registries.insert("political_shop_registry:1",{"political_shop_registry:1","court",0});head("political_shop_registry:1","political_shop_registry_initialized");
  PoliticalWantedRecord warrant;warrant.id="political_wanted:2";warrant.court_id="court";warrant.actor_id="wanted";warrant.registry_fact_id="political_shop_registry:1";warrant.tick=0;state.warrants.insert(warrant.id,warrant);head(warrant.id,"political_wanted_registered");
  PoliticalWantedRelease release;release.id="political_wanted_release:3";release.warrant_fact_id=warrant.id;release.tick=1;state.releases.insert(release.id,release);head(release.id,"political_wanted_released");
  common.state().facts.insert("political_shop_registry_acceptance:4",{"political_shop_registry_acceptance:4",political_shop_registry_accept_kind,political_shop_registry_succession_module});
  PoliticalShopClearance clearance;clearance.id="political_shop_clearance:5";clearance.court_id="court";clearance.registry_fact_id="political_shop_registry:1";clearance.tick=2;state.clearances.insert(clearance.id,clearance);head(clearance.id,political_shop_clearance_kind);
  state.permits.insert("political_shop_permit:6",{"political_shop_permit:6",clearance.id,2});head("political_shop_permit:6",political_shop_permit_kind);
  PoliticalShopRevocation revoke;revoke.id="political_shop_revocation:7";revoke.permit_fact_id="political_shop_permit:6";revoke.tick=3;state.revocations.insert(revoke.id,revoke);head(revoke.id,"political_shop_permit_revoked");
 }
 void head(const StableId& id,const StableId& kind){common.state().facts.insert(id,{id,kind,political_shop_licensing_module});}
 auto read(const StableId& boundary={},std::optional<Integer> tick=std::nullopt){return read_political_shop_registry_legal_sources(common,catalog,state,"court",boundary,tick);}
};
}
int main(){try{
 Fixture f;auto all=f.read();check(all.ok&&all.value.fact_ids.size()==6,"current complete book retains released warrant and revoked permit");
 auto old=f.read("political_shop_registry_acceptance:4",2);check(old.ok&&old.value.fact_ids==std::vector<StableId>{"political_shop_registry:1","political_wanted:2","political_wanted_release:3"},"actual acceptance before same-tick later permit sources");
 check(!f.read("political_shop_permit:6",2).ok,"ordinary permit cannot impersonate acceptance boundary");check(!f.read("political_shop_registry_acceptance:4",5).ok,"future tick rejected");check(!f.read("political_shop_registry_acceptance:4").ok,"history requires explicit actual tick");check(!f.read({},3).ok,"current cannot request arbitrary old tick");
 {Fixture bad;bad.common.state().facts.find("political_shop_registry_acceptance:4")->kind="political_shop_permit_issued";check(!bad.read("political_shop_registry_acceptance:4",2).ok,"wrong actual boundary kind rejected");}
 {Fixture bad;bad.state.permits.find("political_shop_permit:6")->id="political_shop_permit:8";check(!bad.read().ok,"table key and payload cannot split");}
 {Fixture bad;bad.state.releases.find("political_wanted_release:3")->warrant_fact_id="missing";check(!bad.read().ok,"released source must remain in original book");}
 {Fixture bad;bad.state.revocations.find("political_shop_revocation:7")->permit_fact_id="missing";check(!bad.read().ok,"revoked permit must remain in original book");}
 {Fixture bad;bad.state.clearances.find("political_shop_clearance:5")->tick=1;check(!bad.read("political_shop_registry_acceptance:4",2).ok,"later source cannot claim earlier tick");}
 {Fixture bad;auto rows=bad.common.state().facts.rows();std::swap(rows[2],rows[3]);bad.common.state().facts={};for(const auto& row:rows)bad.common.state().facts.insert(row.first,row.second);check(!bad.read("political_shop_registry_acceptance:4",2).ok,"canonical allocation and Common order must agree");}
 {Fixture bad;bad.common.state().facts.insert("political_shop_permit:8",{"political_shop_permit:8",political_shop_permit_kind,political_shop_licensing_module});check(!bad.read().ok,"orphan legal head rejected");}
 {Fixture bad;bad.state.permits.insert("political_shop_permit:8",{"political_shop_permit:8","political_shop_clearance:5",4});check(!bad.read().ok,"sidecar without Common head rejected");}
 {Fixture bad;bad.state.registries.insert("political_shop_registry:8",{"political_shop_registry:8","court",4});bad.head("political_shop_registry:8","political_shop_registry_initialized");check(!bad.read().ok,"one original complete book per court");}
 std::cout<<checks<<" registry source checks, 0 failures; structure only\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
