// 候选独有的待绑定原开工证明；没有公开构造或可伪造的布尔豁免。
#pragma once
#include "hanmo/resident/model_contract.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include <memory>
namespace hanmo::resident {
class CommonStore;
class CompleteExactCommandCapture;
struct GrowthState;
struct CharacterConditionSources;
class GrowthPendingStartedProof final : public CheckpointAllocatedObject {
  friend class CompleteExactCommandCapture;
  struct State;
  std::unique_ptr<State> state_;
  explicit GrowthPendingStartedProof(std::unique_ptr<State>);
public:
  ~GrowthPendingStartedProof();
  GrowthPendingStartedProof(const GrowthPendingStartedProof&) = delete;
  GrowthPendingStartedProof& operator=(const GrowthPendingStartedProof&) = delete;
  // 仅原消费者借同候选证明；旧行动和已绑定行动永远不获得豁免。
  Outcome<bool> permits(const CommonStore&, const GrowthState&,
                        const CharacterConditionSources&, const StableId&) const;
};
}
