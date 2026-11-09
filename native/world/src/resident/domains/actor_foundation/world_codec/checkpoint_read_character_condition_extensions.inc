// 人物底座检查点恢复分派：沿原 else-if 链读取
// character_condition_extensions；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "character_condition_extensions") r =
    injury_version.value == 1
        ? visit_character_condition_extensions_v8(
              reader, actor_foundation->character_condition_extensions)
    : catalog.growth->complete_capability_application_chain
        ? visit_character_condition_extensions_v7(
              reader, actor_foundation->character_condition_extensions)
        : visit_character_condition_extensions_v1(
              reader, actor_foundation->character_condition_extensions,
              exact_complete_selected(*catalog.exact_time)
                  ? ConditionWireProfile::receipt_v2_union
                  : ConditionWireProfile::legacy_rest_only);
