// 新CLI的有限结果编码；仅转写DTO，不读世界或形成第二关联查询。
#pragma once
#include "hanmo/legacy_projection.hpp"
namespace hanmo::detail {
Value actor_commitments_value(const std::vector<ActorCommitmentsResult>& results);
} // 整段沿唯一TaggedValue wire编码。
