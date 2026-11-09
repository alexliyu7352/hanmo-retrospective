## 学习页面与原活动会话的有限来源；不报价、授课、变更题面或缓存另一份世界身份。
extends RefCounted
const Pause=preload("res://core/domains/cultivation/skill_study_pause.gd")
const Study=preload("res://core/domains/cultivation/skill_study_rules.gd")
const Etiquette=preload("res://core/domains/cultivation/study_etiquette.gd")
const Interaction=preload("res://core/interaction_rules.gd")

## 在原合法场所分支按作者岗位插入序列课程入口；不可用context仍留给原study_courses诊断。
static func course_contexts(state: Dictionary, data: Dictionary) -> Array:
	var actor: Dictionary=state.actors[state.player_actor_id]
	var place_id: String=actor.location.place_id
	var result: Array=[]
	for role in data.roles.values():
		if role.place_id!=place_id or role.get("skill_courses",[]).is_empty(): continue
		result.append({"role_id":role.id,"context":Interaction.for_role(state,data,role.id)})
	return result

## 只读本人当前课程原付款标量；非课程先返回，不触碰事实、钱包或题面实例。
static func active(state: Dictionary) -> Dictionary:
	var actor: Dictionary=state.actors[state.player_actor_id]
	var action: Dictionary=state.actions.get(actor.action_id,{})
	if not action.get("kind") in ["study_skill","military_train"]:
		for pending in state.actions.values():
			if pending.kind=="study_skill" and pending.actor_id==actor.id and Pause.paused(state.facts[pending.id]):
				action=pending; break
	if not action.get("kind") in ["study_skill","military_train"]: return {"active":false}
	var fact: Dictionary=state.facts[action.id]
	var terms: Dictionary=fact.terms
	var reserved: int=state.wallets[terms.payer_wallet_id].reserved.get(action.id,0)
	var scopes: Array=[]
	if fact.has("challenge") or fact.has("etiquette"): scopes.append("study")
	if fact.has("drill"): scopes.append("training")
	var result: Dictionary={"active":true,"actor_id":actor.id,"action_id":action.id,"kind":action.kind,"remaining_ticks":action.remaining_ticks,"completed_lessons":fact.lessons.size(),"paid":terms.fee-reserved,"reserved":reserved,"challenge_scopes":scopes}
	if action.kind=="study_skill":
		result.paused=Pause.paused(fact); result.pause_supported=Pause.supported(terms); result.paused_ticks=Pause.elapsed(fact,state.tick)
		if terms.get("settlement_policy","")=="per_lesson_v1":
			result.settlement_policy="per_lesson_v1"; result.xp_gained=Study.credited_xp(fact); result.challenge_pending=Etiquette.pending(fact)
	return result

## 每次现算原完整data排序JSON摘要，保同锁不同源的旧票失效；页面与Kernel实例身份仍归UI。
static func identity(state: Dictionary, catalog: Dictionary) -> Dictionary:
	return {"revision":state.get("revision",-1),"actor_id":state.get("player_actor_id",""),"content_digest":catalog.get("content_digest",""),"pack_set_digest":catalog.get("pack_set_digest",""),"source_sha256":JSON.stringify(catalog.get("data",{}),"",true).sha256_text()}
