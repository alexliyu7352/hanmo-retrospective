// 原根的持值与布局检查共用唯一字段绑定；保持原 codec 首错与 /3、/4 分支。
#pragma once
#include "military_exact_root_bindings.hpp"
namespace hanmo::resident::military_exact_state_fields {
// 职责：保留旧选版及非空 /3 所有权拒绝，再委托实际成员/回调绑定。
// 本函数不因 canonical 缺根改写旧 parser，也不补空字段冒完整格式。
template <class Archive>
Status value(Archive &c, MilitaryExactState &state, Integer selected) {
  if (selected != 2 && selected != 3 && selected != 4)
    return c.reject("MilitaryExactSelectedSchemaRequired");
  if (selected == 2 && (!state.formations.empty() || !state.missions.empty()))
    return c.reject("MilitaryTacticalOwnership3RequiresExplicitSelection");
  military_exact_root_bindings::StateArchive<Archive> bound{c, state};
  return military_exact_root_bindings::visit(bound, selected);
}
}
