// Original message Life4 context borrows the same body owner. No P10 capture.
#pragma once
#include "visit_message_life.hpp"
#include "resident/domains/actor_foundation/character_condition_world.hpp"
#include "resident/kernel/world_read_stamp.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
namespace hanmo::resident {
class NativeVisitMessageLifeRead final:public CheckpointAllocatedObject {
 const WorldBundle&world_;const CharacterConditionWorldRead&body_owner_;
 WorldCurrentMomentRead clock_;VisitMessageLifeContext context_;
 NativeVisitMessageLifeRead(const WorldBundle&,const CharacterConditionWorldRead&,const CharacterConditionSources&);
public:
 static Outcome<std::unique_ptr<NativeVisitMessageLifeRead>>create(const WorldBundle&,const CharacterConditionWorldRead&);
 NativeVisitMessageLifeRead(const NativeVisitMessageLifeRead&)=delete;
 NativeVisitMessageLifeRead&operator=(const NativeVisitMessageLifeRead&)=delete;
 Outcome<const VisitMessageLifeContext*>context()const;
};
}
