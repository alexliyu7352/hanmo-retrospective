// 同一 World 的原私人库存产权读租约；它不授予采购、付款或保管同意。
#pragma once
#include "resident/domains/estate/estate_rest_history.hpp"
#include "resident/domains/local_economy/private_inventory_title.hpp"
#include "world_read_stamp.hpp"
#include <memory>

namespace hanmo::resident {
struct WorldBundle;

// 外层 owner 必须覆盖所有借用者，clone/decode 后由新 World 重新构造。
class NativePrivateInventoryWorldRead final : public CheckpointAllocatedObject {
  const WorldBundle &world_;
  WorldCurrentMomentRead clock_;
  EstateRestHistoryContext estate_history_;
  NativeEstateAssetTitleRead estate_titles_;
  PrivateInventoryTitleContext context_;
  NativePrivateInventoryTitleRead read_;

  explicit NativePrivateInventoryWorldRead(const WorldBundle &);

public:
  NativePrivateInventoryWorldRead(const NativePrivateInventoryWorldRead &) =
      delete;
  NativePrivateInventoryWorldRead &
  operator=(const NativePrivateInventoryWorldRead &) = delete;
  NativePrivateInventoryWorldRead(NativePrivateInventoryWorldRead &&) = delete;
  NativePrivateInventoryWorldRead &
  operator=(NativePrivateInventoryWorldRead &&) = delete;
  static Outcome<std::unique_ptr<NativePrivateInventoryWorldRead>>
  create(const WorldBundle &);
  bool bound_to(const WorldBundle &) const noexcept;
  Status validate_bindings() const;
  const PrivateInventoryTitleContext &context() const noexcept {
    return context_;
  }
  const NativePrivateInventoryTitleRead &read() const noexcept { return read_; }
};
} // namespace hanmo::resident
