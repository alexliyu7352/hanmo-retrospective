// 设施占用检查点恢复分派：沿原 else-if 链读取
// facility_claims；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "facility_claims") {
  auto claims = make_state<Seed, FacilityClaimsState>();
  r = visit_facility_claims2(reader, *claims);
  seed.facility_claims = std::move(claims);
}