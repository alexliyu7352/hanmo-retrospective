// 家庭检查点恢复分派：沿原 else-if 链读取 control_continuation,
// life_age_checks；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "control_continuation") {
  if (!catalog.control_continuation ||
      section.codec_version != catalog.control_continuation->selected_version ||
      seed.control_continuation)
    return {
        false,
        {},
        common_failure("ControlContinuationCheckpointProfileMismatch").error};
  auto state = make_state<Seed, ControlContinuationState>();
  r = section.codec_version == 3
          ? visit_control_continuation_v3(reader, *state)
          : visit_control_continuation_v2(reader, *state);
  seed.control_continuation = std::move(state);
}
else if (section.domain == "life_age_checks") {
  if (!catalog.life_age_checks || section.codec_version != 1 ||
      seed.life_age_checks)
    return {
        false, {}, common_failure("LifeAgeCheckpointProfileMismatch").error};
  auto state = make_state<Seed, LifeAgeState>();
  r = visit_life_age_checks(reader, *state);
  seed.life_age_checks = std::move(state);
}