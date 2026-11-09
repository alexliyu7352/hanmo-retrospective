// 期限只从同候选真实新俘获落账，读者不接受调用方伪造的 casualty 或目录缺省值。
#pragma once
#include "named_military_custody.hpp"
namespace hanmo::resident {
Outcome<bool> selected_named_military_custody(const CatalogIdentity &);
Status validate_named_military_custody_policy(const NamedMilitaryCustodyContext &);
Outcome<const NamedMilitaryCustodyTerm *> read_original_named_military_custody_term(
    const NamedMilitaryCustodyContext &, const SourceRef &);
// 含原 Common 头与实际新捕获的双向覆盖；不能删掉 term 后借空表过冷门。
Status validate_named_military_custody_terms(const NamedMilitaryCustodyContext &);
// 仅同一未发布 World 候选调用；失败由调用者丢弃整候选，不局部提交。
// 幂等返回该收据既有期限身份，Naval 已有俘虏转手只续用原期限。
Outcome<std::vector<StableId>> record_named_military_custody_terms_in_candidate(
    CommonStore &, MilitaryState &, const NamedMilitaryCustodyContext &,
    const MilitaryCasualtyFact &);
} // namespace hanmo::resident
