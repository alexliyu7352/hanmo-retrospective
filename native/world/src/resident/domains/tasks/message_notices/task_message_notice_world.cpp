// 仅将同一候选的真实死亡与军战亲历读接到原Task；不扫描全事实生成NPC发信意图。
#include "task_message_notice_world.hpp"
#include "resident/kernel/world_bundle.hpp"
#include "resident/domains/family/death_notice_military_read.hpp"
namespace hanmo::resident {
Outcome<std::optional<TaskMessageNoticeServices>> make_task_message_notice_services(const WorldBundle& w,const DynamicStepInput* phase){
    if(!w.catalog->task_message_notices){if(w.tasks.message_notices)return {false,{},common_failure("TaskMessageNoticeProfileMismatch").error};return {true,std::nullopt,{}};}
    if(!w.family_life_enabled()||!w.catalog->task_message_handoffs_enabled||!w.tasks.message_handoffs||!w.tasks.message_notices)return {false,{},common_failure("TaskMessageNoticeProfileMismatch").error};
    TaskMessageNoticeServices out;out.catalog=w.catalog->task_message_notices.get();out.state=&*w.tasks.message_notices;out.phase=phase;
    out.read=[&w](const StableId& sender,const StableId& recipient,const StableId& deceased,Integer tick,const DynamicStepInput* step){return read_witnessed_death_notice(w.common,w.family_life,w.family_death_read(),family_military_death_witness_read(w.common,w.military,w.naval_enabled()?&w.naval:nullptr),sender,recipient,deceased,tick,step);};
    out.validate=[&w](const WitnessedDeathNoticeSource& source,const StableId& id,const DynamicStepInput* step){return validate_witnessed_death_notice(w.common,w.family_life,w.family_death_read(),family_military_death_witness_read(w.common,w.military,w.naval_enabled()?&w.naval:nullptr),source,id,step);};
    return {true,std::move(out),{}};
}
}
