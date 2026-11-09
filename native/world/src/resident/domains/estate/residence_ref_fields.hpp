// EC53 原地址唯一字段表；调用者负责其外层字段顺序与实际来源验证。
#pragma once
#include "residence_service_refs.hpp"

namespace hanmo::resident::residence_ref_fields {
// 地址只是原行定位，不能据字段合法性授予居住权。
template <class C> Status agreement(C &c, ResidenceAgreementRef &value) {
  c.field("agreement_id", value.agreement_id);
  c.field("terms_revision", value.terms_revision);
  if (c.status().ok &&
      (value.agreement_id.empty() || value.terms_revision <= 0))
    return c.reject("InvalidResidenceAgreementRef", value.agreement_id);
  return c.status();
}
// 暂定内部 wire：agreement 原对象后接 line_id；不含第二份条款/权限。
template <class C> Status intake_line(C &c, ResidenceIntakeLineRef &value) {
  c.object("agreement",
           [&](auto &out) { return agreement(out, value.agreement); });
  c.field("line_id", value.line_id);
  if (c.status().ok && value.line_id.empty())
    return c.reject("InvalidResidenceIntakeLineRef", value.line_id);
  return c.status();
}
} // namespace hanmo::resident::residence_ref_fields
