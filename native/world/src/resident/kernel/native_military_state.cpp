#include "resident/kernel/catalog_section_observation.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "native_military_state.hpp"
#include "native_organization_runtime.hpp"
#include "resident/checkpoint/checkpoint_json_decode.hpp"
#include "resident/checkpoint/checkpoint_catalog_json_kind.hpp"
#include "resident/domains/military/duty/military_duty_codec_fields.hpp"
#include "resident/domains/military/dynamic_action_codec.hpp"
#include "resident/domains/military/military_action_condition_codec.hpp"
#include "resident/domains/military/military_action_resource_codec.hpp"
#include "resident/domains/military/military_authority_time.hpp"
#include "resident/domains/military/military_formation_reservation_codec.hpp"
#include "resident/domains/military/military_return_detachment_codec.hpp"
#include "resident/domains/military/military_spatial_definition_catalog.hpp"
#include "resident/domains/military/military_session_definition_catalog.hpp"
#include "resident/domains/military/military_training_completion_time_codec.hpp"
#include "resident/storage/common_codec.hpp"
#include "world_bundle.hpp"
namespace hanmo::resident {
namespace {
const BoundaryValue* authored(const BoundaryValue& scenario,const char* key){
    const BoundaryValue* found=nullptr;
    for(const auto& field:boundary::object(scenario,"scenario"))if(field.first==key){
        if(found)boundary::fail("DuplicateMilitaryStateSelection",key);
        found=&field.second;
    }
    return found;
}
// 原 profile/版本不变，仅允许作者显式补充原物理/会话定义键表。
// 旧缺字段保原选择；仍拒绝未知字段和重复的必填或可选键。
Integer dynamic_selection(const BoundaryValue &config) {
  boundary::fields(config, {"profile", "timings"},
                   {"spatial_definitions", "session_definitions"},
                   "dynamic_military");
  const auto expected_fields =
      2u + unsigned(bool(config.find("spatial_definitions"))) +
      unsigned(bool(config.find("session_definitions")));
  if (boundary::object(config, "dynamic_military").size() != expected_fields)
    boundary::fail("DuplicateMilitaryStateSelection", "dynamic_military");
  const auto profile =
      boundary::get_string(config, "profile", "dynamic_military");
  if (profile == "exact_dynamic_military_v2")
    return 2;
  if (profile == "exact_dynamic_military_v3")
    return 3;
  if (profile == "exact_dynamic_military_v4")
    return 4;
  boundary::unsupported("UnsupportedDynamicMilitarySelection",
                        "dynamic_military/profile");
}
void duty_catalog_selection(const BoundaryValue& config){
    boundary::fields(config,{"profile","catalog"},{},"military_duty");
    if(boundary::object(config,"military_duty").size()!=2||
       boundary::get_string_ref(config,"profile","military_duty")!="original_military_duty_v1")
        boundary::unsupported("UnsupportedMilitaryDutyCatalogSelection","military_duty/profile");
}
void action_time_selection(const BoundaryValue& config){
    boundary::fields(config,{"profile"},{},"military_action_time");
    if(boundary::object(config,"military_action_time").size()!=1||
       boundary::get_string(config,"profile","military_action_time")!="original_military_action_time_v1")
        boundary::unsupported("UnsupportedMilitaryActionTimeSelection","military_action_time/profile");
}
void authority_time_selection(const BoundaryValue& config){
    boundary::fields(config,{"profile"},{},"military_authority_time");
    if(boundary::object(config,"military_authority_time").size()!=1||
       boundary::get_string_ref(config,"profile","military_authority_time")!="original_military_authority_time_v1")
        boundary::unsupported("UnsupportedMilitaryAuthorityTimeSelection","military_authority_time/profile");
}
Integer action_conditions_selection(const BoundaryValue& config){
    boundary::fields(config,{"profile"},{},"military_action_conditions");
    if(boundary::object(config,"military_action_conditions").size()!=1)
        boundary::unsupported("UnsupportedMilitaryActionConditionSelection","military_action_conditions/profile");
    const auto& profile=boundary::get_string_ref(config,"profile","military_action_conditions");
    if(profile=="original_military_action_conditions_v1")return 1;
    if(profile=="original_military_action_conditions_v2")return 2;
    boundary::unsupported("UnsupportedMilitaryActionConditionSelection","military_action_conditions/profile");
}
Integer formation_reservations_selection(const BoundaryValue& config){
    boundary::fields(config,{"profile"},{},"military_formation_reservations");
    if(boundary::object(config,"military_formation_reservations").size()!=1)
        boundary::unsupported("UnsupportedMilitaryFormationReservationSelection","military_formation_reservations/profile");
    const auto& profile=boundary::get_string_ref(config,"profile","military_formation_reservations");
    if(profile=="original_military_formation_reservations_v1")return 1;
    if(profile=="original_military_formation_reservations_v2")return 2;
    boundary::unsupported("UnsupportedMilitaryFormationReservationSelection","military_formation_reservations/profile");
}
void training_completion_selection(const BoundaryValue& config){
    boundary::fields(config,{"profile"},{},"military_training_completion_time");
    if(boundary::object(config,"military_training_completion_time").size()!=1||boundary::get_string_ref(config,"profile","military_training_completion_time")!="original_military_training_completion_time_v1")
        boundary::unsupported("UnsupportedMilitaryTrainingCompletionSelection","military_training_completion_time/profile");
}
void return_detachments_selection(const BoundaryValue& config){
    boundary::fields(config,{"profile"},{},"military_return_detachments");
    if(boundary::object(config,"military_return_detachments").size()!=1||boundary::get_string_ref(config,"profile","military_return_detachments")!="original_military_return_detachments_v1")
        boundary::unsupported("UnsupportedMilitaryReturnDetachmentSelection","military_return_detachments/profile");
}
void action_resources_selection(const BoundaryValue& config){
    boundary::fields(config,{"profile"},{},"military_action_resources");
    if(boundary::object(config,"military_action_resources").size()!=1||boundary::get_string_ref(config,"profile","military_action_resources")!="original_military_action_resources_v1")
        boundary::unsupported("UnsupportedMilitaryActionResourceSelection","military_action_resources/profile");
}
}
Outcome<NativeMilitaryStateVersions> selected_native_military_state_versions(const CatalogIdentity& identity){
    NativeMilitaryStateVersions out;
    for(const auto& row:identity.module_versions){
        if(row.first=="dynamic_military"){
            if(out.dynamic_military||(row.second!="2"&&row.second!="3"&&row.second!="4"))
                return {false,{},common_failure("DynamicMilitaryModuleMismatch",row.first).error};
            out.dynamic_military=row.second=="2"?2:(row.second=="3"?3:4);
        }else if(row.first=="military_duty_catalog"){
            if(out.military_duty_catalog||row.second!="1")
                return {false,{},common_failure("MilitaryDutyCatalogModuleMismatch",row.first).error};
            out.military_duty_catalog=1;
        }else if(row.first=="military_action_time"){
            if(out.military_action_time||row.second!="1")
                return {false,{},common_failure("MilitaryActionTimeModuleMismatch",row.first).error};
            out.military_action_time=1;
        }else if(row.first=="military_authority_time"){
            if(out.military_authority_time||row.second!="1")return {false,{},common_failure("MilitaryAuthorityTimeModuleMismatch",row.first).error};
            out.military_authority_time=1;
        }else if(row.first=="military_return_detachments"){
            if(out.military_return_detachments||row.second!="1")return {false,{},common_failure("MilitaryReturnDetachmentModuleMismatch",row.first).error};out.military_return_detachments=1;
        }else if(row.first=="military_action_resources"){
            if(out.military_action_resources||row.second!="1")return {false,{},common_failure("MilitaryActionResourceModuleMismatch",row.first).error};out.military_action_resources=1;
        }else if(row.first=="military_action_conditions"){
            if(out.military_action_conditions||(row.second!="1"&&row.second!="2"))return {false,{},common_failure("MilitaryActionConditionModuleMismatch",row.first).error};out.military_action_conditions=row.second=="1"?1:2;
        }else if(row.first=="military_formation_reservations"){
            if(out.military_formation_reservations||(row.second!="1"&&row.second!="2"))return {false,{},common_failure("MilitaryFormationReservationModuleMismatch",row.first).error};out.military_formation_reservations=row.second=="1"?1:2;
        }else if(row.first=="military_training_completion_time"){
            if(out.military_training_completion_time||row.second!="1")return {false,{},common_failure("MilitaryTrainingCompletionModuleMismatch",row.first).error};out.military_training_completion_time=1;
        }
    }
    if(out.military_authority_time&&!out.military_action_time)return {false,{},common_failure("MilitaryAuthorityActionTimeRequired").error};
    if(out.military_duty_catalog&&!out.dynamic_military)return {false,{},common_failure("MilitaryDutyDynamicStateRequired").error};
    if(out.military_action_resources&&!out.military_action_time)return {false,{},common_failure("MilitaryResourceActionTimeRequired").error};
    if(out.military_return_detachments&&!out.military_action_time)return {false,{},common_failure("MilitaryReturnDetachmentActionTimeRequired").error};
    if(out.military_action_conditions&&!out.military_action_time)return {false,{},common_failure("MilitaryConditionActionTimeRequired").error};
    if(out.military_training_completion_time&&!out.military_action_time)return {false,{},common_failure("MilitaryTrainingCompletionActionTimeRequired").error};
    if(out.military_formation_reservations&&!out.military_action_time)return {false,{},common_failure("MilitaryFormationReservationActionTimeRequired").error};
    return {true,out,{}};
}
Status append_native_military_state_modules(const BoundaryValue& scenario,CatalogIdentity& identity){try{
    auto old=selected_native_military_state_versions(identity);if(!old.ok)return {false,{},std::move(old.error)};
    if(old.value.dynamic_military||old.value.military_duty_catalog||old.value.military_action_time||old.value.military_authority_time||old.value.military_action_resources||old.value.military_return_detachments||old.value.military_action_conditions||old.value.military_training_completion_time||old.value.military_formation_reservations)
        return common_failure("DuplicateMilitaryStateModule");
    if(const auto* config=authored(scenario,"dynamic_military"))
        identity.module_versions.emplace_back("dynamic_military",std::to_string(dynamic_selection(*config)));
    if(const auto* config=authored(scenario,"military_duty")){
        duty_catalog_selection(*config);identity.module_versions.emplace_back("military_duty_catalog","1");
    }
    if(const auto* config=authored(scenario,"military_action_time")){
        action_time_selection(*config);identity.module_versions.emplace_back("military_action_time","1");
    }
    if(const auto* config=authored(scenario,"military_authority_time")){
        authority_time_selection(*config);identity.module_versions.emplace_back("military_authority_time","1");
    }
    if(const auto* config=authored(scenario,"military_action_resources")){action_resources_selection(*config);identity.module_versions.emplace_back("military_action_resources","1");}
    if(const auto* config=authored(scenario,"military_return_detachments")){return_detachments_selection(*config);identity.module_versions.emplace_back("military_return_detachments","1");}
    if(const auto* config=authored(scenario,"military_action_conditions")){const auto version=action_conditions_selection(*config);identity.module_versions.emplace_back("military_action_conditions",std::to_string(version));}
    if(const auto* config=authored(scenario,"military_training_completion_time")){training_completion_selection(*config);identity.module_versions.emplace_back("military_training_completion_time","1");}
    if(const auto* config=authored(scenario,"military_formation_reservations")){const auto version=formation_reservations_selection(*config);identity.module_versions.emplace_back("military_formation_reservations",std::to_string(version));}
    auto selected=selected_native_military_state_versions(identity);if(!selected.ok)return {false,{},std::move(selected.error)};
    return common_success();
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
// 职责：核已选模块与真实目录一致，再复用原 Military 规则校验。
// 可选物理/会话表缺席不升级旧 profile；表存在也不替代完整原来源。
Status validate_native_military_state_catalog(const PreparedCatalog &catalog) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 145, "PreparedCatalog", "validate_native_military_state_catalog", &catalog);

  auto selected = selected_native_military_state_versions(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (bool(selected.value.dynamic_military) !=
      bool(catalog.dynamic_military_actions))
    return common_failure("DynamicMilitarySelectedCatalogMismatch");
  if (catalog.military_spatial_definitions && !selected.value.dynamic_military)
    return common_failure("MilitarySpatialDefinitionSelectedCatalogMismatch");
  if (catalog.military_session_definitions && !selected.value.dynamic_military)
    return common_failure("MilitarySessionDefinitionSelectedCatalogMismatch");
  if (bool(selected.value.military_duty_catalog) !=
      bool(catalog.military_duty_catalog))
    return common_failure("MilitaryDutySelectedCatalogMismatch");
  if (!selected.value.dynamic_military &&
      !selected.value.military_duty_catalog &&
      !selected.value.military_action_time &&
      !selected.value.military_authority_time &&
      !selected.value.military_action_resources &&
      !selected.value.military_return_detachments &&
      !selected.value.military_action_conditions &&
      !selected.value.military_training_completion_time &&
      !selected.value.military_formation_reservations)
    return common_success();
  if (!catalog.exact_time || !exact_complete_selected(*catalog.exact_time) ||
      !catalog.military)
    return common_failure("MilitaryExactOriginalCatalogsRequired", {},
                          ErrorKind::unsupported_capability);
  if (selected.value.military_action_conditions && !catalog.actor_foundation)
    return common_failure("MilitaryActionConditionFoundationRequired", {},
                          ErrorKind::unsupported_capability);
  if (selected.value.military_authority_time ||
      selected.value.military_action_time) {
    auto effects =
        selected_native_organization_service_effects(catalog.identity);
    if (!effects.ok)
      return {false, {}, std::move(effects.error)};
    if (effects.value != 1 || !catalog.common || !catalog.organization ||
        !catalog.organization->service_effects_enabled || !catalog.family ||
        !catalog.interaction)
      return common_failure("MilitaryAuthorityOriginalRoleSourcesRequired", {},
                            ErrorKind::unsupported_capability);
  }
  if (catalog.dynamic_military_actions) {
    auto valid = validate_dynamic_military_action_catalog(
        *catalog.dynamic_military_actions, catalog.military->battle_catalog);
    if (!valid.ok)
      return valid;
  }
  if (catalog.military_spatial_definitions) {
    auto valid = validate_military_spatial_definition_catalog(
        *catalog.military_spatial_definitions);
    if (!valid.ok)
      return valid;
  }
  if (catalog.military_session_definitions) {
    auto valid = validate_military_session_definition_catalog(
        *catalog.military_session_definitions);
    if (!valid.ok)
      return valid;
  }
  if (catalog.military_duty_catalog)
    return validate_military_duty_catalog(*catalog.military_duty_catalog);
  return common_success();
}
// 职责：在同一冷读任务构造已选 Military 的唯一不可变目录。
// 原物理/会话表沿唯一 codec 解码并随 Prepared 锁定，缺字段保持 null。
Status prepare_native_military_state_catalog(const BoundaryValue &scenario,
                                             PreparedCatalog &catalog) {
  try {
    if (catalog.dynamic_military_actions ||
        catalog.military_spatial_definitions ||
        catalog.military_session_definitions)
      return common_failure("DynamicMilitaryCatalogAlreadyPrepared", {},
                            ErrorKind::internal);
    if (catalog.military_duty_catalog)
      return common_failure("MilitaryDutyCatalogAlreadyPrepared", {},
                            ErrorKind::internal);
    auto selected = selected_native_military_state_versions(catalog.identity);
    if (!selected.ok)
      return {false, {}, std::move(selected.error)};
    const auto *config = authored(scenario, "dynamic_military");
    if (bool(config) != bool(selected.value.dynamic_military))
      return common_failure("DynamicMilitarySelectedCatalogMismatch");
    if (config) {
      if (dynamic_selection(*config) != selected.value.dynamic_military)
        return common_failure("DynamicMilitarySelectedCatalogMismatch");
      {
        ScopedCheckpointJsonDecode original_context(
            CheckpointCatalogJsonKind::dynamic_military_action_timings);
        auto decoded = decode_dynamic_military_action_catalog2(
            boundary::field(*config, "timings", "dynamic_military"));
        if (!decoded.ok)
          return {false, {}, std::move(decoded.error)};
        catalog.dynamic_military_actions =
            checkpoint_make_shared<const DynamicMilitaryActionCatalog2>(
                std::move(decoded.value));
      }
      // 同一原冷读任务解真实作者键表；此处不持 view 或构造业务 provider。
      // 最终 identity 锁仍由后续原 PreparedCatalog sealing 完成。
      if (const auto *spatial = config->find("spatial_definitions")) {
        ScopedCheckpointJsonDecode original_context(
            CheckpointCatalogJsonKind::military_spatial_definitions);
        auto decoded = decode_military_spatial_definition_catalog(*spatial);
        if (!decoded.ok)
          return {false, {}, std::move(decoded.error)};
        catalog.military_spatial_definitions =
            checkpoint_make_shared<const MilitarySpatialDefinitionCatalog>(
                std::move(decoded.value));
      }
      // 同一冷读任务独立接纳原五槽 definition lock，不把它混入物理表。
      // 此处不初始化 session；RNG、entry 快照仍须后续真实来源。
      if (const auto *session = config->find("session_definitions")) {
        ScopedCheckpointJsonDecode original_context(
            CheckpointCatalogJsonKind::military_session_definitions);
        auto decoded = decode_military_session_definition_catalog(*session);
        if (!decoded.ok)
          return {false, {}, std::move(decoded.error)};
        catalog.military_session_definitions =
            checkpoint_make_shared<const MilitarySessionDefinitionCatalog>(
                std::move(decoded.value));
      }
    }
    const auto *duty = authored(scenario, "military_duty");
    if (bool(duty) != bool(selected.value.military_duty_catalog))
      return common_failure("MilitaryDutySelectedCatalogMismatch");
    if (duty) {
      duty_catalog_selection(*duty);
      ScopedCheckpointJsonDecode original_context(
          CheckpointCatalogJsonKind::military_duty_catalog);
      ActorFoundationJsonVisitor decoder(
          boundary::field(*duty, "catalog", "military_duty"));
      auto defaults =
          prepare_checkpoint_default_values<MilitaryDutyCatalog>(decoder);
      if (!defaults.ok)
        return defaults;
      auto decoded = checkpoint_make_shared<MilitaryDutyCatalog>();
      auto rows =
          decoder.table("rules", decoded->rules, [](auto &codec, auto &row) {
            return military_duty_codec::value(codec, row);
          });
      if (!rows.ok)
        return rows;
      auto finished = decoder.finish();
      if (!finished.ok)
        return finished;
      auto valid = validate_military_duty_catalog(*decoded);
      if (!valid.ok)
        return valid;
      // The immutable Prepared handle retains this same original catalog.
      // No synthetic identity or second Duty state is installed here.
      catalog.military_duty_catalog = std::move(decoded);
    }
    const auto *time = authored(scenario, "military_action_time");
    if (bool(time) != bool(selected.value.military_action_time))
      return common_failure("MilitaryActionTimeSelectedCatalogMismatch");
    if (time)
      action_time_selection(*time);
    const auto *authority = authored(scenario, "military_authority_time");
    if (bool(authority) != bool(selected.value.military_authority_time))
      return common_failure("MilitaryAuthorityTimeSelectedCatalogMismatch");
    if (authority)
      authority_time_selection(*authority);
    const auto *resources = authored(scenario, "military_action_resources");
    if (bool(resources) != bool(selected.value.military_action_resources))
      return common_failure("MilitaryActionResourceSelectedCatalogMismatch");
    if (resources)
      action_resources_selection(*resources);
    const auto *detachments = authored(scenario, "military_return_detachments");
    if (bool(detachments) != bool(selected.value.military_return_detachments))
      return common_failure("MilitaryReturnDetachmentSelectedCatalogMismatch");
    if (detachments)
      return_detachments_selection(*detachments);
    const auto *conditions = authored(scenario, "military_action_conditions");
    if (bool(conditions) != bool(selected.value.military_action_conditions))
      return common_failure("MilitaryActionConditionSelectedCatalogMismatch");
    if (conditions && action_conditions_selection(*conditions) !=
                          selected.value.military_action_conditions)
      return common_failure("MilitaryActionConditionSelectedCatalogMismatch");
    const auto *training =
        authored(scenario, "military_training_completion_time");
    if (bool(training) !=
        bool(selected.value.military_training_completion_time))
      return common_failure(
          "MilitaryTrainingCompletionSelectedCatalogMismatch");
    if (training)
      training_completion_selection(*training);
    const auto *reservations =
        authored(scenario, "military_formation_reservations");
    if (bool(reservations) !=
        bool(selected.value.military_formation_reservations))
      return common_failure(
          "MilitaryFormationReservationSelectedCatalogMismatch");
    if (reservations && formation_reservations_selection(*reservations) !=
                            selected.value.military_formation_reservations)
      return common_failure(
          "MilitaryFormationReservationSelectedCatalogMismatch");
    return validate_native_military_state_catalog(catalog);
  } catch (const boundary::DecodeFailure &error) {
    return {false, {}, error.error};
  }
}
Status validate_native_military_training_completion_headers(const CommonStore& common,const MilitaryState& military,const MilitaryActionTimeState* actions,Integer selected){
    if(selected!=0&&selected!=1)return common_failure("MilitaryTrainingCompletionModuleMismatch");
    if(selected&&!actions)return common_failure("MilitaryTrainingCompletionActionTimeRequired");
    if(actions){auto layout=validate_military_training_completion_time_layout(*actions,selected);if(!layout.ok)return layout;}
    for(const auto& row:common.state().facts.rows())if(row.second.kind=="military_training_completion_applied"){
        if(!actions)return common_failure("MilitaryTrainingCompletionActionTimeRequired",row.first);
        auto header=validate_military_training_completion_header_source(row.second,military,*actions,selected);if(!header.ok)return header;
    }
    if(actions)for(const auto& row:actions->dynamic_training_facts.rows())if(row.second.completion_application_source){
        auto header=validate_exact_training_completion_header(common,military,*actions,row.second);if(!header.ok)return header;
    }
    return common_success();
}
Status validate_native_military_state_layout(const WorldBundle& world){
    if(!world.catalog)return common_failure("MilitaryStateCatalogRequired",{},ErrorKind::internal);
    auto valid=validate_native_military_state_catalog(*world.catalog);if(!valid.ok)return valid;
    auto selected=selected_native_military_state_versions(world.catalog->identity);if(!selected.ok)return {false,{},std::move(selected.error)};
    if(bool(selected.value.dynamic_military)!=bool(world.military_exact)||
       bool(selected.value.military_action_time)!=bool(world.military_action_time))
        return common_failure("MilitarySelectedStatePairRequired");
    if(selected.value.dynamic_military==2&&
       (!world.military_exact->formations.empty()||!world.military_exact->missions.empty()))
        return common_failure("MilitaryTacticalOwnership3RequiresExplicitSelection");
    if(world.military_exact){auto duty=validate_military_duty_selected_layout(world.military_exact->duty_sources,selected.value.dynamic_military);if(!duty.ok)return duty;}
    for(const auto& row:world.military.campaigns)if(bool(row.exact_acceptance)!=bool(selected.value.military_authority_time))
        return common_failure("MilitaryAuthorityCampaignPayloadSelectionMismatch",row.id);
    for(const auto& row:world.military.resignation_facts)if(bool(row.exact_resignation)!=bool(selected.value.military_authority_time))
        return common_failure("MilitaryAuthorityResignationPayloadSelectionMismatch",row.id);
    if(world.military_action_time){auto resources=validate_military_action_resource_sources(world.common,*world.military_action_time,selected.value.military_action_resources);if(!resources.ok)return resources;}
    if(world.military_action_time){auto detachments=validate_military_return_detachment_sources(world.common,*world.military_action_time,selected.value.military_return_detachments);if(!detachments.ok)return detachments;}
    if(world.military_action_time){auto conditions=validate_military_action_condition_layout(*world.military_action_time,selected.value.military_action_conditions);if(!conditions.ok)return conditions;}
    if(world.military_action_time){auto reservations=validate_military_formation_reservation_sources(world.common,*world.catalog->military,*world.military_action_time,selected.value.military_formation_reservations);if(!reservations.ok)return reservations;}
    auto training=validate_native_military_training_completion_headers(world.common,world.military,world.military_action_time?&*world.military_action_time:nullptr,selected.value.military_training_completion_time);if(!training.ok)return training;
    // Both JSON and binary restore use the same real original row-to-head gate.
    // These are Common headers only; their payload remains on the original owner.
    for(const auto& row:world.common.state().facts.rows())if(row.second.kind=="campaign_accepted"||row.second.kind=="military_office_completion_applied"||row.second.kind=="military_formation_completion_checked"){
        const auto& fact=row.second;
        if(!world.military_action_time)return common_failure("MilitaryAuthorityHeaderActionTimeRequired",fact.id);
        auto source=validate_military_authority_header_source(fact,world.military,*world.military_action_time,selected.value.military_authority_time,selected.value.military_action_time);
        if(!source.ok)return source;
    }
    return common_success();
}
}
