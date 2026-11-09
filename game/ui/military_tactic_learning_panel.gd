## 军阵战法只读永久取得与本人操练来源；按钮仍向原Kernel重新报价，界面不增掌握或经验。
extends RefCounted

## 只列有限本人记录；教师报名由实际当面对白提供，不能在军务页隔空购买。
static func render(host, parent: Control, view: Dictionary) -> void:
	if not view.get("active", false): return
	host._text(parent, "战法与实践认定", 23)
	host._text(parent, "技艺达标后，仍须当面受教或完成具名操练考核。一次操练只能认定一种战法。", 17)
	for known in view.get("known", []).slice(0, 32):
		host._text(parent, "%s · 第%d半日取得" % [host._name({"name_key":known.name_key}), int(known.tick)], 19)
		host._text(parent, "原取得 " + str(known.acquisition_fact_id) + " · 来源 " + str(known.source_fact_id), 14)
	if view.get("known", []).is_empty(): host._text(parent, "本人尚无永久战法取得。", 17)
	for candidate in view.get("practice_candidates", []).slice(0, 32):
		host._text(parent, host._name({"name_key":candidate.name_key}) + " · 原操练 " + str(candidate.training_action_id), 18)
		if not candidate.get("ok", false):
			host._text(parent, reason(str(candidate.get("code", ""))), 16)
			continue
		if candidate.get("replayed", false):
			host._text(parent, "这次操练已完成认定，原取得保留。", 16)
			continue
		host._text(parent, "原考核%d分 · 开训前已证至少%d级" % [int(candidate.score), int(candidate.minimum_level)], 16)
		_command(host, parent, view, candidate.params)
	if view.get("truncated", false): host._text(parent, "本页仅列最近16次操练及32项本人记录；完整来源保留。", 15)

## 异步未就绪禁用；同页身份、操作者及场景代次都绑定实际提交，重复点击只接一次。
static func _command(host, parent: Control, view: Dictionary, params: Dictionary) -> void:
	var button = Button.new(); button.name = "MilitaryTacticPractice"; button.text = "本人确认实践认定"; parent.add_child(button)
	var offer: Dictionary = host.kernel.military_camp_offer("military_tactic_practice", params)
	var quote: Dictionary = offer.get("quote", {})
	button.disabled = host._advancing or not offer.get("ok", true) or not quote.get("ok", false)
	if button.disabled: host._text(parent, reason(str(offer.get("code", quote.get("code", "")))), 16)
	var binding = {"source":view.get("source", {}).duplicate(true), "actor_id":view.get("actor_id", ""), "scene":host._scene_counter, "load":int(host.get_meta("personal_battle_generation", 0))}
	button.pressed.connect(_send.bind(host, button, host.kernel, binding, params.duplicate(true)))

## 不使用今天重建的旧页参数；实际Kernel身份变化、存读中或已提交都使原按钮失效。
static func _send(host, button: Button, kernel, binding: Dictionary, params: Dictionary) -> void:
	if not is_instance_valid(button) or not button.is_inside_tree() or not button.is_visible_in_tree() or button.is_queued_for_deletion(): return
	if button.disabled or host._advancing or host.kernel != kernel or not host._storage_intent.is_empty(): return
	if host._scene_counter != binding.scene or int(host.get_meta("personal_battle_generation", 0)) != binding.load: return
	if kernel.source_identity() != binding.source: return
	var context: Dictionary = kernel.command_context()
	if not context.get("ok", false) or context.get("actor_id", "") != binding.actor_id: return
	button.disabled = true
	host._perform("military_tactic_practice", params)

## 拒绝只解释真实规则；未识别错误保留原码以便诊断。
static func reason(code: String) -> String:
	return {"NativeAsyncRequired":"正在核对原操练与当前身份…", "MilitaryTacticPracticeTrainingRequired":"需要本人完整完成的原付费操练。", "MilitaryTacticPracticeScoreRequired":"原题面或成绩未达到这项战法的明确考核要求。", "MilitaryTacticPracticeQualificationRequired":"原开训前的基础技艺尚不足；后来升级不能倒补。", "MilitaryTacticPracticeAlreadyAssessed":"这次操练已经用于另一项认定。", "MilitaryTacticAlreadyLearned":"本人已经掌握此战法。", "MilitaryTacticPracticeActorUnavailable":"本人须在世、可用且没有其他进行中行动。", "MilitaryTacticPracticeActorOccupied":"本人仍在军战、围困或俘获占用中。", "MilitaryOccupationHistoryIncomplete":"旧占用记录不足以证明可亲办，暂不能认定。"}.get(code, "暂不能办理：" + code)
