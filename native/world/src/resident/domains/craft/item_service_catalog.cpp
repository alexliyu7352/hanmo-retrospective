#include "resident/kernel/catalog_section_observation.hpp"
#include "item_service.hpp"
#include <algorithm>
namespace hanmo::resident {
Status validate_item_service_catalog(const ItemServiceCatalog&catalog,const CommonCatalog&common){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 19, "ItemServiceCatalog", "validate_item_service_catalog", &catalog);

 for(const auto&row:catalog.property_policies.rows()){
  const auto&p=row.second;
  if(row.first.empty()||row.first!=p.id||p.version<1||p.definition_id.empty()||!common.items.find(p.definition_id))return common_failure("InvalidItemPropertyPolicy",row.first,ErrorKind::internal);
  if(p.nonperishable?p.shelf_life_ticks.has_value():(!p.shelf_life_ticks||*p.shelf_life_ticks<=0))return common_failure("ItemExpiryPolicyMismatch",p.id,ErrorKind::internal);
  for(std::size_t i=0;i<p.allowed_fit_tag_ids.size();++i)if(p.allowed_fit_tag_ids[i].empty()||(i&&p.allowed_fit_tag_ids[i-1]>=p.allowed_fit_tag_ids[i]))return common_failure("ItemPropertyFitTagSetRequired",p.id,ErrorKind::internal);
 }
 return common_success();
}
}
