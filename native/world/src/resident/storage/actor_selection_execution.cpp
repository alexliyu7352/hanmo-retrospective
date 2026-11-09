// 原装备选择的唯一短栈写入许可与实录；不生成永久事实、业务结果或库存授权。
#include "common_store.hpp"
#include <limits>
namespace hanmo::resident {
struct ActorSelectionCaptureIdentity {};
struct ActorSelectionScopeIdentity {};
namespace {
// 只投影raw三字段，learned仍由原成长写者独立维护。
ActorSelectionValues values(const ActorCombat& c){return {c.weapon_id,c.heart_id,c.armor_id};}
// armor可选存在位参与精确相等，空字串不是无字段。
bool equal(const ActorSelectionValues& a,const ActorSelectionValues& b){return a.weapon_id==b.weapon_id&&a.heart_id==b.heart_id&&a.armor_id==b.armor_id;}
// 同提交epoch/revision/tick必须稳定，后续半日不能借原命令scope。
bool same_identity(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
// 装备命令只允许原combat替换，不能借scope夹带另一人物字段写入。
bool selection_only(const ActorMutation& m){return !m.location&&!m.action_id&&!m.alive&&!m.available&&!m.hp&&!m.fatigue&&!m.merit&&!m.injury&&!m.skills&&m.location_mode==ActorMutation::LocationMode::replace;}
}
// 只在保存点之外开唯一捕获；原未启用实例保持原行为。
Status CommonStore::begin_actor_selection_capture(){if(actor_selection_capture_.active||savepoint_)return common_failure("ActorSelectionCaptureBusy",{},ErrorKind::internal);actor_selection_capture_.identity=std::make_shared<ActorSelectionCaptureIdentity>();actor_selection_capture_.active=true;return common_success();}
// 草稿借用只读到本次捕获结束，不允许外部追加虚假写者。
const std::vector<ActorSelectionExecutionDraft>& CommonStore::actor_selection_execution_drafts() const{return actor_selection_capture_.drafts;}
// 持有原不透明身份，防止关闭重开或同址候选混用。
std::shared_ptr<const ActorSelectionCaptureIdentity> CommonStore::actor_selection_capture_identity()const{return actor_selection_capture_.identity;}
// 原scope固定真实前像，后续任何同人插入写都必须精确回到该命令的唯一合法后像。
Outcome<ActorSelectionWriteHandle> CommonStore::begin_actor_selection_write(const StableId& actor_id,ActorSelectionCommand command,const StableId& selected){
    const auto* actor=state_.actors.find(actor_id);
    if(!actor_selection_capture_.active||!actor_selection_capture_.identity||actor_selection_capture_.scope)return {false,{},common_failure("ActorSelectionScopeBusy",actor_id).error};
    if(!actor)return {false,{},common_failure("UnknownActor",actor_id).error};
    auto expected=values(actor->combat);
    switch(command){case ActorSelectionCommand::equip_weapon:expected.weapon_id=selected;break;case ActorSelectionCommand::equip_armor:expected.armor_id=selected;break;case ActorSelectionCommand::configure_heart:expected.heart_id=selected;break;default:return {false,{},common_failure("ActorSelectionCommandRequired",actor_id).error};}
    ActorSelectionScope scope;scope.identity=std::make_shared<ActorSelectionScopeIdentity>();scope.actor_id=actor_id;scope.command=command;scope.before=actor->combat;scope.world_before=state_.identity;scope.next_instance_before=state_.next_instance_id;scope.expected=std::move(expected);ActorSelectionWriteHandle handle;handle.identity_=scope.identity;actor_selection_capture_.scope=std::move(scope);return {true,std::move(handle),{}};
}
// 写前逐字段核单一命令与原前像，未知选择变化不得先写任何人物字段。
Outcome<std::optional<ActorSelectionExecutionDraft>> CommonStore::prepare_actor_selection_write(const ActorMutation& m,const ActorRecord& actor)const{
    if(!m.combat)return {true,std::nullopt,{}};
    const bool changed=!equal(values(actor.combat),values(*m.combat));
    const auto& capture=actor_selection_capture_;const auto* scope=capture.scope?&*capture.scope:nullptr;
    const bool scoped=scope&&scope->actor_id==m.actor_id;
    if(!scoped){if(changed&&((catalog_&&catalog_->personal_loadout_protection_enabled)||capture.active))return {false,{},common_failure("ProtectedActorSelectionScopeRequired",m.actor_id,ErrorKind::internal).error};return {true,std::nullopt,{}};}
    if(!capture.active||!capture.identity||!scope->identity||scope->applied_index||!same_identity(scope->world_before,state_.identity)||scope->next_instance_before!=state_.next_instance_id||!selection_only(m)||!equal(values(actor.combat),values(scope->before))||actor.combat.learned.rows()!=scope->before.learned.rows()||!equal(values(*m.combat),scope->expected)||m.combat->learned.rows()!=scope->before.learned.rows())return {false,{},common_failure("ProtectedActorSelectionMismatch",m.actor_id).error};
    if(capture.drafts.size()>=static_cast<std::size_t>(std::numeric_limits<Integer>::max()))return {false,{},common_failure("ActorSelectionCaptureOverflow").error};
    ActorSelectionExecutionDraft draft;draft.identity=state_.identity;draft.sequence=static_cast<Integer>(capture.drafts.size()+1);draft.actor_id=m.actor_id;draft.command=scope->command;draft.before=values(actor.combat);draft.after=values(*m.combat);draft.next_instance_before=draft.next_instance_after=state_.next_instance_id;draft.capture_identity=capture.identity;draft.scope_identity=scope->identity;return {true,std::move(draft),{}};
}
// 仅在原人物成功发布后记录同一scope的唯一实际行。
void CommonStore::record_actor_selection_write(ActorSelectionExecutionDraft draft){actor_selection_capture_.scope->applied_index=actor_selection_capture_.drafts.size();actor_selection_capture_.drafts.push_back(std::move(draft));}
// 原after_write内先消费真实草稿再分配领域头，晚时钟/实例或改后像均拒。
Outcome<ActorSelectionExecutionDraft> CommonStore::finish_actor_selection_write(const ActorSelectionWriteHandle& handle){auto& scope=actor_selection_capture_.scope;if(!actor_selection_capture_.active||!scope||!handle.identity_||handle.identity_!=scope->identity||!scope->applied_index||*scope->applied_index>=actor_selection_capture_.drafts.size())return {false,{},common_failure("ActorSelectionActualWriteRequired").error};auto& original=actor_selection_capture_.drafts[*scope->applied_index];const auto& draft=original;const auto* actor=state_.actors.find(scope->actor_id);if(!actor||!equal(values(actor->combat),draft.after)||actor->combat.learned.rows()!=scope->before.learned.rows()||state_.next_instance_id!=draft.next_instance_after||draft.scope_identity!=handle.identity_||draft.capture_identity!=actor_selection_capture_.identity||!same_identity(draft.identity,state_.identity))return {false,{},common_failure("ActorSelectionScopeMismatch").error};original.scope_completed=true;auto completed=original;scope.reset();return {true,std::move(completed),{}};}
// 原许可失败只可撤销尚未写入的scope，已写者必须绑定或丢弃整个候选。
Status CommonStore::abandon_actor_selection_write(const ActorSelectionWriteHandle& handle){auto& scope=actor_selection_capture_.scope;if(!actor_selection_capture_.active||!scope||!handle.identity_||handle.identity_!=scope->identity||scope->applied_index)return common_failure("ActorSelectionUnwrittenScopeRequired");scope.reset();return common_success();}
// 保存点及每条原写scope都闭合后才取走草稿；回滚留下的未完成行不得因句柄已作废而漏过。
Outcome<std::vector<ActorSelectionExecutionDraft>> CommonStore::finish_actor_selection_capture(){if(!actor_selection_capture_.active||savepoint_||actor_selection_capture_.scope||std::any_of(actor_selection_capture_.drafts.begin(),actor_selection_capture_.drafts.end(),[](const auto& row){return !row.scope_completed;}))return {false,{},common_failure("ActorSelectionCaptureBoundary",{},ErrorKind::internal).error};auto out=std::move(actor_selection_capture_.drafts);actor_selection_capture_.clear();return {true,std::move(out),{}};}
}
