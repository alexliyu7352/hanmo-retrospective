// Common实际位置写tap；仅本候选拥有，永不编码、不给裸permit写权。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct ActorLocationCaptureIdentity; struct ActorLocationWriteIdentity;
struct ActorLocationExecutionDraft {
 enum class Kind { insertion,mutation }; Kind kind=Kind::mutation;
 StableId actor_id; Integer sequence=0; std::optional<Location> before; Location after; std::shared_ptr<const ActorLocationWriteIdentity> write_identity;
};
}
