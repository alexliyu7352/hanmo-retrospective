## 五项修业命令注册回归；冻结旧Kernel对照完整候选、回执和Codec，保留三项旧行动生命周期。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const LegacyKernel=preload("res://tests/fixtures/cultivation_2ee6900/world_kernel.gd")
const Module=preload("res://core/domains/cultivation/cultivation_module.gd")
const Cultivation=preload("res://core/cultivation_rules.gd")
const Study=preload("res://core/domains/cultivation/skill_study_rules.gd")
const Registry=preload("res://core/runtime/module_registry.gd")
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const Codec=preload("res://core/save_codec.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const Strict=preload("res://core/strict_json.gd")
## 仅在测试中移除一个发布命令，验证完整新局检查不会回退旧兜底。
class MissingCommandKernel extends "res://core/world_kernel.gd":
	var missing: String=""

	## 保留全部真实描述，只注入指定缺项；不改其他领域或旧行动验证。
	func _module_descriptors() -> Array:
		var descriptors: Array=super._module_descriptors()
		for descriptor in descriptors:
			if descriptor.id!="cultivation": continue
			for index in range(descriptor.commands.size()-1,-1,-1):
				if descriptor.commands[index].kind==missing: descriptor.commands.remove_at(index)
		return descriptors

var checks: int=0
var failures: int=0
var failed_tail_candidate: Dictionary={}

## 初始化后延迟执行，缺注册用明确断言而非解析错误报告。
func _init() -> void:
	call_deferred("run_checks")

## 每条失败保留标签；进程退出状态与真实断言结果保持一致。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 从同一正式作者目录建立隔离新旧世界，旧Kernel不加载新命令适配器。
func fresh() -> Dictionary:
	var current=Kernel.new(); var legacy=LegacyKernel.new()
	if not check(current.new_game("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json").ok,"注册世界创建"): return {}
	if not check(legacy.new_game("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json").ok,"独立旧Kernel世界创建"): return {}
	check(current.snapshot()==legacy.snapshot(),"相同工厂和目录完整初态相等")
	return {"current":current,"legacy":legacy}

## 同一命令比较预览只读、完整候选、提交结果、重复请求和严格Codec往返。
func compare(pair: Dictionary, kind: String, params: Dictionary={}, actor_id: String="") -> Dictionary:
	var state: Dictionary=pair.current.snapshot()
	if actor_id.is_empty(): actor_id=state.player_actor_id
	var preview: Dictionary=pair.current.preview(kind,params,actor_id)
	var old_preview: Dictionary=pair.legacy.preview(kind,params,actor_id)
	check(preview==old_preview and pair.current.snapshot()==state and pair.legacy.snapshot()==state,"纯预览与旧守卫同序 "+kind)
	if preview.ok:
		var candidate: Dictionary=state.duplicate(true); var old_candidate: Dictionary=state.duplicate(true)
		var applied: Dictionary=pair.current._apply(candidate,kind,params,actor_id,preview)
		var old_applied: Dictionary=pair.legacy._apply(old_candidate,kind,params,actor_id,old_preview)
		check(applied==old_applied and candidate==old_candidate,"独立旧源码完整候选与details一致 "+kind)
	var command: Dictionary={"kind":kind,"params":params,"actor_id":actor_id,"request_seq":state.request_sequences[actor_id]+1,"expected_revision":state.revision}
	var result: Dictionary=pair.current.submit(command); var old_result: Dictionary=pair.legacy.submit(command)
	var after: Dictionary=pair.current.snapshot()
	check(result==old_result and after==pair.legacy.snapshot(),"完整提交结果与权威世界一致 "+kind+" "+str(result))
	if not result.ok:
		check(after==state,"拒绝不动资产、RNG、序号或时钟 "+kind)
		return result
	check(pair.current.submit(command)==result and pair.legacy.submit(command)==old_result and pair.current.snapshot()==after and pair.legacy.snapshot()==after,"重复请求返回原回执且不重收费发奖 "+kind)
	var encoded: String=Codec.encode(after,0,"registered-cultivation",pair.current.content())
	var old_encoded: String=Codec.encode(pair.legacy.snapshot(),0,"registered-cultivation",pair.legacy.content())
	var decoded: Dictionary=Codec.decode(encoded,pair.current.content())
	check(not encoded.is_empty() and encoded==old_encoded and decoded.ok and decoded.state==after,"完整Codec字节与恢复世界一致 "+kind)
	if decoded.ok: check(pair.current.restore(decoded.state).ok and pair.current.snapshot()==after,"真实恢复不重放费用或奖励 "+kind)
	return result

## 冻结文件只允许重定向Builtin路径，校验保存的原源码摘要防止旧路悄然跟随新注册。
func oracle_integrity() -> void:
	var root: String="res://tests/fixtures/cultivation_2ee6900/"
	var parsed: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(root+"provenance.json"))
	if not check(parsed.ok,"旧源码来源清单可解析"): return
	for filename in parsed.value.sources:
		var entry: Dictionary=parsed.value.sources[filename]
		var source: String=FileAccess.get_file_as_string(root+filename)
		check(source.sha256_text()==entry.fixture_sha256,"冻结文件摘要 "+filename)
		if filename=="world_kernel.gd": source=source.replace(root+"builtin_modules.gd","res://core/runtime/builtin_modules.gd")
		check(source.sha256_text()==entry.original_sha256,"恢复原路径后等于2ee6900原源码 "+filename)

