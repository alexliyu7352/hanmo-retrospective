// 原政治三个物理根只搬实际已解码行；不修改 wire 或借未知类型回退深拷。
#pragma once
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "public_market/public_market_authority.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include "treaty/treaty_terms_state.hpp"

namespace hanmo::resident::political_original_root_codec {
// 只有下列原根拥有行包含可抛精确时刻。相同候选的 table 先预留真实壳，
// 本转移仅逐行搬移；精确数仍由自身 allocator 计费，失败丢弃整个候选。
template <class T>
inline constexpr bool exact_root_row =
    std::is_same_v<T, PublicMarketAuthorityRecord> ||
    std::is_same_v<T, TreatyOfferSource> ||
    std::is_same_v<T, TreatyCounterofferSource> ||
    std::is_same_v<T, TreatyMandateSource> ||
    std::is_same_v<T, TreatyPresentationSource> ||
    std::is_same_v<T, TreatyReplyRecord> ||
    std::is_same_v<T, TreatySignatureSource> ||
    std::is_same_v<T, TreatyActivationSource> ||
    std::is_same_v<T, TreatyTerminationSource>;
struct ExactRootRowTransfer {
  template <class Destination, class T>
  Status operator()(Destination &destination,
                    std::pair<StableId, T> &row) const {
    static_assert(exact_root_row<T>,
                  "Only original political root rows may use this transfer");
    destination.emplace_back(std::move(row));
    return common_success();
  }
};
// 只读投影保持原借值，不为序列化复制另一状态；读侧不隐式扩大可转移类型。
template <class T, class F>
Status table(CodecVisitor &codec, const StableId &name, OrderedTable<T> &rows,
             F fields) {
  static_assert(exact_root_row<T>);
  return common_checkpoint::table(codec, name, rows, std::move(fields),
                                  ExactRootRowTransfer{});
}
template <class T, class F>
Status table(ActorFoundationJsonVisitor &codec, const StableId &name,
             OrderedTable<T> &rows, F fields) {
  static_assert(exact_root_row<T>);
  return codec.table(name, rows, std::move(fields), ExactRootRowTransfer{});
}
template <class T, class F>
Status table(ActorFoundationProjectionArchive &codec, const StableId &name,
             OrderedTable<T> &rows, F fields) {
  static_assert(exact_root_row<T>);
  return codec.table(name, rows, std::move(fields));
}
} // namespace hanmo::resident::political_original_root_codec
