// 社交原 JSON 的硬上限沿同一发射器核验，不构造值树。
#pragma once
#include "resident/checkpoint/projection_writer.hpp"
#include <array>
namespace hanmo::resident::social_projection_detail {
// 只检查原字节、节点和嵌套边界；字段次序仍由正式输出 sink 核验。
class WireLimitWriter final : public ProjectionWriter {
  struct Frame {
    bool array = false;
    std::size_t count = 0, expected = 0;
  };
  std::array<Frame, CodecVisitor::max_depth> frames_{};
  std::size_t depth_ = 0, nodes_ = 0, bytes_ = 0;
  bool root_ = false;
  // 独立原段使用自己的根和硬上限；回调或边界失败原样交回父 writer。
  Status validate_branch(ValidationEmit emit, const void *value) override {
    WireLimitWriter child;
    auto result = child.object("", [&](auto &out) { return emit(out, value); });
    return result.ok ? child.finish() : result;
  }
  // 累计原字段字节，首错由 ProjectionWriter 保留。
  Status bytes(std::size_t n) {
    if (n > CodecVisitor::max_bytes - bytes_)
      return reject("CheckpointTooLarge", "social_time2_codec");
    bytes_ += n;
    return status();
  }
  // 每个值只计一次节点，并保持原集合与深度上限。
  Status value(std::string_view name) {
    if (depth_ >= CodecVisitor::max_depth)
      return reject("CheckpointTooDeep", "social_time2_codec");
    if (++nodes_ > 2000000u)
      return reject("CheckpointTooLarge", "social_time2_codec");
    if (!depth_) {
      if (root_ || !name.empty())
        return reject("InvalidProjectionRoot");
      root_ = true;
    } else {
      auto &f = frames_[depth_ - 1];
      if (++f.count > CodecVisitor::max_items)
        return reject("CheckpointCollectionTooLarge", "social_time2_codec");
      if (!f.array)
        bytes(name.size());
    }
    return bytes(1);
  }
  // 字符串按原长度收费；其它标量沿原单节点计数。
  Status scalar(std::string_view name, const Scalar &v) override {
    value(name);
    if (const auto *s = std::get_if<std::string_view>(&v))
      bytes(s->size());
    return status();
  }
  // 只入栈原对象边界，不更改 FieldOrder view 或布局。
  Status begin_object(std::string_view name,
                      ProjectionFieldOrderView) override {
    value(name);
    if (status().ok)
      frames_[depth_++] = {};
    return status();
  }
  // 关闭对象须与原栈顶匹配。
  Status end_object() override {
    if (!depth_ || frames_[depth_ - 1].array)
      return reject("InvalidProjectionBoundary");
    --depth_;
    return status();
  }
  // 本 sink 不重排字段；正式输出仍核验同一个排序 view。
  Status reorder_object(ProjectionFieldOrderView) override {
    return status();
  }
  // 数组保留声明数量并执行原集合硬上限。
  Status begin_array(std::string_view name, std::size_t count) override {
    value(name);
    if (count > CodecVisitor::max_items)
      return reject("CheckpointCollectionTooLarge", "social_time2_codec");
    if (status().ok)
      frames_[depth_++] = {true, 0, count};
    return status();
  }
  // 数组只有实际元素数等于声明数量才允许关闭。
  Status end_array() override {
    if (!depth_ || !frames_[depth_ - 1].array ||
        frames_[depth_ - 1].count != frames_[depth_ - 1].expected)
      return reject("InvalidProjectionBoundary");
    --depth_;
    return status();
  }

public:
  // 输出必须有唯一根且所有边界已闭合；不覆盖原失败。
  Status finish() override {
    if (!root_ || depth_)
      return reject("InvalidProjectionBoundary");
    return status();
  }
};
} // namespace hanmo::resident::social_projection_detail