## 直接从规则的唯一集合验证注册覆盖，不用测试自造五项名单冒充真实注册。
func coverage() -> bool:
	var registry: Dictionary=Registry.build(Builtin.descriptors())
	if not check(registry.ok,"内建描述可构建"): return false
	var found: Array=[]
	for kind in registry.commands:
		if registry.commands[kind].owner=="cultivation": found.append(kind)
	var expected: Array=Cultivation.KINDS.duplicate(); expected.append_array([Study.KIND,Study.CANCEL_KIND]); expected.append_array(Module.ETIQUETTE_KINDS); expected.append_array(Module.CHALLENGE_KINDS); expected.sort(); found.sort()
	if not check(found==expected,"旧五项、普通课程与礼议操作均真实且唯一注册"): return false
	check(registry.actions.has(Study.KIND) and not registry.legacy_action_validators.has(Study.KIND),"普通课程走正式行动而非旧兼容桥")
	check(registry.commands[Study.CANCEL_KIND].gate_id==Module.STUDY_CANCEL_GATE,"本人取消使用唯一精确早门")
	for kind in Cultivation.KINDS:
		var command: Dictionary=registry.commands[kind]
		check(command.gate_id=="legacy_kernel","保留公共守卫原插入点 "+kind)
		check(command.mode==("action" if kind in Cultivation.TRAINING_KINDS else "instant"),"模式明确区分兼容行动 "+kind)
		if kind in Cultivation.TRAINING_KINDS:
			check(command.get("compat_action")=="legacy_begin_action","训练明确使用旧开工桥 "+kind)
			check(not registry.actions.has(kind) and registry.legacy_action_validators.has(kind),"未冒充已迁移完整生命周期 "+kind)
	check(Module.command_contract_status(registry).ok,"完整世界五项合同通过")
	for kind in Cultivation.KINDS:
		for mutation in ["missing","owner","mode","gate","compat"]:
			var bad: Dictionary=registry.duplicate(true)
			match mutation:
				"missing": bad.commands.erase(kind)
				"owner": bad.commands[kind].owner="personal"
				"mode": bad.commands[kind].mode="instant" if kind in Cultivation.TRAINING_KINDS else "action"
				"gate": bad.commands[kind].gate_id="cultivation_early"
				"compat": bad.commands[kind].compat_action="wrong_bridge"
			check(not Module.command_contract_status(bad).ok,"完整世界拒绝漏项或错配 "+mutation+" "+kind)
	for kind in Cultivation.TRAINING_KINDS:
		var bad: Dictionary=registry.duplicate(true); bad.legacy_action_validators.erase(kind)
		check(not Module.command_contract_status(bad).ok,"兼容训练不能丢载荷验证 "+kind)
	return true

