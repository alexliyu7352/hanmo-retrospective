## 家庭三个真实命令受信注册；短收养与原成年存档续跑保持全状态、回执及postclock顺序。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Commands=preload("res://core/domains/family/family_commands.gd")
const Family=preload("res://core/family_rules.gd")
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const Registry=preload("res://core/runtime/module_registry.gd")
const Codec=preload("res://core/save_codec.gd")
const Identity=preload("res://core/actor_identity.gd")
const F=preload("res://tests/companion_fixture.gd")
var checks: int=0
var failures: int=0
var comparisons: int=0
var adult_save: String=""
var legacy_path: String=""
var contracts_only: bool=false
var legacy=null
var kernels: Array=[]

## 只包裹原家庭调用以证明路由；故障在真实变更后触发，不能替代领域执行。
class Probe extends RefCounted:
	var hits: Dictionary={}
	var failure: String=""
	var changed_player: bool=false
	## 原预览只读且在公共守卫之后；计数用于检测隐藏旧分支。
	func preview(state: Dictionary,data: Dictionary,actor: String,kind: String,params: Dictionary) -> Dictionary:
		hits[kind+":preview"]=hits.get(kind+":preview",0)+1
		return Commands.preview(state,data,actor,kind,params)
	## 真正指定/退隐完成后才注入错误或Schema不变量破坏。
	func apply(state: Dictionary,data: Dictionary,actor: String,kind: String,params: Dictionary,offer: Dictionary) -> Dictionary:
		hits[kind+":apply"]=hits.get(kind+":apply",0)+1
		var result: Dictionary=Commands.apply(state,data,actor,kind,params,offer)
		changed_player=state.player_actor_id!=actor
		if result.ok and failure=="apply": return {"ok":false,"code":"InjectedFamilyApply"}
		if result.ok and failure=="tail": state.wallets[state.actors[actor].wallet_id].balance=-1
		return result

## 生产描述上只替换测试探针；继承Kernel依然经过真实注册、候选与尾验。
class TestKernel extends "res://core/world_kernel.gd":
	var probe=Probe.new()
	var mutation: String=""
	var selected: String=""
	var fail_begin: bool=false
	var begin_hits: int=0
	var fail_phase: String=""
	var boundaries: Dictionary={}
	## 保留原所有权，登记错误夹具由完整new_game拒绝且不动原世界。
	func _module_descriptors() -> Array:
		var rows: Array=super._module_descriptors(); var moved: Dictionary={}
		for descriptor in rows:
			if descriptor.id=="family":
				for entry in descriptor.commands.duplicate():
					entry.preview=probe.preview
					if entry.mode=="instant": entry.apply=probe.apply
					if entry.kind!=selected: continue
					match mutation:
						"missing": descriptor.commands.erase(entry)
						"duplicate": descriptor.commands.append(entry.duplicate(true))
						"gate": entry.gate_id="family_early"
						"mode": entry.mode="instant"; entry.apply=probe.apply; entry.erase("compat_action")
						"owner": moved=entry; descriptor.commands.erase(entry)
						"bridge": entry.erase("compat_action")
						"extra_bridge": entry.compat_action="legacy_begin_action"
				if mutation=="extra": descriptor.commands.append({"kind":"family_invented","mode":"instant","preview":probe.preview,"apply":probe.apply,"gate_id":"legacy_kernel"})
			for entry in descriptor.phases:
				if not entry.id in ["actions_step","social_step","clock_commit","military_return_complete","family_complete"]: continue
				var run: Callable=entry.run; var id: String=entry.id
				entry.run=func(state: Dictionary,data: Dictionary,context: Dictionary) -> Dictionary:
					var result: Dictionary=run.call(state,data,context)
					boundaries[id]=state.duplicate(true)
					if result.ok and fail_phase==id: return {"ok":false,"code":"InjectedFamilyPhase"}
					return result
		if not moved.is_empty():
			for descriptor in rows:
				if descriptor.id=="tasks": descriptor.commands.append(moved)
		return rows
	## 实际收养开始后可失败；费用预留、占用、action/fact和ID须随候选一起放弃。
	func _begin_action(state: Dictionary,actor: String,kind: String,params: Dictionary,offer: Dictionary) -> Dictionary:
		var result: Dictionary=super._begin_action(state,actor,kind,params,offer)
		if kind=="family_adopt":
			begin_hits+=1
			if result.ok and fail_begin: return {"ok":false,"code":"InjectedFamilyBegin"}
		return result

