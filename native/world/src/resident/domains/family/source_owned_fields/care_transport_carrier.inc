// Care 接受对象的两个实际值；由中央在唯一 source_owned_fields 内包含。
// 不遍历另一份船体或编组状态，不把拥有图存在当真实承运许可。

// 原 S1 引用和实际接受版本分别拥有，不能跳过未被当前分支读取的成员。
template <class Visitor>
inline void visit(const ::hanmo::resident::CareGroundTransportCarrier &value,
                  Visitor &&visitor) {
  visitor(value.transport_unit_ref);
  visitor(value.expected_composition_revision);
}

// 原 VesselRef 交 Naval 唯一拥有字段登记，不在 Family 展开一份船键模型。
template <class Visitor>
inline void visit(const ::hanmo::resident::CareActualVesselCarrier &value,
                  Visitor &&visitor) {
  visitor(value.vessel_ref);
}
