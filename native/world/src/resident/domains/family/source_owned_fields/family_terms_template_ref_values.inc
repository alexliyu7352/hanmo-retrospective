// 原共享静态地址的唯一拥有成员；不带业务表或发布门。
#pragma once
// FamilyTermsTemplateRef 的实际拥有成员同原clone/retention共用。
template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyTermsTemplateRef &value,
                  Visitor &&visitor) {
  visitor(value.template_id);
  visitor(value.version);
}
