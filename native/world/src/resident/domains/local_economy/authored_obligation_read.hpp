// 初始责任定义只读原目录和原义务行，不建立应付、付款或今日权力。
#pragma once
#include "local_economy_model.hpp"
namespace hanmo::resident {
// 返回借用原目录行，只能在绑定同一只读 job/root
// 的上下文有效期内使用，不得持久保存。 organization_id
// 仅限定要核的原机构，不替代原权源、容器或主体校验。
Outcome<const RecurringObligation *> read_economic_authored_obligation(
    const CommonStore &, const LocalEconomyState &, const LocalEconomyCatalog &,
    const SourceRef &, const StableId &organization_id);
} // namespace hanmo::resident
