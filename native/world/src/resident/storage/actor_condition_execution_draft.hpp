// 原Common人物写者的瞬态实录；不是永久人物条件史、业务授权或第二份可写HP。
#pragma once
#include "resident/model/common_operations.hpp"
namespace hanmo::resident {
// 捕获真实写前/写后完整四身体标量，presence另存以区分未写和同值/零值写入。
struct ActorConditionExecutionValues {Integer hp=0,fatigue=0,injury=0;bool alive=false,available=false;};
// 动态create_actor没有前像；原Installed初始化仍须独立正规初态来源。
struct ActorConditionExecutionDraft {
    enum class Kind { mutation,creation };
    WorldIdentity identity;
    Integer sequence=0;
    Kind kind=Kind::mutation;
    StableId actor_id;
    std::optional<ActorConditionExecutionValues> before;
    ActorConditionExecutionValues after;
    bool hp_written=false,fatigue_written=false,injury_written=false,alive_written=false;
    Integer next_instance_before=0,next_instance_after=0;
    bool available_written=false;
};
// 实际有效tick/业务key/课时及晚到source头由唯一原writer配对；不能从此值自证原因。
// 该片没有profile保护或永久账，未捕获的直接state写必须由后续完整方案明确拒绝。
}
