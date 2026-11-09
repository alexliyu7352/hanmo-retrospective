// 所有Condition原因共用同一候选写入证据；持久收据不能自称捕获了真实Common写入。
#pragma once
#include "resident/domains/actor_foundation/actor_foundation_refs.hpp"
namespace hanmo::resident {
class CommonStore;struct ActorConditionCaptureIdentity;
struct BoundActorAvailabilityWrite {StableId transition_id;std::shared_ptr<const ActorConditionCaptureIdentity> capture_identity;Integer capture_sequence=0;};
struct BoundActorConditionCreation {StableId actor_id;InitialConditionSeedRef initial_seed_ref;std::shared_ptr<const ActorConditionCaptureIdentity> capture_identity;Integer capture_sequence=0;};
struct BoundActorConditionWrite {ConditionReceiptRefV2 receipt_ref;std::shared_ptr<const ActorConditionCaptureIdentity> capture_identity;Integer capture_sequence=0;};
// Valid only while the original same-candidate capture is live.
Outcome<const ConditionReceiptV2*> read_actual_actor_condition_write(const CommonStore&,const AuthoredActorFoundationState&,const BoundActorConditionWrite&);
Outcome<const InitialConditionSeed*> read_actual_actor_condition_creation(const CommonStore&,const AuthoredActorFoundationState&,const BoundActorConditionCreation&);
// 必须覆盖本次capture的全部写入，净零写presence也逐项核；冷读取不调用此口。
Status validate_actor_condition_capture(const CommonStore&,const AuthoredActorFoundationState&,const std::vector<BoundActorConditionWrite>&,const std::vector<BoundActorAvailabilityWrite>& availability={},const std::vector<BoundActorConditionCreation>& creations={});
}
