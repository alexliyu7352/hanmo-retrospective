// 冷许可字节明确绑定同一世界payload摘要，不能交换两份同tick分支的许可上下文。
#pragma once
#include "continuation_snapshot.hpp"
#include "resident/checkpoint/checkpoint_capacity.hpp"
#include "resident/checkpoint/checkpoint_stream.hpp"
namespace hanmo::resident {
Outcome<Bytes> bind_continuation_checkpoint(const ContinuationSnapshot&,const Bytes& checkpoint);
Outcome<ContinuationSnapshot> unbind_continuation_checkpoint(const Bytes&,const Bytes& checkpoint);
// digest必须是完整checkpoint含内部trailer的SHA256，不可传envelope_sha256。
// HMCB2只绑定HMCN2；实际受信槽预算扣除45字节wrapper后交原typed快照codec。
Outcome<Bytes> bind_continuation_checkpoint_v2(const ContinuationSnapshot&,const CheckpointDigest&,const SlotCapacityProfile&);
Outcome<ContinuationSnapshot> unbind_continuation_checkpoint_v2(const Bytes&,const CheckpointDigest&,const SlotCapacityProfile&);
// 小Bytes便捷口复用同一raw摘要绑定，不能自行扩大生产槽transport预算。
Outcome<Bytes> bind_continuation_checkpoint(const ContinuationSnapshot&,const Bytes&,const SlotCapacityProfile&);
Outcome<ContinuationSnapshot> unbind_continuation_checkpoint(const Bytes&,const Bytes&,const SlotCapacityProfile&);
// 只对已由同job拥有的HMCN2字节处理外壳，不宣称旧typed HMCN编码已经前置共账。
CheckpointJobResult<CheckpointJobBytes> bind_encoded_continuation_checkpoint_v2(const CheckpointJobBytes&,const CheckpointDigest&,const SlotCapacityProfile&,CheckpointJobBudget&);
CheckpointJobResult<CheckpointJobBytes> unbind_encoded_continuation_checkpoint_v2(const CheckpointJobBytes&,const CheckpointDigest&,const SlotCapacityProfile&,CheckpointJobBudget&);
}
namespace hanmo::resident {
// 同一个capture job的typed HMCN2及HMCB2；完整checkpoint摘要由实际stream receipt提供。
CheckpointJobResult<CheckpointJobBytes> bind_continuation_checkpoint_v2(const ContinuationSnapshot&,const CheckpointDigest&,const CapacityProfile&,const SlotCapacityProfile&,CheckpointJobBudget&);
// 私有候选读完整HMCB2，实际读取字节同时核descriptor摘要，禁止header/leaf ABA换包。
CheckpointJobStatus decode_bound_continuation_into_candidate(CheckpointReadSource&,std::uint64_t begin,std::uint64_t length,const CheckpointDigest& checkpoint_sha256,const CheckpointDigest& continuation_sha256,const CapacityProfile&,const SlotCapacityProfile&,CodecDecodeBudget&,CheckpointJobBudget&,ContinuationSnapshot&);
}
