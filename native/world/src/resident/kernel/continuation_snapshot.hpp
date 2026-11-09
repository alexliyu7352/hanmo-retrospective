// 有限许可只保存小型执行上下文；物理存档负责将此字节串绑定原世界payload摘要。
#pragma once
#include "hanmo/resident_world_api.hpp"
#include "resident/checkpoint/checkpoint_capacity.hpp"
namespace hanmo::resident {
struct WorldBundle;
inline constexpr std::size_t continuation_snapshot_byte_limit=2048;
// 当前受信工程部署技术预算；不是文件自报权限，也不扩大旧HMCN1的2048界。
inline constexpr std::size_t exact_continuation_snapshot_byte_limit=1024u*1024u;
// 仅承认当前三种已接受行动，不隐式接入其他工作或战斗。
bool bounded_continuation_kind(const StableId&);
// 冷恢复先验许可本身的边界，再核同一世界、玩家、行动和私人钱包。
Status validate_continuation_shape(const ContinuationSnapshot&);
Status validate_continuation_snapshot(const WorldBundle&,const ContinuationSnapshot&);
// 有限二进制格式拒绝尾随字节、超长ID、负预算及未知停止码。
Outcome<Bytes> encode_continuation_snapshot(const ContinuationSnapshot&);
Outcome<ContinuationSnapshot> decode_continuation_snapshot(const Bytes&);
// 受信外层按自身完整预算传入；旧HMCN1仍同时受原2048限制，文件不能自授更大限。
Outcome<Bytes> encode_continuation_snapshot(const ContinuationSnapshot&,std::size_t trusted_byte_limit);
Outcome<ContinuationSnapshot> decode_continuation_snapshot(const Bytes&,std::size_t trusted_byte_limit);
// 同capture/continuation job两遍有界写；与世界流编码使用同一个显式预算。
CheckpointJobResult<CheckpointJobBytes> encode_continuation_snapshot(const ContinuationSnapshot&,const CapacityProfile&,const SlotCapacityProfile&,CheckpointJobBudget&);
CheckpointJobStatus decode_continuation_snapshot_into_candidate(CheckpointReadSource&,std::uint64_t begin,std::uint64_t length,const CapacityProfile&,const SlotCapacityProfile&,CodecDecodeBudget&,CheckpointJobBudget&,ContinuationSnapshot& private_candidate);
// 完整profile的调用者必须保留技术失败；此接口从同WorldBundle取得拥有型原owner适配器。
Outcome<RunStop> read_original_continuation_stop(const WorldBundle&,const ContinuationSnapshot&);
// 旧只读兼容投影；新run/恢复使用上述Outcome接口，不将技术拒绝伪装成领域停点。
RunStop continuation_domain_stop(const WorldBundle&,const ContinuationSnapshot&);
} // 快照不包含run ID或permit序号，恢复不能继承旧执行许可。