## 显式路径指向已核SHA的原成年存档和精确旧Kernel，不在测试中伪造日期或家庭事实。
func _init() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg=="--contracts-only": contracts_only=true
		if arg.begins_with("--adult-save="): adult_save=arg.trim_prefix("--adult-save=")
		if arg.begins_with("--legacy-kernel="): legacy_path=arg.trim_prefix("--legacy-kernel=")
	call_deferred("run_checks")

## 失败包含具体边界并返回非零进程码；计数不是完整发行结论。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 建立正常Alpha内容世界，无加速年龄或领域预填。
func fresh():
	var k=TestKernel.new(); kernels.append(k)
	check(k.new_game("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json").ok,"正常Alpha开局")
	return k

## 所有测试请求使用正式当前玩家、版本与未占用序号。
func request(k,kind: String,params: Dictionary={}) -> Dictionary:
	var state: Dictionary=k.snapshot(); var actor: String=state.player_actor_id
	return {"kind":kind,"params":params,"actor_id":actor,"request_seq":state.request_sequences[actor]+1,"expected_revision":state.revision}

## 保存与恢复均经真实Codec及Schema，不删去历史、RNG、缓存或家庭来源。
func roundtrip(k,label: String) -> void:
	var before: Dictionary=k.snapshot()
	var saved: Dictionary=Codec.decode(Codec.encode(before,0,"registered-family",k.content()),k.content())
	check(saved.ok and k.restore(saved.state).ok and k.snapshot()==before,"全状态Codec往返 "+label)

## 同一请求在旧源与注册核逐字段相同；重复提交只返回原回执不重入处理器。
func send(k,kind: String,params: Dictionary={}) -> bool:
	var command: Dictionary=request(k,kind,params); var before: Dictionary=k.snapshot(); var old_result: Dictionary={}
	if legacy!=null:
		if not check(legacy.restore(before).ok,"旧源恢复真实命令前状态 "+kind): return false
		old_result=legacy.submit(command)
	var prior: Dictionary=k.probe.hits.duplicate(true); var begin_before: int=k.begin_hits
	var result: Dictionary=k.submit(command)
	if not check(result.ok,"正式命令 "+kind+" "+str(result)): return false
	var after: Dictionary=k.snapshot(); var hits: Dictionary=k.probe.hits.duplicate(true)
	if kind in Family.KINDS:
		check(hits.get(kind+":preview",0)==prior.get(kind+":preview",0)+1,"注册preview真实命中 "+kind)
		if kind=="family_adopt": check(k.begin_hits==begin_before+1,"收养唯一旧begin桥")
		else: check(hits.get(kind+":apply",0)==prior.get(kind+":apply",0)+1,"注册apply真实命中 "+kind)
	check(k.submit(command)==result and k.snapshot()==after and k.probe.hits==hits and k.begin_hits==begin_before+(1 if kind=="family_adopt" else 0),"重复请求全状态与回执幂等 "+kind)
	if legacy!=null:
		check(old_result==result and legacy.snapshot()==after,"旧源码完整state/result等价 "+kind); comparisons+=1
	return true

## 注册集合直接取真实KINDS；缺项与错误owner/gate/mode启动失败后仍保留原世界。
func coverage() -> bool:
	var registry: Dictionary=Registry.build(Builtin.descriptors())
	if not check(registry.ok,"生产Registry完整构建 "+str(registry.get("code"))): return false
	var expected: Array=Family.KINDS.duplicate(); expected.sort(); var actual: Array=[]
	for kind in registry.commands:
		if registry.commands[kind].owner=="family": actual.append(kind)
	actual.sort()
	if not check(actual==expected and Commands.command_kinds()==Family.KINDS,"生产注册集合等于Family.KINDS"): return false
	check(registry.commands.family_adopt.mode=="action" and registry.commands.family_adopt.get("compat_action")=="legacy_begin_action" and registry.commands.family_adopt.apply==null and not registry.actions.has("family_adopt"),"收养明确旧action桥且没有伪造生命周期")
	for kind in expected:
		for mutation in ["missing","duplicate","gate","owner"]:
			var k=fresh(); var before: Dictionary=k.snapshot(); var content: Dictionary=k.content(); k.mutation=mutation; k.selected=kind
			var result: Dictionary=k.new_game()
			var expected_code: String={"missing":"MissingBuiltinCommand","duplicate":"DuplicateCommand","gate":"InvalidBuiltinCommand","owner":"UnknownCompatAction" if kind=="family_adopt" else "InvalidBuiltinCommand"}[mutation]
			check(result.get("code")==expected_code and k.snapshot()==before and k.content()==content,"错误注册首错且保留世界 "+kind+" "+mutation+" "+str(result))
	for row in [["family_adopt","mode"],["family_adopt","bridge"],["family_designate","extra_bridge"],["family_retire","extra_bridge"],["family_retire","extra"]]:
		var k=fresh(); k.selected=row[0]; k.mutation=row[1]
		check(not k.new_game().ok,"兼容桥和额外命令边界拒绝 "+str(row))
	return true

