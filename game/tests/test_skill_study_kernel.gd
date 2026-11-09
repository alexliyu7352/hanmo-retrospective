## 只补普通课程正式Kernel事务/保存/取消接线，不复跑叶规则矩阵或UI体验。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Codec=preload("res://core/save_codec.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const PLAYER="base::actor.player"
const PAYEE="base::wallet.office"
const ROLE="base::role.office"

## 只包装原课程终验，观察已结算候选后主动拒绝；不修改正式注册身份或绕过任何旧检查。
class TerminalProbe extends "res://core/world_kernel.gd":
	var reject_terminal: bool=false
	var target_action: String=""
	var observed: Dictionary={}
	## 测试故障通过原描述构建，终态之前的报价、保存和课程校验仍正常运行。
	func _module_descriptors() -> Array:
		var rows: Array=super._module_descriptors()
		for descriptor in rows:
			for entry in descriptor.validators:
				if entry.id!="skill_study.state": continue
				var original: Callable=entry.callback
				entry.callback=func(candidate: Dictionary,data: Dictionary) -> Dictionary:
					var checked: Dictionary=original.call(candidate,data)
					if not checked.ok or not reject_terminal: return checked
					var fact: Dictionary=candidate.facts.get(target_action,{})
					if fact.get("phase")!="completed" or fact.get("code")!="StudyCancelled": return checked
					observed=candidate.duplicate(true)
					return {"ok":false,"code":"InjectedStudyTerminalValidation"}
		return rows

var checks: int=0
var failures: int=0

## 只在准备器的独立工程内延后执行。
func _init() -> void:
	call_deferred("_run")

## 记录失败且非零退出，不用日志安静冒充断言成功。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 所有世界变化均通过真实版本化命令，不能直接补结果或放松Schema。
func submit(kernel, kind: String, params: Dictionary={}) -> Dictionary:
	var state: Dictionary=kernel.snapshot()
	return kernel.submit({"kind":kind,"params":params,"actor_id":PLAYER,"request_seq":state.request_sequences[PLAYER]+1,"expected_revision":state.revision})

