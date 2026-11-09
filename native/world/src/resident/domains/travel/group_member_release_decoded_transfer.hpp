#pragma once
#include "group_movement.hpp"
#include "transport_decoded_row_transfer.hpp"
namespace hanmo::resident::transport_decoded_transfer {
// Explicit original fallible WorldMoment transfer, including
// pair<StableId,row>. Never mark the aggregate move noexcept or substitute a
// second time format.
template <>
struct OriginalRow<GroupContactMemberReleaseSource> : std::true_type {};
} // namespace hanmo::resident::transport_decoded_transfer