## 旧公共守卫及能力缺失首错不提前，比较全过程快照证明预览纯只读。
func guards(k,kind: String,params: Dictionary) -> void:
	if legacy==null: return
	var state: Dictionary=k.snapshot(); var actor: String=state.player_actor_id
	for variant in ["original","unavailable","busy","following","in_army","no_capability","no_capability_busy"]:
		var candidate: Dictionary=state.duplicate(true)
		if variant=="unavailable": candidate.actors[actor].alive=false; candidate.actors[actor].available=false
		if variant in ["busy","no_capability_busy"]: candidate.actors[actor].action_id="missing"
		if variant=="following": candidate.actors[actor].location={"kind":"Following","leader_actor_id":"actor.chen"}
		if variant=="in_army": candidate.actors[actor].location={"kind":"InArmy","army_id":"missing"}
		var data: Dictionary=k._catalog.data.duplicate(true); var old_data: Dictionary=legacy._catalog.data.duplicate(true)
		if variant in ["no_capability","no_capability_busy"]: k._catalog.data.required_capabilities.erase("family"); legacy._catalog.data.required_capabilities.erase("family")
		var before: Dictionary=candidate.duplicate(true)
		check(k._preview(candidate,kind,params,actor)==legacy._preview(candidate,kind,params,actor) and candidate==before,"旧守卫首错且预览只读 "+kind+" "+variant)
		k._catalog.data=data; legacy._catalog.data=old_data
	check(k._preview(state,kind,params,"missing").get("code")=="UnknownActor","未知人物仍最先拒绝")

## 两半日收养在旧postclock完成；创建前不参与social，冻结来源/ID/年龄/费用不漂移。
func adoption() -> void:
	var k=fresh()
	if not send(k,"move_local",{"place_id":"west.estate"}): return
	var params: Dictionary={"context":F.context(k,"role.estate_steward"),"display_name":"阿宁"}; var before: Dictionary=k.snapshot()
	guards(k,"family_adopt",params)
	k.fail_begin=true
	check(k.submit(request(k,"family_adopt",params)).get("code")=="InjectedFamilyBegin" and k.snapshot()==before,"真实收养begin后失败回滚预留、来源、ID和占用")
	k.fail_begin=false
	if not send(k,"family_adopt",params): return
	var pending: Dictionary=k.snapshot(); var id: String=pending.actors[pending.player_actor_id].action_id
	check(pending.tick==0 and pending.actors.size()==before.actors.size() and pending.facts[id].kind=="family_adoption_pending" and pending.wallets["wallet.player"].balance==800 and pending.wallets["wallet.player"].reserved[id]==40,"开始只预留40且不提前生人")
	roundtrip(k,"pending")
	if not send(k,"advance"): return
	before=k.snapshot(); check(before.tick==1 and before.actions[id].remaining_ticks==1 and before.family.children.is_empty(),"首半日尚无孩子")
	roundtrip(k,"before-completion")
	k.fail_phase="family_complete"
	check(k.submit(request(k,"advance")).get("code")=="InjectedFamilyPhase" and k.snapshot()==before and k.boundaries.family_complete.actors.size()==before.actors.size()+1,"真实postclock创建孩子后失败整候选回滚")
	k.fail_phase=""
	if not send(k,"advance"): return
	var after: Dictionary=k.snapshot(); var child: String=after.family.children.keys()[0]
	for phase in ["actions_step","social_step","clock_commit","military_return_complete"]:
		check(k.boundaries[phase].family.children.is_empty() and k.boundaries[phase].actions.has(id),"阶段未提前finish或释放收养 "+phase)
	check(k.boundaries.social_step.tick==1 and k.boundaries.clock_commit.tick==2 and k.boundaries.military_return_complete.tick==2,"社会在旧时钟、家庭在新时钟之后")
	check(after.tick==2 and Identity.age_years(after,child)==16 and after.actors[child].created_tick==2 and after.facts[id].child_actor_id==child and after.family.children[child]==id,"真实ID与来源绑定、创建tick/年龄保持")
	check(not after.actions.has(id) and after.actors[after.player_actor_id].action_id.is_empty() and after.wallets["wallet.player"].balance==760 and after.wallets["wallet.steward"].balance==40 and after.wallets["wallet.player"].reserved.is_empty(),"完工释放占用且支付一次")
	check(k.preview("family_designate",{"heir_actor_id":child}).get("code")=="FamilyHeirNotAdult","未成年仍不得指定")
	roundtrip(k,"completed")