## 真实学习、短练习和真人传授只在到期时结算；不运行长修业矩阵。
func training_flow() -> void:
	var pair: Dictionary=fresh()
	if pair.is_empty(): return
	compare(pair,"equip_weapon",{"weapon_id":""}); compare(pair,"equip_weapon",{"weapon_id":"item.sword"})
	compare(pair,"configure_heart",{"skill_id":""})
	compare(pair,"configure_heart",{"skill_id":"skill.root"},"actor.wei")
	compare(pair,"configure_heart",{"skill_id":""},"actor.wei")
	compare(pair,"move_local",{"place_id":"west.training"})
	compare(pair,"talk",{"role_id":"role.wei"}); compare(pair,"dialogue_next")
	compare(pair,"dialogue_choose",{"option_id":"learn0"})
	var state: Dictionary=pair.current.snapshot(); var player: String=state.player_actor_id
	var before_money: int=state.wallets["wallet.player"].balance
	if not compare(pair,"dialogue_confirm").ok: return
	state=pair.current.snapshot()
	var action_id: String=state.actors[player].action_id
	check(state.actions[action_id].kind=="learn_skill" and state.actions[action_id].remaining_ticks==2 and not state.actors[player].combat.learned.has("skill.cleave"),"课时和师生占用建立，开工不发奖")
	check(state.wallets["wallet.player"].balance==before_money-35,"真实学费只收一次")
	compare(pair,"move_local",{"place_id":"west.street"},"actor.wei")
	compare(pair,"cancel_task",{"task_id":"absent"})
	check(compare(pair,"rest_cancel").get("code")=="NotResting","休整取消不能取消学习或退款")
	compare(pair,"advance")
	state=pair.current.snapshot()
	check(not state.actors[player].combat.learned.has("skill.cleave") and state.actions[action_id].remaining_ticks==1,"第一半日不分期授予技能")
	compare(pair,"advance")
	state=pair.current.snapshot()
	check(state.actors[player].combat.learned.get("skill.cleave",-1)==0 and state.facts[action_id].phase=="completed","到期一次完成学习")
	compare(pair,"end_interaction")
	compare(pair,"practice_skill",{"skill_id":"skill.cleave","ticks":2})
	var before_mastery: int=pair.current.snapshot().actors[player].combat.learned["skill.cleave"]
	compare(pair,"advance")
	check(pair.current.snapshot().actors[player].combat.learned["skill.cleave"]==before_mastery,"短练习途中不发奖")
	compare(pair,"advance")
	check(pair.current.snapshot().actors[player].combat.learned["skill.cleave"]==26,"短练习按原两课时一次结算")
	# 已精通真人授课，复用同一内容且不为达到资格运行八半日修炼。
	compare(pair,"move_local",{"place_id":"west.street"},"actor.wei")
	compare(pair,"move_local",{"place_id":"west.grain"},"actor.wei")
	var context: Dictionary=Interaction.for_role(pair.current.snapshot(),pair.current.content().data,"role.clerk")
	state=pair.current.snapshot(); var pupil: String=context.provider_actor_id
	var teacher_learned: Dictionary=state.actors["actor.wei"].combat.learned.duplicate(true)
	if not compare(pair,"teach_skill",{"skill_id":"skill.cleave","context":context},"actor.wei").ok: return
	compare(pair,"advance")
	check(pair.current.snapshot().actors[pupil].combat.learned.get("skill.cleave",-1)==state.actors[pupil].combat.learned.get("skill.cleave",-1),"传授途中目标本人尚未得奖")
	compare(pair,"advance")
	state=pair.current.snapshot()
	check(state.actors[pupil].combat.learned.get("skill.cleave",-1)==10 and state.actors["actor.wei"].combat.learned==teacher_learned,"仅原目标真人得十点，老师不复制或重设技能")

