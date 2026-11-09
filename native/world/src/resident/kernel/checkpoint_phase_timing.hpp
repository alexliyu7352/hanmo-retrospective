// 仅宿主显式打开的同步诊断窗；固定栈数组/TLS 借用，不进入世界、作者或存档。
#pragma once
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace hanmo::resident::checkpoint_phase_timing {
enum class Stage : std::size_t {
  save_validate, save_snapshot, save_producers, save_encode,
  binary_prepare, binary_measure, binary_write,
  restore_selection, restore_catalog, restore_decode, restore_construct,
  restore_validate, restore_export, restore_publish, count
};
inline constexpr std::size_t stage_count = static_cast<std::size_t>(Stage::count);
inline constexpr std::array<const char*, stage_count> names{
  "save.world_validation", "save.snapshot_clone_and_identity", "save.producer_setup", "save.encode_api",
  "save.binary_prepare", "save.binary_measure_and_hash", "save.binary_write_and_hash",
  "restore.selection_inspection", "restore.catalog_prepare", "restore.decode_envelope_and_typed",
  "restore.candidate_and_derived", "restore.world_validation", "restore.export_validation", "restore.publish_and_indexes"
};
// 每行是该阶段所有实际调用的 inclusive 总和；encode 内三子段不能再与父段相加。
struct Sample { std::uint64_t calls=0, nanoseconds=0; };
struct Frame { std::array<Sample,stage_count> samples{}; bool overflow=false; };
namespace detail { inline thread_local Frame* active=nullptr; }

// 只借调用栈的 Frame；嵌套窗按 RAII 恢复，显式 stop 只准按原栈顺序结束。
class Capture {
public:
  explicit Capture(Frame& frame) noexcept : previous_(detail::active) { detail::active=&frame; }
  ~Capture() noexcept { stop(); }
  Capture(const Capture&)=delete;
  Capture& operator=(const Capture&)=delete;
  void stop() noexcept { if(active_){detail::active=previous_;active_=false;} }
private:
  Frame* previous_;
  bool active_=true;
};

// 无窗时不读时钟；无堆分配/回调，早退和异常只记录耗时，不参与任何业务判断。
class Scope {
  using Clock=std::chrono::steady_clock;
public:
  explicit Scope(Stage stage) noexcept : frame_(detail::active), stage_(stage) {
    if(frame_)start_=Clock::now();
  }
  ~Scope() noexcept { stop(); }
  Scope(const Scope&)=delete;
  Scope& operator=(const Scope&)=delete;
  void stop() noexcept {
    if(!frame_)return;
    const auto elapsed=std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-start_).count();
    const auto amount=elapsed>0?static_cast<std::uint64_t>(elapsed):0;
    auto& sample=frame_->samples[static_cast<std::size_t>(stage_)];
    constexpr auto maximum=std::numeric_limits<std::uint64_t>::max();
    if(sample.calls==maximum)frame_->overflow=true;else ++sample.calls;
    if(amount>maximum-sample.nanoseconds){sample.nanoseconds=maximum;frame_->overflow=true;}
    else sample.nanoseconds+=amount;
    frame_=nullptr;
  }
private:
  Frame* frame_;
  Stage stage_;
  Clock::time_point start_{};
};
}
