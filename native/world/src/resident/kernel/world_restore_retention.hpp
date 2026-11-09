// 原载入job保留真实旧世界；同一封闭lease既持有分配账，也界定一次暂时的写入隔离期。
#pragma once
#include "world_checkpoint_capture.hpp"
#include <optional>
namespace hanmo::resident {
class NativeBoundarySession;
class WorldRestoreRetention final {
public:
    class ConstructionKey {friend class ResidentWorld;friend class NativeBoundarySession;ConstructionKey()=default;};
    WorldRestoreRetention(ConstructionKey,CheckpointJobReservation&&,std::shared_ptr<const WorldBundle>,std::optional<WorldIdentity>)noexcept;
    WorldRestoreRetention(const WorldRestoreRetention&)=delete;
    WorldRestoreRetention& operator=(const WorldRestoreRetention&)=delete;
    const std::optional<WorldIdentity>& expected_identity()const noexcept{return expected_;}
    bool belongs_to(const CheckpointJobBudget& job)const noexcept{return retained_.belongs_to(job);}
private:
    friend class ResidentWorld;friend class NativeBoundarySession;
    // world必须先销毁，retained最后释放；外部不能提取或替换旧世界拥有者。
    CheckpointJobReservation retained_;
    std::shared_ptr<const WorldBundle> world_;
    std::optional<WorldIdentity> expected_;
};
}
