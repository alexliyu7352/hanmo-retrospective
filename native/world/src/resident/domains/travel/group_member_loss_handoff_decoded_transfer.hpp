#pragma once
#include "group_member_loss_handoff.hpp"
#include "transport_decoded_row_transfer.hpp"
namespace hanmo::resident::transport_decoded_transfer {
// WorldMoment has real fallible limb transfer. The existing explicit transfer
// also covers pair<StableId,row>; never replace this with a noexcept move
// claim.
template <>
struct OriginalRow<GroupMemberLossHandoffSource> : std::true_type {};
} // namespace hanmo::resident::transport_decoded_transfer