## 即时原规则真正变更后失败须回滚，退隐时已改玩家也不泄漏到权威状态。
func failures_after_apply(k,kind: String,params: Dictionary) -> void:
	var before: Dictionary=k.snapshot()
	for failure in ["apply","tail"]:
		k.probe.failure=failure
		check(not k.submit(request(k,kind,params)).ok and k.snapshot()==before,"真实apply后完整候选回滚 "+kind+" "+failure)
		if kind=="family_retire": check(k.probe.changed_player,"失败探针确已执行真实玩家接续")
	k.probe.failure=""
	var offer: Dictionary=k.preview(kind,params); offer.family.place_id="missing"
	var candidate: Dictionary=before.duplicate(true)
	check(Commands.apply(candidate,k.content().data,before.player_actor_id,kind,params,offer).get("code")=="StaleFamilyQuote" and candidate==before,"即时apply重新核验冻结报价 "+kind)

## 核SHA原件经正式迁移续跑成年指定/退隐，绝不重跑1452步或改birth_tick。
func succession() -> void:
	if adult_save.is_empty(): print("REGISTERED_FAMILY_ADULT_SAVE=NOT_RUN"); return
	var k=fresh(); var bytes: String=FileAccess.get_file_as_string(adult_save)
	if not check(bytes.sha256_text()=="f786e31864af2a8db55aab640a34a40c47264ca37deed0b965dac5342a28a99b","成年档原字节符合已备份trace SHA"): return
	var decoded: Dictionary=Codec.decode(bytes,k.content())
	if not check(decoded.ok,"原成年档正式Codec迁移 "+str(decoded.get("code"))): return
	if not check(k.restore(decoded.state).ok,"真实原成年状态通过restore"): return
	var before: Dictionary=k.snapshot(); var parent: String=before.player_actor_id; var child: String=before.family.children.keys()[0]
	check(before.tick==1442 and Identity.age_years(before,child)==18 and before.family.successions.is_empty(),"沿真实两年自然成长档续接")
	var params: Dictionary={"heir_actor_id":child}
	guards(k,"family_designate",params); failures_after_apply(k,"family_designate",params)
	if not send(k,"family_designate",params): return
	roundtrip(k,"designated"); guards(k,"family_retire",{}); failures_after_apply(k,"family_retire",{})
	before=k.snapshot()
	if not send(k,"family_retire"): return
	var after: Dictionary=k.snapshot()
	check(after.tick==before.tick and after.player_actor_id==child and after.actors[parent].alive and after.family.successions.size()==1 and after.actors[child].birth_tick==before.actors[child].birth_tick,"继任不改年龄/日期且保留前代与完整控制链")
	check(after.estates["estate.riverside"].owner_actor_id==child and after.wallets["wallet.office"]==before.wallets["wallet.office"] and after.wallets["wallet.player"].balance==0 and after.wallets[after.actors[child].wallet_id].balance==before.wallets["wallet.player"].balance,"真实私产移交公库不动")
	roundtrip(k,"retired")
	check(send(k,"estate_withdraw",{"estate_id":"estate.riverside","item_id":"item.grain","quantity":1}) and send(k,"move_local",{"place_id":"west.street"}) and send(k,"wait") and send(k,"advance"),"继任者持真实所有权继续生活")
	print("REGISTERED_FAMILY_ADULT_SAVE=PASS")

## 局部生产注册门覆盖短收养及已备份成年原档；未提供旧源时明确不算旧源对照。
func run_checks() -> void:
	if not coverage(): finish(); return
	if contracts_only: finish(); return
	if not legacy_path.is_empty():
		var script=load(legacy_path)
		if not check(script!=null,"精确旧Kernel源码可加载"): finish(); return
		legacy=script.new()
		if not check(legacy.new_game("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json").ok,"旧Kernel真实开局"): finish(); return
	adoption(); succession(); finish()

## 汇总只声明实际运行的局部检查，完整发行/长程回归继续属于后续整合门。
func finish() -> void:
	for k in kernels: k._registry={}
	kernels.clear(); legacy=null
	print("REGISTERED_FAMILY_LEGACY_COMPARISONS=",comparisons)
	print("REGISTERED_FAMILY_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
