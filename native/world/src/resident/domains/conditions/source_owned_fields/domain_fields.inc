// 职责：集中条件结果中的有限拥有字段访问；仅借用原字段，不改变顺序或失败行为。
// 由中央登记头在 source_owned_fields
// 命名空间内包含；依赖与具名助手已先行可见。

template <class Visitor>
inline void visit(const ::hanmo::resident::Condition &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.flag);
  visitor(value.actor_id);
  visitor(value.place_id);
  visitor(value.offer_id);
  visitor(value.state);
  visitor(value.from_actor);
  visitor(value.to_actor);
  visitor(value.equals);
  visitor(value.trust);
  visitor(value.terms);
}
