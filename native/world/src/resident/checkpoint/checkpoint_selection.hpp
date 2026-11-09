// 有界封套选择：先验摘要，再由身份选择一份完整域表；不发布世界。
#pragma once
#include "checkpoint_envelope.hpp"
namespace hanmo::resident {
using CheckpointDomainSelector = std::function<Outcome<std::vector<CheckpointDomain>>(const CheckpointIdentity&)>;
Outcome<CheckpointEnvelope> inspect_checkpoint_selected(const CodecVisitor::Buffer&,const CheckpointDomainSelector&);
// Resident 的格式规则仍由中央定义；返回唯一顺序，不枚举能力组合。
Outcome<std::vector<CheckpointDomain>> checkpoint_domains_from_identity(const CheckpointIdentity&);
// /2先验实际长度/全摘要/目录/段摘要，再调用原core精确目录选择器。
Outcome<CheckpointStreamIndex> inspect_checkpoint_v2_stream(CheckpointReadSource&,const CheckpointDomainSelector&,const CapacityProfile&);
// 小档兼容仅在完整容器核验后复制payload，受总输入/candidate/scratch预算约束。
Outcome<CheckpointEnvelope> inspect_checkpoint_v2_selected(const CodecVisitor::Buffer&,const CheckpointDomainSelector&,const CapacityProfile&);
// 显式同job只接已经确定的受信有限域表；任意selector回调内存不纳入本叶保证。
CheckpointJobResult<CheckpointJobStreamIndex> inspect_checkpoint_v2_stream(CheckpointReadSource&,const std::vector<CheckpointDomain>&,const CapacityProfile&,CheckpointJobBudget&);
CheckpointJobResult<CheckpointJobEnvelope> inspect_checkpoint_v2_selected(const CheckpointJobBytes&,const std::vector<CheckpointDomain>&,const CapacityProfile&,CheckpointJobBudget&);
}
