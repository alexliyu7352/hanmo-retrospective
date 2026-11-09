#include "resident/kernel/catalog_section_observation.hpp"
// 作者绑定必须完整覆盖原条款且只落真实辖区；声明不代实际签约时控制权。
#include "treaty_passage_model.hpp"
#include <set>
namespace hanmo::resident {
Status validate_treaty_passage_catalog(const CommonCatalog& common,const OrganizationCatalog& organizations,const PoliticsCatalog& politics,const DiplomacyCatalog& diplomacy,const TreatyPassageCatalog& catalog){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 115, "TreatyPassageCatalog", "validate_treaty_passage_catalog", &catalog);

    if(catalog.version!=1)return common_failure("UnsupportedTreatyPassageVersion");
    if(!catalog.enabled)return catalog.provisions.empty()?common_success():common_failure("UnexpectedTreatyPassageCatalog");
    if(!politics.enabled||!diplomacy.enabled||!organizations.enabled||catalog.provisions.empty())return common_failure("TreatyPassageDependenciesRequired");
    for(const auto& row:catalog.provisions.rows()){
        const auto& p=row.second;const auto* terms=diplomacy.passage_terms.find(p.terms_id);
        if(row.first!=p.id||p.id!=p.terms_id||!terms||p.places.empty()||p.places.size()!=terms->place_ids.size()||terms->grantor_organization_id==terms->beneficiary_organization_id)return common_failure("InvalidTreatyPassageProvision",row.first);
        std::set<StableId> places;
        for(const auto& binding:p.places){const auto* jurisdiction=politics.jurisdictions.find(binding.jurisdiction_id);if(!common.places.find(binding.place_id)||!jurisdiction||!places.insert(binding.place_id).second)return common_failure("InvalidTreatyPassagePlace",binding.place_id);}
        for(const auto& place:terms->place_ids)if(!places.count(place))return common_failure("MissingTreatyPassagePlace",place);
        std::set<StableId> offices;for(const auto& id:p.npc_ratifier_office_ids){const auto* office=organizations.offices.find(id);if(!office||!offices.insert(id).second||(office->organization_id!=terms->grantor_organization_id&&office->organization_id!=terms->beneficiary_organization_id))return common_failure("InvalidTreatyPassageNpcMandate",id);}
    }
    return common_success();
}
}