## 用冲突前提比较首个错误，不靠合法存档入口允许的状态限制隐藏守卫次序。
func guard_order() -> void:
	var pair: Dictionary=fresh()
	if pair.is_empty(): return
	var original: Dictionary=pair.current.snapshot(); var player: String=original.player_actor_id
	for mutation in ["unavailable","busy","following","army","battle","capability"]:
		var state: Dictionary=original.duplicate(true)
		var data: Dictionary=pair.current._catalog.data.duplicate(true)
		match mutation:
			"unavailable": state.actors[player].available=false
			"busy": state.actors[player].action_id="action.busy"
			"following": state.actors[player].location={"kind":"Following","leader_id":"actor.wei"}
			"army": state.actors[player].location={"kind":"InArmy","army_id":"army.test"}
			"battle": state.battles={"battle.test":{"kind":"personal","phase":"planning","units":{player:{}}}}
			"capability": data.required_capabilities.erase("cultivation")
		pair.current._catalog.data=data; pair.legacy._catalog.data=data
		for kind in Cultivation.KINDS:
			var before: Dictionary=state.duplicate(true)
			var result: Dictionary=pair.current._preview(state,kind,{},player)
			check(not result.ok and result==pair.legacy._preview(state,kind,{},player) and state==before,"错误优先和资产/RNG不变 "+mutation+" "+kind)
	for kind in Cultivation.KINDS:
		compare(pair,kind,{})

## 固定探针只在测试注册副本上使用，用命中结果证明实际Kernel经过注册回调。
func probe_preview(_state: Dictionary, _data: Dictionary, _actor_id: String, _kind: String, _params: Dictionary) -> Dictionary:
	return {"ok":false,"code":"CultivationPreviewProbe"}

## apply探针在业务前返回，避免只验证描述存在却仍走旧分支的假注册。
func probe_apply(_state: Dictionary, _data: Dictionary, _actor_id: String, _kind: String, _params: Dictionary, _offer: Dictionary) -> Dictionary:
	return {"ok":false,"code":"CultivationApplyProbe"}

## 五命令都必须命中注册派发，缺注册的完整开局必须保留先前世界与内容。
func routing_witness() -> void:
	var pair: Dictionary=fresh()
	if pair.is_empty(): return
	var k=pair.current; var state: Dictionary=k.snapshot(); var player: String=state.player_actor_id
	for kind in Cultivation.KINDS:
		var descriptor: Dictionary=k._registry.commands[kind].duplicate(true)
		k._registry.commands[kind].preview=probe_preview
		check(k.preview(kind,{}).get("code")=="CultivationPreviewProbe","真实preview命中登记回调 "+kind)
		k._registry.commands[kind].mode="instant"; k._registry.commands[kind].apply=probe_apply
		k._registry.commands[kind].erase("compat_action")
		var candidate: Dictionary=state.duplicate(true)
		check(k._apply(candidate,kind,{},player,{}).get("code")=="CultivationApplyProbe" and candidate==state,"真实apply命中登记派发 "+kind)
		k._registry.commands[kind]=descriptor
	var startup=MissingCommandKernel.new()
	if not check(startup.new_game().ok,"缺项检查前有效世界建立"): return
	var before: Dictionary=startup.snapshot(); var content: Dictionary=startup.content()
	for kind in Cultivation.KINDS:
		startup.missing=kind
		var result: Dictionary=startup.new_game()
		check(result.get("code")=="MissingBuiltinCommand" and startup.snapshot()==before and startup.content()==content,"漏登记不开局且不替换原世界 "+kind)

## 注入过期课时，让真实旧开工先分配ID和占用再失败，检查submit的候选隔离。
func stale_duration(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary) -> Dictionary:
	var offer: Dictionary=Module.preview(state,data,actor_id,kind,params)
	if offer.ok: offer.duration+=1
	return offer

