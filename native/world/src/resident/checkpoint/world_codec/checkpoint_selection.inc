// 共同编解码世界编解码片段：保持原调用位置、字段与首错顺序；仅由
// world_codec.cpp 登记包含。
// 标题选槽只借用既有typed读者取得原目录/情景/创始人；不导出世界，不分配执行epoch。
Outcome<BoundaryValue> inspect_world_checkpoint_selection(const Bytes &bytes) {
  try {
    auto decoded = inspect_known_checkpoint(bytes);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    auto handover_version = political_head_handover_selected_version(
        decoded.value.identity.catalog.module_versions);
    if (!handover_version.ok)
      return {false, {}, handover_version.error};
    auto round_version = political_successor_round_selected_version(
        decoded.value.identity.catalog.module_versions);
    if (!round_version.ok)
      return {false, {}, round_version.error};
    auto term_version = political_term_handover_selected_version(
        decoded.value.identity.catalog.module_versions);
    if (!term_version.ok)
      return {false, {}, term_version.error};
    auto reception_version = political_office_reception_selected_version(
        decoded.value.identity.catalog.module_versions);
    if (!reception_version.ok)
      return {false, {}, reception_version.error};
    auto resignation_version = political_office_resignation_selected_version(
        decoded.value.identity.catalog.module_versions);
    if (!resignation_version.ok)
      return {false, {}, resignation_version.error};
    auto subordinate_version =
        political_subordinate_appointment_selected_version(
            decoded.value.identity.catalog.module_versions);
    if (!subordinate_version.ok)
      return {false, {}, subordinate_version.error};
    auto dismissal_version = political_dismissal_source_selected_version(
        decoded.value.identity.catalog.module_versions);
    if (!dismissal_version.ok)
      return {false, {}, dismissal_version.error};
    auto agency_version = political_governance_agency_selected_version(
        decoded.value.identity.catalog.module_versions);
    if (!agency_version.ok)
      return {false, {}, agency_version.error};
    auto succession_version = political_succession_selected_version(
        decoded.value.identity.catalog.module_versions);
    if (!succession_version.ok)
      return {false, {}, succession_version.error};
    CodecDecodeBudget budget(engineering_checkpoint_capacity_profile());
    if (decoded.value.container_version == 2) {
      auto charged =
          charge_exact_checkpoint_payloads(budget, bytes, decoded.value);
      if (!charged.ok)
        return {false, {}, charged.error};
    }
    CommonState common;
    FamilyState family;
    ResidentResultCodec receipts;
    Integer naval_version = 0;
    for (const auto &section : decoded.value.sections)
      if (section.domain == "military_naval")
        naval_version = section.codec_version;
    std::uint32_t political_features = 0;
    {
      const auto &modules = decoded.value.identity.catalog.module_versions;
      const bool enabled =
          std::find(modules.begin(), modules.end(),
                    std::make_pair(StableId{private_shop_daily_rent_module},
                                   StableId{"1"})) != modules.end();
      auto checked = validate_private_shop_daily_rent_pair(enabled, modules);
      if (!checked.ok)
        return {false, {}, checked.error};
      if (enabled)
        political_features |= private_shop_daily_rent_checkpoint_feature;
    }
    {
      const auto &modules = decoded.value.identity.catalog.module_versions;
      const bool enabled =
          std::find(modules.begin(), modules.end(),
                    std::make_pair(StableId{governance_invitation_reply_module},
                                   StableId{"1"})) != modules.end();
      auto checked =
          validate_governance_invitation_reply_modules(modules, enabled);
      if (!checked.ok)
        return {false, {}, checked.error};
      if (enabled)
        political_features |= governance_invitation_reply_feature;
    }
    {
      const auto &modules = decoded.value.identity.catalog.module_versions;
      const bool enabled =
          std::find(
              modules.begin(), modules.end(),
              std::make_pair(StableId{political_governance_continuation_module},
                             StableId{"1"})) != modules.end();
      auto checked =
          validate_political_governance_continuation_modules(modules, enabled);
      if (!checked.ok)
        return {false, {}, checked.error};
      if (enabled)
        political_features |= political_governance_continuation_feature;
    }
    {
      const auto &modules = decoded.value.identity.catalog.module_versions;
      const bool enabled =
          std::find(modules.begin(), modules.end(),
                    std::make_pair(StableId{"private_shops"}, StableId{"1"})) !=
          modules.end();
      auto paired = validate_private_shop_pair(enabled, modules);
      if (!paired.ok)
        return {false, {}, paired.error};
      if (enabled)
        political_features |= private_shop_checkpoint_feature;
      if (std::find(
              modules.begin(), modules.end(),
              std::make_pair(StableId{"political_shop_registry_successions"},
                             StableId{"1"})) != modules.end())
        political_features |=
            political_shop_registry_succession_checkpoint_feature;
    }
    {
      const auto &modules = decoded.value.identity.catalog.module_versions;
      const bool enabled =
          std::find(modules.begin(), modules.end(),
                    std::make_pair(StableId{"economic_supply_policies"},
                                   StableId{"1"})) != modules.end();
      auto layout = validate_economic_supply_layout(enabled, modules);
      if (!layout.ok)
        return {false, {}, layout.error};
      if (enabled)
        political_features |= economic_supply_policy_checkpoint_feature;
    }
    if (std::find(decoded.value.identity.catalog.module_versions.begin(),
                  decoded.value.identity.catalog.module_versions.end(),
                  std::make_pair(StableId{military_field_command_module},
                                 StableId{"1"})) !=
        decoded.value.identity.catalog.module_versions.end())
      political_features |= military_field_command_checkpoint_feature;
    if (std::find(
            decoded.value.identity.catalog.module_versions.begin(),
            decoded.value.identity.catalog.module_versions.end(),
            std::make_pair(StableId{military_officer_module}, StableId{"1"})) !=
        decoded.value.identity.catalog.module_versions.end())
      political_features |= military_officer_checkpoint_feature;
    if (std::find(decoded.value.identity.catalog.module_versions.begin(),
                  decoded.value.identity.catalog.module_versions.end(),
                  std::make_pair(StableId{"treaty_passage"}, StableId{"1"})) !=
        decoded.value.identity.catalog.module_versions.end())
      political_features |= treaty_passage_checkpoint_feature;
    if (std::find(decoded.value.identity.catalog.module_versions.begin(),
                  decoded.value.identity.catalog.module_versions.end(),
                  std::make_pair(StableId{"political_occupation_fiscal"},
                                 StableId{"1"})) !=
        decoded.value.identity.catalog.module_versions.end())
      political_features |= occupation_fiscal_checkpoint_feature;
    if (std::find(decoded.value.identity.catalog.module_versions.begin(),
                  decoded.value.identity.catalog.module_versions.end(),
                  std::make_pair(StableId{"diplomacy"}, StableId{"2"})) !=
        decoded.value.identity.catalog.module_versions.end())
      political_features |= diplomatic_report_checkpoint_feature;
    if (std::find(decoded.value.identity.catalog.module_versions.begin(),
                  decoded.value.identity.catalog.module_versions.end(),
                  std::make_pair(StableId{"household_provision_management"},
                                 StableId{"1"})) !=
        decoded.value.identity.catalog.module_versions.end())
      political_features |=
          common_checkpoint::household_provision_configuration_receipts;
    if (naval_version)
      political_features |= naval_command_codec::checkpoint_feature;
    if (std::find(
            decoded.value.identity.catalog.module_versions.begin(),
            decoded.value.identity.catalog.module_versions.end(),
            std::make_pair(StableId{"military_requests"}, StableId{"1"})) !=
        decoded.value.identity.catalog.module_versions.end())
      political_features |= military_request_codec::checkpoint_feature;
    if (std::find(
            decoded.value.identity.catalog.module_versions.begin(),
            decoded.value.identity.catalog.module_versions.end(),
            std::make_pair(StableId{"production_management"}, StableId{"1"})) !=
        decoded.value.identity.catalog.module_versions.end())
      political_features |=
          common_checkpoint::production_configuration_receipts;
    for (const auto &s : decoded.value.sections) {
      if (s.domain == "politics")
        political_features |= common_checkpoint::politics_receipts;
      if (s.domain == "political_fiscal")
        political_features |= common_checkpoint::political_fiscal_receipts;
    }
    {
      const auto &modules = decoded.value.identity.catalog.module_versions;
      const bool estate =
          std::find(modules.begin(), modules.end(),
                    std::make_pair(StableId{private_shop_liquidation_module},
                                   StableId{"1"})) != modules.end();
      auto paired = validate_private_shop_liquidation_pair(estate, modules);
      if (!paired.ok)
        return {false, {}, paired.error};
      if (estate)
        political_features |= private_shop_liquidation_checkpoint_feature;
    }
    for (const auto &section : decoded.value.sections) {
      if (section.domain != "common" && section.domain != "family")
        continue;
      CodecVisitor reader =
          decoded.value.container_version == 2
              ? CodecVisitor(section.payload,
                             engineering_checkpoint_capacity_profile(),
                             section.domain, budget)
              : CodecVisitor(section.payload);
      auto resource_features = configure_domain_resource_references(
          reader, decoded.value.identity.catalog);
      if (!resource_features.ok)
        return {false, {}, std::move(resource_features.error)};
      reader.political_succession_codec_version = succession_version.value;
      reader.political_head_handover_codec_version = handover_version.value;
      reader.political_term_handover_codec_version = term_version.value;
      reader.political_office_reception_codec_version = reception_version.value;
      reader.political_office_resignation_codec_version =
          resignation_version.value;
      reader.political_subordinate_appointment_codec_version =
          subordinate_version.value;
      reader.political_governance_agency_codec_version = agency_version.value;
      reader.political_successor_round_codec_version = round_version.value;
      reader.schema_features = political_features;
      reader.naval_codec_version = naval_version;
      if (decoded.value.identity.catalog.ruleset_id == dynamic_ruleset_id)
        reader.schema_features |= common_checkpoint::typed_resource_references;
      if (section.domain == "common") {
        auto physical = configure_common_physical_codec(
            reader, section.codec_version, decoded.value.identity.catalog);
        if (!physical.ok)
          return {false, {}, std::move(physical.error)};
      }
      // 标题和完整恢复均先拆接续位，再拆成年收养位，保留原家庭/创建能力。
      auto status =
          section.domain == "common"
              ? visit_common_checkpoint(reader, common, &receipts)
              : visit_family_checkpoint_versioned(
                    reader, family, section.codec_version,
                    std::find(
                        decoded.value.identity.catalog.module_versions.begin(),
                        decoded.value.identity.catalog.module_versions.end(),
                        std::make_pair(StableId{"skills_extended"},
                                       StableId{"1"})) !=
                        decoded.value.identity.catalog.module_versions.end());
      if (!status.ok)
        return {false, {}, status.error};
      status = reader.finish();
      if (!status.ok)
        return {false, {}, status.error};
    }
    const auto &identity = decoded.value.identity;
    const auto &id = identity.catalog;
    const auto &metadata = common.metadata;
    if (common.identity.world_id != identity.world_id ||
        common.identity.tick != identity.tick ||
        common.identity.revision != identity.revision)
      return {false, {}, common_failure("CheckpointIdentityMismatch").error};
    if (metadata.content_id != id.content_id ||
        metadata.content_version != id.content_version ||
        metadata.content_digest != id.content_digest ||
        metadata.ruleset_id != id.ruleset_id)
      return {false, {}, common_failure("CheckpointCatalogMismatch").error};
    if (metadata.scenario_id.empty() || family.founder_actor_id.empty() ||
        !common.actors.find(metadata.player_actor_id) ||
        !common.actors.find(family.founder_actor_id))
      return {false, {}, common_failure("InvalidCheckpointSelection").error};
    using V = BoundaryValue;
    auto catalog = boundary::obj({{"content_id", V{id.content_id}},
                                  {"content_version", V{id.content_version}},
                                  {"content_digest", V{id.content_digest}},
                                  {"ruleset_id", V{id.ruleset_id}},
                                  {"schema_sha256", V{id.schema_sha256}},
                                  {"artifact_sha256", V{id.artifact_sha256}}});
    if (id.pack_set_digest)
      catalog.as<V::Dict>()->emplace_back("pack_set_digest",
                                          V{*id.pack_set_digest});
    V::Array versions;
    for (const auto &version : id.module_versions)
      versions.push_back(boundary::obj(
          {{"name", V{version.first}}, {"version", V{version.second}}}));
    catalog.as<V::Dict>()->emplace_back("module_versions",
                                        V{std::move(versions)});
    return {true,
            boundary::obj({{"ok", V{true}},
                           {"catalog", std::move(catalog)},
                           {"scenario_id", V{metadata.scenario_id}},
                           {"founder_actor_id", V{family.founder_actor_id}},
                           {"player_actor_id", V{metadata.player_actor_id}}}),
            {}};
  } catch (const std::bad_alloc &) {
    return {false, {}, common_failure("CheckpointResourceLimit").error};
  }
}
