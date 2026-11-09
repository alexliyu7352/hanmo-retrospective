// 家庭世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto family = decode_family_seed_versioned(source, family_version_for(catalog),
                                           catalog.common->extended_skills,
                                           family_birth_enabled(catalog));
if (!family.ok)
  return {false, {}, family.error};
result.family = std::make_shared<const FamilyState>(std::move(family.value));
if (family_life_enabled(catalog)) {
  auto life = catalog.family_life->selected_version == 4
                  ? decode_family_life4(field(source, "family_life", "state"))
                  : decode_family_deaths(field(source, "family_life", "state"));
  if (!life.ok)
    return {false, {}, life.error};
  result.family_life =
      std::make_shared<const FamilyDeathState>(std::move(life.value));
} else if (source.find("family_life"))
  fail("UnexpectedFamilyLifeState", "family_life");
if (family_legacy_enabled(catalog)) {
  auto legacy =
      catalog.family_legacy->version == 3
          ? decode_family_legacy_v3(field(source, "family_legacy", "state"))
      : catalog.family_legacy->version == 2
          ? decode_family_legacy_v2(field(source, "family_legacy", "state"))
          : decode_family_legacy(field(source, "family_legacy", "state"));
  if (!legacy.ok)
    return {false, {}, legacy.error};
  result.family_legacy =
      std::make_shared<const FamilyLegacyState>(std::move(legacy.value));
} else if (source.find("family_legacy"))
  fail("UnexpectedFamilyLegacyState", "family_legacy");
if (family_bequest_enabled(catalog)) {
  auto bequest =
      decode_family_bequests(field(source, "family_bequest", "state"));
  if (!bequest.ok)
    return {false, {}, bequest.error};
  result.family_bequest =
      std::make_shared<const FamilyBequestState>(std::move(bequest.value));
} else if (source.find("family_bequest"))
  fail("UnexpectedFamilyBequestState", "family_bequest");
if (family_inheritance_enabled(catalog)) {
  auto inherited =
      decode_family_inheritance(field(source, "family_inheritance", "state"));
  if (!inherited.ok)
    return {false, {}, inherited.error};
  result.family_inheritance = std::make_shared<const FamilyInheritanceState>(
      std::move(inherited.value));
} else if (source.find("family_inheritance"))
  fail("UnexpectedFamilyInheritanceState", "family_inheritance");
if (family_marriage_enabled(catalog)) {
  auto state =
      decode_family_marriage(field(source, "family_marriage", "state"));
  if (!state.ok)
    return {false, {}, state.error};
  result.family_marriage =
      std::make_shared<const FamilyMarriageState>(std::move(state.value));
} else if (source.find("family_marriage"))
  fail("UnexpectedFamilyMarriageState", "family_marriage");
if (family_birth_enabled(catalog)) {
  auto selected = selected_native_family_birth_version(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (selected.value == 4) {
    ScopedCheckpointJsonDecode original(
        CheckpointWorldJsonSection::family_birth);
    original.default_values<FamilyBirthState>();
    auto birth =
        decode_native_family_birth(field(source, "family_birth", "state"), 4);
    if (!birth.ok)
      return {false, {}, std::move(birth.error)};
    result.family_birth =
        checkpoint_make_shared<const FamilyBirthState>(std::move(birth.value));
  } else {
    auto birth = decode_family_birth(field(source, "family_birth", "state"));
    if (!birth.ok)
      return {false, {}, birth.error};
    result.family_birth =
        std::make_shared<const FamilyBirthState>(std::move(birth.value));
  }
} else if (source.find("family_birth"))
  fail("UnexpectedFamilyBirthState", "family_birth");
if (family_care_relief_enabled(catalog)) {
  auto care =
      decode_child_care_relief(field(source, "family_care_relief", "state"));
  if (!care.ok)
    return {false, {}, care.error};
  result.child_care_relief =
      std::make_shared<const ChildCareReliefState>(std::move(care.value));
} else if (source.find("family_care_relief"))
  fail("UnexpectedChildCareReliefState", "family_care_relief");
