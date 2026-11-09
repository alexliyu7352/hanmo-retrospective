// S3 原真实 NativePlace 短表达的有限身体目的登记；不新增身体规则。
#pragma once
#include "resident/kernel/source_ref.hpp"
namespace hanmo::resident {
struct ActorFoundationCatalog;
// 仅 communication_links/2 + actor_start_context/3 +
// director_family_autonomy/1， 借原 character_expression
// 已锁规则；重复相同登记幂等、冲突与缺真实原 binding 均拒绝。
Status register_original_family_communication_participant_purpose(
    ActorFoundationCatalog &);
} // namespace hanmo::resident
