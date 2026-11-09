## 把已知世界事实整理为行旅手记，不受领任务或修改游戏状态。
extends RefCounted
const TimeRules=preload("res://core/base/time_rules.gd")
const Conditions = preload("res://core/condition_rules.gd")
const Identity=preload("res://core/actor_identity.gd")
const STATUS = {"unknown":"尚未得知", "offered":"尚未应承", "known":"已知线索", "available":"可以问询", "active":"正在履行", "ready":"可以回报", "success":"已履约", "late":"逾期办妥", "expired":"期限已过", "failed":"未能办妥", "cancelled":"已经辞谢"}
## 显示名称仅从当前内容的文本目录读取。
static func name_of(data: Dictionary, record: Dictionary) -> String:
	return data.texts.get(record.get("name_key", record.get("title_key", "")), record.get("id", ""))
## 描述已知任务、实际物资归属，以及合同约定的回报地点。
static func describe(state: Dictionary, data: Dictionary) -> String:
	var actor: Dictionary = state.actors[state.player_actor_id]
	var lines: Array[String] = ["行旅手记", "第%d日%s" % [TimeRules.day_index(state.tick) + 1, "上半日" if TimeRules.half_day(state.tick) == 0 else "下半日"], ""]
	var accepted: Dictionary = {}
	var finished: int = 0
	for task in state.tasks.values():
		if task.assignee != actor.id: continue
		accepted[task.offer_id] = true
		var offer: Dictionary = data.offers[task.offer_id]
		lines.append(name_of(data, offer) + " · " + STATUS.get(task.status, "请向经办人问明"))
		var goal: Dictionary = offer.goal
		if goal.kind == "deliver_item":
			lines.append("约定：%s × %d" % [name_of(data, data.items[goal.item_id]), goal.quantity])
		elif goal.kind == "deliver_message":
			lines.append("亲手交给：" + name_of(data, data.actors[goal.receiver_actor]))
		elif goal.kind == "escort":
			lines.append("护送：%s → %s"%[name_of(data,data.actors[goal.actor_id]),name_of(data,data.places[goal.destination_place_id])])
			if not task.get("escort",{}).get("failure","").is_empty(): lines.append("护卫未能办妥，请当面报告损失；不能领取成功酬劳。")
		var role: Dictionary = data.roles[offer.report_role]
		lines.append("回报：" + name_of(data, data.places[role.place_id]) + " · " + name_of(data, role))
		if task.status in ["active", "ready"]:
			lines.append("还余 %d 个半日；以办完交付时刻为准。" % maxi(0, task.deadline_tick - state.tick))
			var wallet_id: String = task.get("budget_wallet_id", "")
			if state.wallets.has(wallet_id) and offer.budget > 0:
				lines.append("受托经费余额：%d 钱" % state.wallets[wallet_id].balance)
		if task.status == "success": finished += 1
		lines.append("")
	for offer_id in state.knowledge[actor.id]:
		if accepted.has(offer_id): continue
		var status: String = Conditions.task_status(state, actor.id, offer_id)
		lines.append(name_of(data, data.offers[offer_id]) + " · " + STATUS.get(status, "已知线索，尚未应承"))
	lines.append("\n随身行囊")
	var lots: Array = state.inventories[actor.inventory_id].lots
	if lots.is_empty(): lines.append("暂未携带货物。")
	for lot in lots:
		lines.append("%s × %d · %s" % [name_of(data, data.items[lot.item_id]), lot.quantity, "受托保管" if lot.owner.kind == "task" else "私有"])
	if finished > 0: lines.append("\n这一程已经兑现了 %d 份承诺。你仍可继续访友、行商，或在客舍歇脚。" % finished)
	lines.append_array(social_notes(state,data,actor.id))
	return "\n".join(lines)

## 只描述本人已知的原始事实，列出首次亲历或转述来源；未知类型和远方秘闻不会从全局状态泄漏。
static func social_notes(state: Dictionary, data: Dictionary, actor_id: String) -> Array[String]:
	var memories: Dictionary=state.get("social",{}).get("memories",{}).get(actor_id,{})
	var lines: Array[String]=[]
	var ids: Array=memories.keys(); ids.sort()
	for id in ids:
		var fact: Dictionary=state.social.events.get(id,state.get("facts",{}).get(id,{}))
		var words: String=""
		match fact.get("kind",""):
			"aid","theft","refusal":
				var person: String=Identity.definition(state,data,fact.actor_id).get("display_name",fact.actor_id)
				var target: String=Identity.definition(state,data,fact.target_actor_id).get("display_name",fact.target_actor_id)
				if fact.kind=="aid": words="%s拿出%s，接济了%s。"%[person,name_of(data,data.items[fact.item_id]),target]
				elif fact.kind=="theft": words="%s取走了%s的%s，引发争执。"%[person,target,name_of(data,data.items[fact.item_id])]
				else: words="%s没有答应%s的求助。"%[person,target]
			"training":
				if fact.get("phase")=="completed": words="%s完成了%s修习。"%[Identity.definition(state,data,fact.training.student_id).get("display_name","此人"),name_of(data,data.personal_skills[fact.training.skill_id])]
			"personal_battle": words="听闻一场比武交锋已有结果。"
		if words.is_empty(): continue
		var memory: Dictionary=memories[id]
		var source: String="亲历" if memory.source_actor_id==actor_id else Identity.definition(state,data,memory.source_actor_id).get("display_name",memory.source_actor_id)+"相告"
		lines.append("%s（第%d日 · %s）"%[words,TimeRules.day_index(memory.learned_tick)+1,source])
	if not lines.is_empty(): lines.push_front("\n近事与见闻")
	return lines