## 同一真实新锁下验证预留、已提供费用、主动取消、完整结课及中途v2完整校验。
func _run() -> void:
	if "--bargain-only" in OS.get_cmdline_user_args():
		_bargain_only(); return
	if "--etiquette-only" in OS.get_cmdline_user_args():
		_etiquette_only(); return
	var index: Dictionary=JSON.parse_string(FileAccess.get_file_as_string("res://data/generated/installed-content.json"))
	var path: String="res://data/generated/"+index.sets[0].directory+"/catalog.hmcatalog.json"
	var kernel=Kernel.new(); var started: Dictionary=kernel.new_game("base::scenario.procurement",path)
	check(started.ok,"正式目录经实际Kernel启动 "+str(started))
	if not started.ok: quit(1); return
	var original: Dictionary=kernel.snapshot()
	var context: Dictionary=Interaction.for_role(original,kernel.content().data,ROLE)
	var query: Dictionary=kernel.study_courses(PLAYER,context,2)
	check(query.ok and not query.courses.is_empty(),"真实薄入口提供课程报价 "+str(query))
	check(kernel.snapshot()==original,"查询不改世界/RNG")
	var params: Dictionary={"skill_id":"rites","ticks":2,"context":context}
	var result: Dictionary=submit(kernel,"study_skill",params)
	check(result.ok,"课程进入正式注册长行动 "+str(result))
	if not result.ok: quit(1); return
	var state: Dictionary=kernel.snapshot(); var action_id: String=state.actors[PLAYER].action_id
	var payer: String=state.actors[PLAYER].wallet_id
	check(state.wallets[payer].balance==800 and state.wallets[payer].reserved[action_id]==60,"开课只预留，不预付全部课程")
	check(state.wallets[PAYEE].balance==5000,"开课收款人未收到未提供课时")
	result=submit(kernel,"advance")
	check(result.ok,"真实半日课程通过全世界校验 "+str(result))
	if not result.ok: quit(1); return
	state=kernel.snapshot()
	check(state.wallets[payer].balance==770 and state.wallets[PAYEE].balance==5030 and state.wallets[payer].reserved[action_id]==30,"仅支付已提供半日且保余款预留")
	check(state.actors[PLAYER].skills.rites==0,"未结课不提前发XP")
	var saved: Dictionary=kernel.save_context(); var text: String=Codec.encode(saved.state,1,"study-kernel",saved.catalog,saved.registry)
	check(not text.is_empty(),"真实v2保存包含新增行动事实/预留")
	var decoded: Dictionary=Codec.decode(text,saved.catalog,saved.registry)
	check(decoded.ok and decoded.state==state,"中途正文完整Schema回读保持所有状态")
	if not decoded.ok: quit(1); return
	var restored=Kernel.new(); var restored_result: Dictionary=restored.restore_with_catalog(decoded.state,saved.catalog,saved.registry)
	check(restored_result.ok and restored.snapshot()==state,"另一Kernel原子恢复同课程")
	var command: Dictionary={"kind":"study_cancel","params":{"action_id":action_id},"actor_id":PLAYER,"request_seq":state.request_sequences[PLAYER]+1,"expected_revision":state.revision}
	result=restored.submit(command)
	check(result.ok,"忙碌中仅合法本人课程取消可进入 "+str(result))
	if not result.ok: quit(1); return
	state=restored.snapshot()
	check(state.actors[PLAYER].action_id.is_empty() and not state.actions.has(action_id) and not state.wallets[payer].reserved.has(action_id),"唯一释放器清双方占用及余款")
	check(state.wallets[payer].balance==770 and state.wallets[PAYEE].balance==5030 and state.actors[PLAYER].skills.rites==20,"取消守恒已提供费用并保留一课20XP")
	check(restored.submit(command)==result and restored.snapshot()==state,"重复命令不再次结算或推进")
	context=Interaction.for_role(state,restored.content().data,ROLE)
	result=submit(restored,"study_skill",{"skill_id":"rites","ticks":2,"context":context})
	check(result.ok,"取消后可再次正式开课")
	for number in range(2):
		result=submit(restored,"advance"); check(result.ok,"后续完整课程半日 "+str(number)+" "+str(result))
	state=restored.snapshot()
	check(state.actors[PLAYER].skills.rites==60 and state.wallets[payer].balance==710 and state.wallets[PAYEE].balance==5090,"后续完整结课新增原40XP，保留先前20且仅实际课费")
	check(state.wallets[payer].reserved.is_empty() and state.actions.is_empty(),"完整结课没有遗留预留或占用")
	_terminal_rollback(path)
	print("SKILL_STUDY_KERNEL_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)

## 真实终态候选中已发生XP与释放之后故障，整个命令仍原子回滚；同请求重试和终态v2都只领一次。
func _terminal_rollback(path: String) -> void:
	var kernel=TerminalProbe.new()
	if not check(kernel.new_game("base::scenario.procurement",path).ok,"终态回滚独立正式目录启动"): return
	var context: Dictionary=Interaction.for_role(kernel.snapshot(),kernel.content().data,ROLE)
	if not check(submit(kernel,"study_skill",{"skill_id":"rites","ticks":2,"context":context}).ok,"回滚前正式开课"): return
	if not check(submit(kernel,"advance").ok,"回滚前实际提供一课"): return
	var before: Dictionary=kernel.snapshot(); var id: String=before.actors[PLAYER].action_id
	var payer: String=before.actors[PLAYER].wallet_id
	var command: Dictionary={"kind":"study_cancel","params":{"action_id":id},"actor_id":PLAYER,"request_seq":before.request_sequences[PLAYER]+1,"expected_revision":before.revision}
	kernel.target_action=id; kernel.reject_terminal=true
	var denied: Dictionary=kernel.submit(command)
	check(denied.get("code")=="InjectedStudyTerminalValidation" and not kernel.observed.is_empty(),"真实尾门在合法取消候选完成后才拒绝 "+str(denied))
	if not kernel.observed.is_empty():
		check(kernel.observed.actors[PLAYER].skills.rites==20 and not kernel.observed.wallets[payer].reserved.has(id) and kernel.observed.wallets[payer].balance==before.wallets[payer].balance and kernel.observed.wallets[PAYEE].balance==before.wallets[PAYEE].balance,"被拒候选确实已给实际20XP并释放余款，未再次付费")
	check(kernel.snapshot()==before,"失败候选丢弃XP、预留释放、事实、revision与序号的全部变化")
	kernel.reject_terminal=false
	var accepted: Dictionary=kernel.submit(command)
	check(accepted.ok and kernel.snapshot().actors[PLAYER].skills.rites==20,"同一未消费请求重试只结算一课20XP")
	var committed: Dictionary=kernel.snapshot()
	check(kernel.submit(command)==accepted and kernel.snapshot()==committed,"成功后的相同请求不二次授奖或退款")
	var saved: Dictionary=kernel.save_context()
	var encoded: String=Codec.encode(saved.state,1,"study-terminal",saved.catalog,saved.registry)
	var decoded: Dictionary=Codec.decode(encoded,saved.catalog,saved.registry)
	check(not encoded.is_empty() and decoded.ok and decoded.state==committed,"新语义取消终态v2精确保存已领成长")
	if decoded.ok:
		var restored=Kernel.new()
		check(restored.restore_with_catalog(decoded.state,saved.catalog,saved.registry).ok and restored.snapshot()==committed,"另一Kernel恢复终态不重发部分经验")
		check(restored.submit(command)==accepted and restored.snapshot()==committed,"终态读档后同请求仍返回原结果")
		# 探针回调捕获原Kernel，显式拆除测试引用，不把泄漏掩盖在断言通过之后。
		restored._registry={}
	saved.registry={}; kernel._registry={}

## 仅新礼议实例接线：一次抽种、即时选择、课中v2与原课程唯一结算，旧21项不重复。
func _etiquette_only() -> void:
	var index: Dictionary=JSON.parse_string(FileAccess.get_file_as_string("res://data/generated/installed-content.json"))
	var path: String="res://data/generated/"+index.sets[0].directory+"/catalog.hmcatalog.json"
	var kernel=Kernel.new(); var started: Dictionary=kernel.new_game("base::scenario.procurement",path)
	check(started.ok,"礼议正式目录实际Kernel启动 "+str(started))
	if not started.ok: quit(1); return
	var before: Dictionary=kernel.snapshot()
	var context: Dictionary=Interaction.for_role(before,kernel.content().data,ROLE)
	var result: Dictionary=submit(kernel,"study_skill",{"skill_id":"rites","ticks":2,"context":context,"challenge_mode":"manual"})
	check(result.ok,"开课一次创建本人题面 "+str(result))
	if not result.ok: quit(1); return
	var state: Dictionary=kernel.snapshot(); var action_id: String=state.actors[PLAYER].action_id
	check(state.rng.minigame!=before.rng.minigame,"开课唯一minigame流真实推进")
	for stream in ["world","event","duel","battle"]: check(state.rng[stream]==before.rng[stream],"开课不抽其它流 "+stream)
	var frozen_rng: Dictionary=state.rng.duplicate(true)
	for round_number in range(3):
		before=kernel.snapshot()
		var view: Dictionary=kernel.study_etiquette(PLAYER,action_id)
		check(view.ok and view.phase=="active" and kernel.snapshot()==before,"题面查询纯只读 "+str(round_number))
		if not view.ok: quit(1); return
		result=submit(kernel,"study_etiquette_choose",{"action_id":action_id,"node_id":view.node_id,"option_id":view.options[0].id})
		check(result.ok,"忙碌只允许本人真实答题 "+str(result))
		if not result.ok: quit(1); return
		state=kernel.snapshot()
		check(state.tick==0 and state.rng==frozen_rng and state.actors[PLAYER].skills.rites==0,"选择不推进课时、不重抽、不提前发XP")
		if round_number==0:
			var saved: Dictionary=kernel.save_context(); var text: String=Codec.encode(saved.state,1,"etiquette-kernel",saved.catalog,saved.registry)
			var decoded: Dictionary=Codec.decode(text,saved.catalog,saved.registry)
			check(not text.is_empty() and decoded.ok and decoded.state==state,"答题中v2完整校验/选择保持")
			if not decoded.ok: quit(1); return
			var restored=Kernel.new(); var restored_result: Dictionary=restored.restore_with_catalog(decoded.state,saved.catalog,saved.registry)
			check(restored_result.ok and restored.snapshot()==state,"新Kernel恢复同一实例")
			kernel=restored
	check(kernel.study_etiquette(PLAYER,action_id).phase=="completed","第三选择终态且未直接授奖")
	for number in range(2):
		result=submit(kernel,"advance");check(result.ok,"真实礼议课程半日通过世界校验 "+str(result))
	state=kernel.snapshot(); var payer: String=state.actors[PLAYER].wallet_id
	check(state.actors[PLAYER].skills.rites==48,"正常两课40XP仅120%且一次授予")
	check(state.wallets[payer].balance==740 and state.wallets[PAYEE].balance==5060 and state.wallets[payer].reserved.is_empty(),"课程实付60且无多余预留")
	check(state.facts[action_id].etiquette.consumed and state.actions.is_empty(),"唯一消费后释放课程")
	print("ETIQUETTE_KERNEL_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)

## 仅新议价练习实例接线：一次抽种、即时选择、课中v2与原课程唯一结算，旧21项不重复。
func _bargain_only() -> void:
	var index: Dictionary=JSON.parse_string(FileAccess.get_file_as_string("res://data/generated/installed-content.json"))
	var path: String="res://data/generated/"+index.sets[0].directory+"/catalog.hmcatalog.json"
	var kernel=Kernel.new(); var started: Dictionary=kernel.new_game("base::scenario.procurement",path)
	check(started.ok,"议价练习正式目录实际Kernel启动 "+str(started))
	if not started.ok: quit(1); return
	var before: Dictionary=kernel.snapshot()
	var context: Dictionary=Interaction.for_role(before,kernel.content().data,ROLE)
	var result: Dictionary=submit(kernel,"study_skill",{"skill_id":"commerce","ticks":2,"context":context,"challenge_mode":"manual"})
	check(result.ok,"开课一次创建本人题面 "+str(result))
	if not result.ok: quit(1); return
	var state: Dictionary=kernel.snapshot(); var action_id: String=state.actors[PLAYER].action_id
	check(state.rng.minigame!=before.rng.minigame,"开课唯一minigame流真实推进")
	for stream in ["world","event","duel","battle"]: check(state.rng[stream]==before.rng[stream],"开课不抽其它流 "+stream)
	var frozen_rng: Dictionary=state.rng.duplicate(true)
	for round_number in range(3):
		before=kernel.snapshot()
		var view: Dictionary=kernel.study_challenge(PLAYER,action_id)
		check(view.ok and view.phase=="active" and kernel.snapshot()==before,"题面查询纯只读 "+str(round_number))
		if not view.ok: quit(1); return
		result=submit(kernel,"study_challenge_choose",{"action_id":action_id,"step_id":view.round_id,"option_id":view.options[0].id})
		check(result.ok,"忙碌只允许本人真实答题 "+str(result))
		if not result.ok: quit(1); return
		state=kernel.snapshot()
		check(state.tick==0 and state.rng==frozen_rng and state.actors[PLAYER].skills.commerce==0,"选择不推进课时、不重抽、不提前发XP")
		if round_number==0:
			var saved: Dictionary=kernel.save_context(); var text: String=Codec.encode(saved.state,1,"bargain-kernel",saved.catalog,saved.registry)
			var decoded: Dictionary=Codec.decode(text,saved.catalog,saved.registry)
			check(not text.is_empty() and decoded.ok and decoded.state==state,"答题中v2完整校验/选择保持")
			if not decoded.ok: quit(1); return
			var restored=Kernel.new(); var restored_result: Dictionary=restored.restore_with_catalog(decoded.state,saved.catalog,saved.registry)
			check(restored_result.ok and restored.snapshot()==state,"新Kernel恢复同一实例")
			kernel=restored
	check(kernel.study_challenge(PLAYER,action_id).phase=="completed","第三选择终态且未直接授奖")
	for number in range(2):
		result=submit(kernel,"advance");check(result.ok,"真实议价练习课程半日通过世界校验 "+str(result))
	state=kernel.snapshot(); var payer: String=state.actors[PLAYER].wallet_id
	check(state.actors[PLAYER].skills.commerce==48,"正常两课40XP仅120%且一次授予")
	check(state.wallets[payer].balance==740 and state.wallets[PAYEE].balance==5060 and state.wallets[payer].reserved.is_empty(),"课程实付60且无多余预留")
	check(state.facts[action_id].challenge.consumed and state.actions.is_empty(),"唯一消费后释放课程")
	print("BARGAIN_KERNEL_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
