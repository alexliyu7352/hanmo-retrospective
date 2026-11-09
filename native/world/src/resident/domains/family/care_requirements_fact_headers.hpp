// Care
// 原拥有payload与Common轻头的唯一桥；不复制payload，不把头覆盖当业务来源通过。
#pragma once
#include "resident/kernel/source_ref.hpp"
#include "resident/model/common_model.hpp"
#include <string_view>
namespace hanmo::resident {
// 仅供延后到 WorldBundle 末验的严格轻头 parser；不放行未知 Care kind。
// 本判定不含 payload/schema/来源许可，不可用于独立 Common 解码成功。
bool owns_care_requirements_fact_kind(std::string_view) noexcept;
class CommonStore;
struct CareRequirementsState;
struct PreparedCatalog;
struct CareRequirementsFactHeaderView {
  const FactRecord *header = nullptr;
  Integer source_schema = 0;
};
class CareRequirementsFactHeaderVisitor {
public:
  virtual ~CareRequirementsFactHeaderVisitor() = default;
  // 每个实际拥有payload恰回调一次；跨域统一registry仍负责全局ID唯一性。
  virtual Status original_care_fact(const CareRequirementsFactHeaderView &) = 0;
};
// 同catalog形状与每个原Common头先核；transition/event和period/source各只一个头。
// 不为binding、maturity、旧schema1教育、内部期间/边界编造Common头。
// callback应沿共享registry原唯一插入口；失败立即中止，不发布半成品。
Status visit_care_requirements_fact_headers(
    const CommonStore &, const CareRequirementsState &, const PreparedCatalog &,
    CareRequirementsFactHeaderVisitor &);
// 仅返回确切真实原头及原source版本；重复拥有ID、未知kind/schema或缺payload拒绝。
// 不是完整业务reader：实际权限、来源因果与PIN必须另由原各owner末验。
Outcome<CareRequirementsFactHeaderView>
read_care_requirements_fact_header(const CommonStore &,
                                   const CareRequirementsState &,
                                   const PreparedCatalog &, const SourceRef &);
// 已持原 Common 头的共享读者仅借地址；拒绝同值副本，不复制三个来源字符串。
Outcome<CareRequirementsFactHeaderView>
read_care_requirements_fact_header(const CommonStore &,
                                   const CareRequirementsState &,
                                   const PreparedCatalog &, const FactRecord &);
// 带expected的消费者沿上方唯一实际分支反读后比较，不另写kind/schema名单。
Outcome<CareRequirementsFactHeaderView> read_care_requirements_fact_header(
    const CommonStore &, const CareRequirementsState &, const PreparedCatalog &,
    const SourceRef &, Integer actual_source_schema);
} // namespace hanmo::resident
