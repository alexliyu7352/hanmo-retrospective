// 原知识保留只枚举真实原来源；同步借用不制造第二事实或拥有型返回前沿。
#pragma once
#include "social_time2.hpp"
namespace hanmo::resident {
struct SocialKnowledgeSourcePins {
  std::vector<SourceRef> original_sources;
  std::vector<MomentOwnerSource> exact_sources;
};
class SocialKnowledgeSourcePinSink {
public:
  virtual ~SocialKnowledgeSourcePinSink() = default;
  virtual Status original(SourceRefBuildView) = 0;
  virtual Status exact(const MomentOwnerSource &) = 0;
};
// 回调中所有字符串/精确源均借本次 Common/Social 原行；不得保留短栈 view 自身。
// 任一失败须丢弃整批输出。顺序与去重沿旧枚举，不新增业务 pin 或释放资格。
Status visit_social_knowledge_source_pins(const CommonStore &,
                                          const SocialState &,
                                          SocialKnowledgeSourcePinSink &);
// 旧裸值 ABI 兼容入口；新 owning 消费者使用上面同步 visitor 自己保活返回租约。
Outcome<SocialKnowledgeSourcePins>
social_knowledge_source_pins(const CommonStore &, const SocialState &);
} // namespace hanmo::resident