## 先执行真实即时规则再返回硬失败，确保已经改写的装备不能漏出候选。
func fail_after_instant(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary, offer: Dictionary) -> Dictionary:
	var applied: Dictionary=Module.apply(state,data,actor_id,kind,params,offer)
	if not applied.ok: return applied
	state.wallets[state.actors[actor_id].wallet_id].balance-=1
	return {"ok":false,"code":"InjectedCultivationApplyFailure"}

## 同一生产submit的开工与即时硬失败都回滚全世界，修复回调后原序号可以正常提交。
func hard_rollback() -> void:
	var pair: Dictionary=fresh()
	if pair.is_empty(): return
	var k=pair.current
	var before: Dictionary=k.snapshot(); var player: String=before.player_actor_id
	var command: Dictionary={"kind":"equip_weapon","params":{"weapon_id":""},"actor_id":player,"request_seq":1,"expected_revision":0}
	var original_apply: Callable=k._registry.commands.equip_weapon.apply
	k._registry.commands.equip_weapon.apply=fail_after_instant
	var failed: Dictionary=k.submit(command)
	check(failed.get("code")=="InjectedCultivationApplyFailure" and k.snapshot()==before,"即时真变更后硬失败全量回滚资产、占用、事实、ID、RNG、序号和缓存")
	k._registry.commands.equip_weapon.apply=original_apply
	check(k.submit(command).ok and k.submit(command)==k.snapshot().results[player+":1"],"硬失败不消耗请求序号且成功重试仍幂等")
	pair=fresh()
	if pair.is_empty(): return
	compare(pair,"move_local",{"place_id":"west.training"})
	k=pair.current; before=k.snapshot()
	var context: Dictionary=Interaction.for_role(before,k.content().data,"role.wei")
	command={"kind":"learn_skill","params":{"skill_id":"skill.cleave","context":context},"actor_id":player,"request_seq":before.request_sequences[player]+1,"expected_revision":before.revision}
	var original_preview: Callable=k._registry.commands.learn_skill.preview
	k._registry.commands.learn_skill.preview=stale_duration
	failed=k.submit(command)
	check(failed.get("code")=="InvalidTrainingDuration" and k.snapshot()==before,"旧开工占用师生并分配action后失败，真实世界所有字段保持原值")
	k._registry.commands.learn_skill.preview=original_preview
	check(k.submit(command).ok,"修复课时报价后同序号可开工")

## 未确认的对话取消只关闭旧报价，不提前收费、建立训练行动或发放技能。
func cancelled_offer() -> void:
	var pair: Dictionary=fresh()
	if pair.is_empty(): return
	compare(pair,"move_local",{"place_id":"west.training"})
	compare(pair,"talk",{"role_id":"role.wei"}); compare(pair,"dialogue_next")
	compare(pair,"dialogue_choose",{"option_id":"learn0"})
	var before: Dictionary=pair.current.snapshot()
	if not compare(pair,"dialogue_cancel").ok: return
	var after: Dictionary=pair.current.snapshot()
	check(after.wallets==before.wallets and after.actions==before.actions and after.facts==before.facts and after.rng==before.rng,"取消教学报价不消费、占用、发奖或抽随机")

## 测试观察器确认原规则已经结算，再故意失败；不将未提交候选作为世界恢复。
func fail_after_training(candidate: Dictionary, _data: Dictionary, _context: Dictionary) -> Dictionary:
	failed_tail_candidate=candidate.duplicate(true)
	candidate.wallets[candidate.actors[candidate.player_actor_id].wallet_id].balance-=1
	return {"ok":false,"code":"InjectedCultivationTailFailure"}

