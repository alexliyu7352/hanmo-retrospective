// 只在显式 Shop /3 的原 shops
// 行追加真实时刻；旧两版逐字段字节不变，无镜像时间表。
#pragma once
#include "private_shop.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
namespace hanmo::resident::dynamic_economy_codec {
template <class A> void record(A &, PrivateShopRecord &);
struct PrivateShopDecodedRowTransfer3 {
  template <class Destination>
  Status operator()(Destination &destination,
                    std::pair<StableId, PrivateShopRecord> &row) const {
    destination.emplace_back(std::move(row));
    return common_success();
  }
};
inline void private_shop_records(Projector &a,
                                 OrderedTable<PrivateShopRecord> &rows,
                                 Integer version) {
  if (version != 3) {
    a("shops", rows);
    return;
  }
  checked(a.writer.keyed_dictionary("shops", {}, [&](auto &w) {
    for (const auto &entry : rows.rows()) {
      checked(w.object(entry.first, [&](auto &child) {
        Projector fields{child};
        auto &row = const_cast<PrivateShopRecord &>(entry.second);
        record(fields, row);
        if (!row.opened_at)
          return child.reject("PrivateShopOpeningMomentRequired", row.id);
        fields("opened_at", *row.opened_at);
        return child.status();
      }));
    }
    return w.status();
  }));
}
inline void private_shop_records(Checkpointer &a,
                                 OrderedTable<PrivateShopRecord> &rows,
                                 Integer version) {
  if (version != 3) {
    a("shops", rows);
    return;
  }
  checked(common_checkpoint::table(
      a.codec, "shops", rows,
      [&](auto &codec, auto &row) {
        return codec.object("record", [&](auto &child) {
          Checkpointer fields{child};
          record(fields, row);
          if (child.reading())
            row.opened_at.emplace();
          if (!row.opened_at)
            return child.reject("PrivateShopOpeningMomentRequired", row.id);
          fields("opened_at", *row.opened_at);
          return child.status();
        });
      },
      PrivateShopDecodedRowTransfer3{}));
}
inline void private_shop_records(Decoder &a,
                                 OrderedTable<PrivateShopRecord> &rows,
                                 Integer version) {
  if (version != 3) {
    a("shops", rows);
    return;
  }
  const auto &object =
      boundary::object(boundary::field(a.input, "shops", a.path), a.path);
  if (object.size() > CodecVisitor::max_items)
    boundary::fail("CheckpointCollectionTooLarge", a.path);
  std::size_t index = 0;
  checked(a.archive.table(
      "shops", rows,
      [&](auto &child, auto &row) {
        const auto position = index++;
        for (std::size_t i = 0; i < position; ++i)
          if (object[i].first == object[position].first)
            boundary::fail("DuplicateDynamicRecord", a.path);
        checked(child.defer_duplicate_field_check_to_finish());
        const auto path = checkpoint_decode_boundary_join(
            {a.path, "/shops/", object[position].first});
        Decoder fields{child, object[position].second, path};
        record(fields, row);
        row.opened_at.emplace();
        fields("opened_at", *row.opened_at);
        fields.finish();
        return child.status();
      },
      PrivateShopDecodedRowTransfer3{}));
}
} // namespace hanmo::resident::dynamic_economy_codec
