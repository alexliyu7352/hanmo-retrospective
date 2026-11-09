// 公开内容首次生产的原普通字段预费；只借原字符串，不复制私有表达或另建预算。
#pragma once
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/kernel/source_ref.hpp"
#include <array>
namespace hanmo::resident::family_content_detail {

// 只用此原固定顺序构造一次 fresh vector，避开 initializer_list
// 的第二份长字符串。
inline FieldOrder field_order(bool content2, CheckpointMemoryCharge *memory) {
  static constexpr std::array<std::string_view, 13> prefix{
      "source_ref",
      "source_schema",
      "content_kind",
      "sender_actor_id",
      "receiver_actor_id",
      "proposal_ref",
      "proposal_source_ref",
      "terms_version",
      "terms_view",
      "decision",
      "previous_proposal_ref",
      "negotiated_reply_ref",
      "expressed_at"};
  if (memory) {


    (*memory).grow(0);
  }
  FieldOrder out;
  out.reserve(content2 ? 16 : 14);
  for (auto name : prefix)
    out.emplace_back(name);
  if (content2) {
    out.emplace_back("etiquette_presentation");
    out.emplace_back("marriage_request_presentation");
  }
  out.emplace_back("field_order");
  return out;
}
} // namespace hanmo::resident::family_content_detail
