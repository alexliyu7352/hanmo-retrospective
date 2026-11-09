#include "native_local_move_catalog.hpp"
#include "native_transport_catalog.hpp"
#include "world_bundle.hpp"
#include "resident/domains/travel/movement_record_codec_fields.hpp"
#include "resident/domains/travel/local_move_source.hpp"
namespace hanmo::resident {
// 冷读必须同时具备版本与真实前置来源；旧 bank 缺 marker 不静默升级。
Outcome<Integer> selected_native_local_move_version(const CatalogIdentity& identity) {
 auto transport=selected_native_transport_versions(identity);
 if(!transport.ok) return {false,{},std::move(transport.error)};
 const auto parent=transport.value.travel;
 bool child=false,origin=false;
 for(const auto& [name,version]:identity.module_versions) {
  if(name==local_move_module) {
   if(child || version!="1") return {false,{},common_failure("LocalMoveModuleMismatch",name).error};
   child=true;
  } else if(name==local_move_derivation_key) {
   if(origin || !parent || version!="travel/"+std::to_string(parent))
    return {false,{},common_failure("LocalMoveDerivationMismatch",name).error};
   origin=true;
  }
 }
 if(child!=bool(parent) || origin!=bool(parent))
  return {false,{},common_failure("LocalMoveDerivedSelectionRequired",local_move_module).error};
 return {true,parent?Integer{1}:Integer{0},{}};
}
// 仅新局目录生成派生身份；作者不能覆盖这一技术子模块。
Status append_native_local_move_module(const BoundaryValue& scenario,CatalogIdentity& identity) {
 try {
  if(const auto* dynamic=scenario.find("dynamic_world"))
   for(const auto& [name,value]:boundary::object(*dynamic,"dynamic_world")) {
    (void)value;
    if(name==local_move_module || name==local_move_derivation_key)
     return common_failure("LocalMoveIsDerived",name);
   }
  for(const auto& [name,version]:identity.module_versions) {
   (void)version;
   if(name==local_move_module || name==local_move_derivation_key)
    return common_failure("DuplicateLocalMoveModule",name);
  }
  auto transport=selected_native_transport_versions(identity);
  if(!transport.ok) return {false,{},std::move(transport.error)};
  if(transport.value.travel) {
   identity.module_versions.emplace_back(local_move_module,"1");
   identity.module_versions.emplace_back(local_move_derivation_key,"travel/"+std::to_string(transport.value.travel));
  }
  return common_success();
 } catch(const boundary::DecodeFailure& e) { return {false,{},e.error}; }
}
// 低层全表正反覆盖，无 World owner 时也不能漏掉孤头或未选残留。
Status validate_native_local_move_headers(const CommonState& common,const CatalogIdentity& identity,
                                          const MovementExactState* movement) {
 auto version=selected_native_local_move_version(identity);
 if(!version.ok) return {false,{},std::move(version.error)};
 return validate_local_move_headers(common,movement,version.value==1);
}
// 此处不重建业务，不以布局校验代替完整空间前驱历史验证。
Status validate_native_local_move_layout(const WorldBundle& world) {
 if(!world.catalog) return common_failure("LocalMoveOriginalCatalogRequired");
 return validate_native_local_move_headers(world.common.state(),world.catalog->identity,
                                          world.movement_exact?&*world.movement_exact:nullptr);
}
// 复用真实 Travel4/5 的字段选择，绝不要求不相关的 Field/boundary 子模块。
Outcome<MovementRecordCodecSelection> native_local_move_codec_selection(const PreparedCatalog& catalog) {
 auto selected=selected_native_local_move_version(catalog.identity);
 if(!selected.ok) return {false,{},std::move(selected.error)};
 if(selected.value!=1) return {false,{},common_failure("LocalMoveNotSelected").error};
 auto transport=native_transport_physical_selection(catalog);
 if(!transport.ok) return {false,{},std::move(transport.error)};
 const auto& v=transport.value;
 return {true,{v.travel_version,v.group_transport_version,v.common_resources,
               v.carrier_location_enabled,v.typed_reservations,v.domain_activity_reservations},{}};
}
} // namespace hanmo::resident
