// 家庭世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
// 师承原件逐一投影共同事实；孤头、孤实体、错域/类型与重复ID均拒，绑定只留独立尾段。
Status emit_apprenticeship_fact_payloads_fields(
    ProjectionWriter &out, const CommonStore &common,
    const PreparedCatalog &catalog, const FamilyApprenticeshipState *state) {
  if (bool(catalog.family_apprenticeship) != bool(state))
    fail("ApprenticeshipProfileMismatch", "family_apprenticeship");
  if (state) {
    auto valid = out.validate_detached([&](auto &writer) {
      return emit_family_apprenticeship_fields(writer, *state);
    });
    if (!valid.ok)
      return valid;
    // 原proposals→replies→exits
    // merge_unique；不复制原记录或用显示列表重建来源。
    for (const auto &row : state->replies.rows())
      if (state->proposals.find(row.first))
        fail("DuplicateDomainPayload", row.first);
    for (const auto &row : state->exits.rows())
      if (state->proposals.find(row.first) || state->replies.find(row.first))
        fail("DuplicateDomainPayload", row.first);
  }
  std::size_t count = 0;
  for (const auto &row : common.state().facts.rows()) {
    const auto &h = row.second;
    const auto *proposal = state ? state->proposals.find(row.first) : nullptr;
    const auto *reply = state ? state->replies.find(row.first) : nullptr;
    const auto *exit = state ? state->exits.find(row.first) : nullptr;
    const bool record = proposal || reply || exit;
    const bool kind = h.kind == "family_apprenticeship_proposed" ||
                      h.kind == "family_apprenticeship_replied" ||
                      h.kind == "family_apprenticeship_exited";
    if (!record && !kind && h.domain != "family_apprenticeship")
      continue;
    if (!state || !record || !kind || h.id != row.first ||
        h.domain != "family_apprenticeship" ||
        (proposal ? proposal->kind
         : reply  ? reply->kind
                  : exit->kind) != h.kind)
      fail("ApprenticeshipFactPayloadMismatch", row.first);
    auto status =
        out.object(row.first, h.field_order, [&](auto &writer) -> Status {
          auto emitted =
              proposal ? emit_apprenticeship_proposal_fields(writer, *proposal)
              : reply  ? emit_apprenticeship_reply_fields(writer, *reply)
                       : emit_apprenticeship_exit_fields(writer, *exit);
          if (!emitted.ok)
            return emitted;
          return writer.text("domain", "family_apprenticeship");
        });
    if (!status.ok)
      return status;
    ++count;
  }
  if (count != (state ? state->proposals.size() + state->replies.size() +
                            state->exits.size()
                      : 0))
    fail("ApprenticeshipFactPayloadMismatch", "family_apprenticeship");
  return out.status();
}
Outcome<BoundaryValue>
apprenticeship_fact_payloads(const CommonStore &common,
                             const PreparedCatalog &catalog,
                             const FamilyApprenticeshipState *state) {
  return projection_value([&](auto &out) {
    return emit_apprenticeship_fact_payloads_fields(out, common, catalog,
                                                    state);
  });
}
// 生成三种新段的精确共同事实投影；孤头、孤实体、错kind/domain和同ID多实体均拒。
