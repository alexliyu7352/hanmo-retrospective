// 政治检查点恢复分派：沿原 else-if 链读取
// political_fiscal；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "political_fiscal") {
  auto selected =
      read_political_locked_body_version(catalog.identity, "political_fiscal");
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (!selected.value || section.codec_version != selected.value ||
      !political_fiscal_enabled(catalog) ||
      selected.value != catalog.political_fiscal->version)
    return {false,
            {},
            common_failure("PoliticalFiscalCheckpointSelectionMismatch").error};
  r = selected.value == 2 ? visit_political_fiscal_checkpoint2(reader, *fiscal)
                          : visit_political_fiscal_checkpoint(reader, *fiscal);
}