## 付费开工与旧WorldStep到期后都能整笔回滚，原序号重试仍与独立旧源码一致。
func tail_rollback() -> void:
	var pair: Dictionary=fresh()
	if pair.is_empty(): return
	compare(pair,"move_local",{"place_id":"west.training"})
	var k=pair.current; var before: Dictionary=k.snapshot(); var player: String=before.player_actor_id
	var context: Dictionary=Interaction.for_role(before,k.content().data,"role.wei")
	var params: Dictionary={"skill_id":"skill.cleave","context":context}
	var command: Dictionary={"kind":"learn_skill","params":params,"actor_id":player,"request_seq":before.request_sequences[player]+1,"expected_revision":before.revision}
	var observer: Dictionary={"id":"test.cultivation_tail","owner":"cultivation","before":[],"after":k._registry.observers.keys(),"callback":fail_after_training}
	k._registry.observers[observer.id]=observer
	failed_tail_candidate={}
	var result: Dictionary=k.submit(command)
	check(result.get("code")=="InjectedCultivationTailFailure" and k.snapshot()==before,"真实学费转账后尾部硬失败回滚全状态、RNG、ID、序号、缓存和参与者占用")
	if not failed_tail_candidate.is_empty():
		var action_id: String=failed_tail_candidate.actors[player].action_id
		check(not action_id.is_empty() and failed_tail_candidate.facts[action_id].phase=="started" and failed_tail_candidate.wallets["wallet.player"].balance==before.wallets["wallet.player"].balance-35,"故障发生在真实收费与训练事实之后")
	else: check(false,"开工尾部探针必须真实命中")
	k._registry.observers.erase(observer.id)
	if not compare(pair,"learn_skill",params).ok: return
	compare(pair,"advance")
	before=k.snapshot()
	var action_id: String=before.actors[player].action_id
	command={"kind":"advance","params":{},"actor_id":player,"request_seq":before.request_sequences[player]+1,"expected_revision":before.revision}
	k._registry.observers[observer.id]=observer
	failed_tail_candidate={}
	result=k.submit(command)
	check(result.get("code")=="InjectedCultivationTailFailure" and k.snapshot()==before,"旧到期结算和公共尾部失败回滚技能、疲劳、事实、tick、全部RNG和资源")
	if not failed_tail_candidate.is_empty():
		check(failed_tail_candidate.facts[action_id].phase=="completed" and failed_tail_candidate.actors[player].combat.learned.get("skill.cleave",-1)==0 and not failed_tail_candidate.actions.has(action_id),"故障发生在学习发奖和师生释放之后")
	else: check(false,"到期尾部探针必须真实命中")
	k._registry.observers.erase(observer.id)
	if not compare(pair,"advance").ok: return
	check(k.snapshot().facts[action_id].success and k.snapshot().actors[player].combat.learned.get("skill.cleave",-1)==0,"尾部失败后同序号只补交一次原学习成果")

## 原已收费学习遇重伤按旧规则记失败事实而不补发技能；状态变动经正式restore验证。
func interrupted_training() -> void:
	var pair: Dictionary=fresh()
	if pair.is_empty(): return
	compare(pair,"move_local",{"place_id":"west.training"})
	var context: Dictionary=Interaction.for_role(pair.current.snapshot(),pair.current.content().data,"role.wei")
	if not compare(pair,"learn_skill",{"skill_id":"skill.cleave","context":context}).ok: return
	compare(pair,"advance")
	var state: Dictionary=pair.current.snapshot(); var player: String=state.player_actor_id
	var action_id: String=state.actors[player].action_id
	state.actors[context.provider_actor_id].injury=3
	if not check(pair.current.restore(state).ok and pair.legacy.restore(state).ok,"真实受伤中断夹具可严格恢复"): return
	compare(pair,"advance")
	state=pair.current.snapshot()
	check(state.facts[action_id].phase=="completed" and not state.facts[action_id].success and not state.actors[player].combat.learned.has("skill.cleave"),"中断保持原失败终态，不退款重收或补发技能")
	check(state.actors[player].action_id.is_empty() and state.actors[context.provider_actor_id].action_id.is_empty(),"正常中断释放真实师生占用")

## 启动只跑本域注册和短兼容轨迹，长周期、视觉与全量发行不在此脚本声明。
func run_checks() -> void:
	oracle_integrity()
	if coverage() and not "--contracts-only" in OS.get_cmdline_user_args():
		training_flow()
		guard_order()
		routing_witness()
		hard_rollback()
		cancelled_offer()
		tail_rollback()
		interrupted_training()
	print("REGISTERED_CULTIVATION_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
