// 家庭检查点恢复分派：沿原 else-if 链读取 family_life, family_legacy,
// family_bequest, family_inheritance, family_marriage, family_birth,
// family_care_relief；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "family_life") r =
    catalog.family_life && catalog.family_life->selected_version == 4
        ? visit_family_life4(reader, *life)
        : visit_family_deaths_checkpoint(reader, *life);
else if (section.domain == "family_legacy") r =
    section.codec_version == 3
        ? visit_family_legacy_checkpoint_v3(reader, *legacy)
    : section.codec_version == 2
        ? visit_family_legacy_checkpoint_v2(reader, *legacy)
        : visit_family_legacy_checkpoint(reader, *legacy);
else if (section.domain == "family_bequest") r =
    visit_family_bequests_checkpoint(reader, *bequest);
else if (section.domain == "family_inheritance") r =
    visit_family_inheritance_checkpoint(reader, *inheritance);
else if (section.domain == "family_marriage") r =
    visit_family_marriage_checkpoint(reader, *marriage);
else if (section.domain == "family_birth") r =
    visit_native_family_birth(reader, *birth, section.codec_version);
else if (section.domain == "family_care_relief") r =
    visit_child_care_relief_checkpoint(reader, *care_relief);
