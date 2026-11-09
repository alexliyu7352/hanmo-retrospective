// A live save may outlast replacement of its captured world. Keep the whole
// capture alive while charging later candidates to its original shared job.
#pragma once
#include "world_checkpoint_capture.hpp"
namespace hanmo::resident {
class WorldCheckpointWriteScope {
    std::shared_ptr<const WorldCheckpointCapture> capture_;
    ScopedCheckpointMemory memory_;
public:
    explicit WorldCheckpointWriteScope(std::shared_ptr<const WorldCheckpointCapture> capture)noexcept
        :capture_(std::move(capture)),memory_(capture_?capture_->candidate_memory():current_checkpoint_memory()){}
    WorldCheckpointWriteScope(const WorldCheckpointWriteScope&)=delete;
    WorldCheckpointWriteScope& operator=(const WorldCheckpointWriteScope&)=delete;
};
}
