// 不可变 Care 选择的真实拥有字段；lease由原 retain credit 单独处理。
// 物理版本不在此值中，不能借字段访问追加第二 selector。
template <class Visitor>
inline void visit(const ::hanmo::resident::CareRootCapabilitySelection &value,
                  Visitor &&visitor) {
  visitor(value.catalog_identity());
  visitor(value.scenario_id());
  visitor(value.family_education_selected());
}
