// Explicit V entry-array shell. It owns no business state and never deep-copies
// an existing table to sort it. Binary and JSON/projection share this shell.
#pragma once
#include "movement_record_codec_fields.hpp"
#include "transport_decoded_row_transfer.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include <algorithm>
namespace hanmo::resident::transport_physical_table {
// 仅 census 访客把原表交给唯一拥有图；编码与投影不留存，并保留访客已有首错。
template <class C, class T>
Status retain(C &codec, const OrderedTable<T> &table) {
  if constexpr (std::is_same_v<std::decay_t<C>, CodecVisitor>) {

  }
  return codec.status();
}
template<class C,class T,class A,class F>Status append_rows(C& codec,const StableId& name,std::vector<T,A>& rows,F visit){if constexpr(std::is_nothrow_move_constructible_v<T>)return codec.append_sequence(name,rows,visit);else return codec.append_sequence(name,rows,visit,transport_decoded_transfer::OriginalTransportDecodedRowTransfer{});}
template<ProjectionRecordShape Shape,class T,class A,class F>Status append_rows(ProjectionRecordArchive<Shape>& codec,const StableId& name,std::vector<T,A>&,F){return codec.reject("PhysicalDecodeOnProjectionWriter",name);}
template<class T>struct DecodedRows {CheckpointVector<std::pair<StableId,T>> values;};
// One pending typed row collection per original table, not per parent. It is
// moved into that original table once when all selected sections have decoded.
template<class C,class T,class Visit,class Filter>
Status selected(C &codec, const StableId &name, OrderedTable<T> &table,
                DecodedRows<T> &decoded, Visit visit, Filter filter) {
  if (codec.reading()) {
    const auto begin = decoded.values.size();
    auto status = append_rows(codec, name, decoded.values,
                              [&](auto &fields, std::pair<StableId, T> &row) {
                                fields.field("key", row.first);
                                return fields.object("fields", [&](auto &body) {
                                  return visit(body, row.first, row.second);
                                });
                              });
    if (!status.ok)
      return status;
    for (std::size_t n = begin; n < decoded.values.size(); ++n) {
      const auto &row = decoded.values[n];
      if (row.first.empty() ||
          (n > begin && decoded.values[n - 1].first >= row.first) ||
          !filter(row.first, row.second))
        return codec.reject("InvalidCanonicalTransportRow", name);
    }
    return codec.status();
  }
  // 留存沿唯一 owned graph，编码不进入专用 census 分派。
  if constexpr (std::is_same_v<std::decay_t<C>, CodecVisitor>) {

  }
  using Row = std::pair<StableId, T>;
  CheckpointVector<const Row *> selected_rows;
  for (const auto &row : table.rows())
    if (filter(row.first, row.second))
      selected_rows.push_back(&row);
  std::sort(selected_rows.begin(), selected_rows.end(),
            [](const Row *a, const Row *b) { return a->first < b->first; });
  return codec.sequence(
      name, selected_rows, [&](auto &fields, const Row *&row) {
        auto &key = const_cast<StableId &>(row->first);
        fields.field("key", key);
        return fields.object("fields", [&](auto &body) {
          return visit(body, key, const_cast<T &>(row->second));
        });
      });
}
template<class C,class T,class Visit>Status all(C& codec,const StableId& name,OrderedTable<T>& table,DecodedRows<T>& decoded,Visit visit){if(codec.reading()&&(!table.empty()||!decoded.values.empty()))return codec.reject("PhysicalTransportRootAlreadyPopulated",name);return selected(codec,name,table,decoded,visit,[](const StableId&,const T&){return true;});}
template<class T>Status adopt(OrderedTable<T>& table,DecodedRows<T>& decoded,const StableId& name){
 if(!table.empty())return common_failure("PhysicalTransportRootAlreadyPopulated",name);
 using Row=std::pair<StableId,T>;
 if constexpr(std::is_nothrow_move_constructible_v<Row>){if(!table.adopt_checkpoint_rows(std::move(decoded.values)))return common_failure("DuplicatePhysicalTransportOwner",name);return common_success();}
 else return table.adopt_checkpoint_rows(std::move(decoded.values),transport_decoded_transfer::OriginalTransportDecodedRowTransfer{});
}
}
