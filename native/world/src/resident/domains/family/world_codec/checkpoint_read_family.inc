// 家庭检查点恢复分派：沿原 else-if 链读取 family,
// family_voluntary_paths；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "family") r = visit_family_checkpoint_versioned(
    reader, *family, section.codec_version, catalog.common->extended_skills);
else if (section.domain == "family_voluntary_paths") {
  auto paths = make_state<Seed, VoluntaryPathState>();
  r = visit_voluntary_path_checkpoint(reader, *paths);
  seed.family_voluntary_paths = std::move(paths);
}