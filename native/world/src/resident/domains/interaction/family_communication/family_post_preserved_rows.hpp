// 只比较同一命令候选的原持久字段，不复制四根或物化 JSON。
#pragma once
#include "family_communication_links2.hpp"
namespace hanmo::resident {
Status validate_family_post_preserved_rows1(
    const FamilyCommunicationLinksState2 &before,
    const FamilyCommunicationLinksState2 &after,
    const StableId *new_expression = nullptr,
    const StableId *new_post = nullptr);
} // namespace hanmo::resident
