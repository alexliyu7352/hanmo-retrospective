// 原装备三命令的同候选选择保护；仅护raw选择，不证明物品可用量或心法生效资格。
#pragma once
#include "resident/model/common_operations.hpp"
namespace hanmo::resident {
struct ActorSelectionCaptureIdentity;
struct ActorSelectionScopeIdentity;
// 空护甲与缺护甲保不同存在性，不能以空串抹掉原卸甲写者。
struct ActorSelectionValues { StableId weapon_id,heart_id;std::optional<StableId> armor_id; };
enum class ActorSelectionCommand { equip_weapon,equip_armor,configure_heart };
// 只有Common实际begin创建身份；调用者可保留句柄但不能伪造有效新scope。
class ActorSelectionWriteHandle {
    friend class CommonStore;
    std::shared_ptr<const ActorSelectionScopeIdentity> identity_;
};
// 只记录原apply_actor成功的一个选择写回；scope/capture身份绝不进入保存格式。
struct ActorSelectionExecutionDraft {
    WorldIdentity identity;
    Integer sequence=0;
    StableId actor_id;
    ActorSelectionCommand command=ActorSelectionCommand::equip_weapon;
    ActorSelectionValues before,after;
    Integer next_instance_before=0,next_instance_after=0;
    std::shared_ptr<const ActorSelectionCaptureIdentity> capture_identity;
    std::shared_ptr<const ActorSelectionScopeIdentity> scope_identity;
    // 只有原finish成功才置真；领域消费者须同时核本Common原数组，不能信外部复制值。
    bool scope_completed=false;
};
}
