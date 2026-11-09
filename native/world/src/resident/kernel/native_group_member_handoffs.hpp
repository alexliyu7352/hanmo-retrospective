// 原群体退出／死俘交接的两个显式物理附项；不改旧 group 4/5/6 字段。
#pragma once
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/travel/group_member_release_codec.hpp"
#include "resident/domains/travel/group_member_loss_handoff_codec.hpp"
namespace hanmo::resident {
struct PreparedCatalog;
struct WorldBundle;
struct CommonState;
// 目录中的选择不由表是否为空或当前角色状态推定。
struct NativeGroupMemberHandoffVersions {
  Integer releases = 0;
  Integer losses = 0;
};
Outcome<NativeGroupMemberHandoffVersions>
selected_native_group_member_handoff_versions(const CatalogIdentity &);
Status append_native_group_member_handoff_modules(const BoundaryValue &scenario,
                                                  CatalogIdentity &);
Status validate_native_group_member_handoff_catalog(const PreparedCatalog &);
Status validate_native_group_member_handoff_layout(const WorldBundle &);
// 这里只反核原 Common 头与原表的一一身份；不授予死亡、俘虏或位置写权。
Status validate_native_group_member_handoff_headers(
    const CommonState &, const CatalogIdentity &,
    const GroupMovementExecutionState *);
Outcome<MovementRecordCodecSelection>
native_group_member_handoff_codec_selection(const PreparedCatalog &);
} // namespace hanmo::resident
