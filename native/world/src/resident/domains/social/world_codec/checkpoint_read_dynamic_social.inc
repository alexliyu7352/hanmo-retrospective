// 社交检查点恢复分派：沿原 else-if 链读取 dynamic_social,
// social_period_sources；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "dynamic_social") {
  auto selected = selected_native_dynamic_social_version(catalog);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (section.codec_version != selected.value || !selected.value || s->dynamic)
    return {false,
            {},
            common_failure("DynamicSocialCheckpointProfileMismatch", {},
                           ErrorKind::internal)
                .error};
  s->dynamic.emplace();
  r = visit_native_dynamic_social_checkpoint(reader, *s->dynamic, catalog);
}
else if (section.domain == "social_period_sources") {
  auto period = make_state<Seed, SocialPeriodSources>();
  r = visit_social_period_sources_checkpoint(reader, *period);
  seed.social_period_sources = std::move(period);
}