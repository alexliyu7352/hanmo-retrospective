// 人物底座检查点恢复分派：沿原 else-if 链读取 actor_start_context,
// character_origin_records；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "actor_start_context") r =
    exact_complete_selected(*catalog.exact_time)
        ? visit_actor_start_context_v3(reader,
                                       actor_foundation->actor_start_context)
        : visit_actor_start_context_v2(reader,
                                       actor_foundation->actor_start_context);
else if (section.domain == "character_origin_records") r =
    visit_character_origin_records_v1(
        reader, actor_foundation->character_origin_records,
        exact_complete_selected(*catalog.exact_time)
            ? ActorOriginWireProfile::complete_origin2
            : ActorOriginWireProfile::authored_only);
