// 原开店资格完整读的保留投影；不复制许可/客户/商术规则，不授释放权。
#pragma once
#include "private_shop.hpp"
namespace hanmo::resident {
struct PoliticalShopLicensingContext;
// 任一 callback 失败，调用方丢弃整批 pin；部分输出不准发布。
// 所有 callback 同步借实际只读根。不得保存短栈 context/request/boundary；
// 如需持久 pin，接收方先核同 World 表址后保留真实根/行身份。whole roots
// 包含已检查的排除项和后缀，不是只成功三客户、已计 XP 或选中的许可。
class PrivateShopOpeningProvenanceSink {
public:
  virtual ~PrivateShopOpeningProvenanceSink() = default;
  virtual Status opening(const PrivateShopContext &,
                         const PrivateShopRecord &) = 0;
  virtual Status growth_study(const GrowthState &,
                              const GrowthSkillOriginContext *) = 0;
  virtual Status task_source(const TaskFulfilledClientContext &,
                             const TaskFulfilledClientCheckedSource &) = 0;
  virtual Status task_clients(const TaskFulfilledClientContext &,
                              const StableId &actor,
                              const TaskFulfilledClientBoundary &) = 0;
  virtual Status permission(const PoliticalShopLicensingContext &,
                            const PrivateShopPermissionRequest &,
                            const StableId &actual_opening,
                            Integer opened_tick) = 0;
};
// 仅 actual private_shops/private_shop_opened 原槽；schema 沿原 owner 为 1，
// 本口不自造 SourceRef 或把整个 checked root 宣称为某个事实版本。
Status visit_original_private_shop_opening_provenance(
    const PrivateShopContext &, const SourceRef &,
    PrivateShopOpeningProvenanceSink &);
} // namespace hanmo::resident
