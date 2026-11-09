// JSON、二进制和无树导出共用这一份字段定义，不从表内容推断选型。
#pragma once
#include "local_move_codec.hpp"
#include "movement_record_codec_fields.hpp"
#include "transport_physical_table.hpp"
namespace hanmo::resident::local_move_fields {
// 低层只核持久形状；真实前驱、原 passage 与写入顺序由完整源读口验证。
template<class C> Status record(C& c, const StableId& key, LocalMoveSource& v,
                               const MovementRecordCodecSelection& selection) {
  using movement_record_fields::field;
  field(c,"id",v.id,selection); field(c,"actor_id",v.actor_id,selection);
  field(c,"occurred_at",v.occurred_at,selection);
  field(c,"before_fact_count",v.before_prefix.common_fact_count,selection);
  field(c,"before_last_fact",v.before_prefix.last_common_fact,selection);
  field(c,"before_head",v.before_head,selection);
  field(c,"from",v.from,selection); field(c,"to",v.to,selection);
  if (!c.status().ok) return c.status();
  if(key.empty() || key!=v.id || v.actor_id.empty() || v.before_head.empty() ||
     v.before_prefix.common_fact_count<0 ||
     bool(v.before_prefix.last_common_fact)!=(v.before_prefix.common_fact_count!=0) ||
     v.from.settlement_id.empty() || v.from.place_id.empty() ||
     v.to.settlement_id.empty() || v.to.place_id.empty())
    return c.reject("InvalidLocalMoveSourceShape",key);
  return c.status();
}
// 独立 revision 只接受 /1；旧 bank 不凭空补造此段。
template<class C> Status root(C& c, OrderedTable<LocalMoveSource>& table,
    transport_physical_table::DecodedRows<LocalMoveSource>& pending,
    Integer selected,const MovementRecordCodecSelection& selection) {
  if(selected!=local_move_codec_revision) return c.reject("UnsupportedLocalMovePhysicalRevision");
  Integer revision=local_move_codec_revision; c.field("revision",revision);
  if(!c.status().ok) return c.status();
  if(revision!=local_move_codec_revision) return c.reject("UnsupportedLocalMovePhysicalRevision");
  return transport_physical_table::all(c,"local_moves",table,pending,
      [&](auto& row,const StableId& key,LocalMoveSource& v){return record(row,key,v,selection);});
}
} // namespace hanmo::resident::local_move_fields
