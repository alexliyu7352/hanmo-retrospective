// 流口只传借用字节；调用同步完成才可复用chunk，不持有可变世界。
#pragma once
#include "hanmo/resident/model_contract.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
namespace hanmo::resident {
using CheckpointDigest=std::array<std::uint8_t,32>;
// write必须全收或失败；失败后的临时输出不得被提升为可见存档。
class CheckpointWriteSink {
public:
    virtual ~CheckpointWriteSink()=default;
    virtual Status write(const std::uint8_t*,std::size_t)=0;
};
// source必须由调用者保持不变(私有冻结staging/稳定fd)；read精确读指定长度。
class CheckpointReadSource {
public:
    virtual ~CheckpointReadSource()=default;
    virtual std::uint64_t size() const=0;
    virtual Status read(std::uint64_t offset,std::uint8_t*,std::size_t)=0;
};
// 仅报告完整字节和摘要，不代表fsync或槽可见/耐久回执。
// envelope摘要只覆盖末32之前；HMCB2/槽descriptor必须使用包含末32的full摘要。
struct CheckpointStreamReceipt { std::uint64_t total_bytes=0; CheckpointDigest envelope_sha256{},full_checkpoint_sha256{}; };
} // 文件生命周期、取消和原子替换仍由原K9介质服务负责。
