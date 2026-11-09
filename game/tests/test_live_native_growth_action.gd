## 真实旧Kernel／Main成长行动的独立oracle与按需native缓存门；不改变规则或用preview代替错页检查。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Main=preload("res://ui/main.gd")
const Art=preload("res://ui/art_catalog.gd")
const Schema=preload("res://core/state_schema.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const Codec=preload("res://core/backends/legacy_projection_codec.gd")
const BASELINE="91587fed507d7915d35a1e7ac9dc2cfdd5374340"
const EXTENSION="res://native_live_growth/hanmo_world.gdextension"
var checks: int=0
var failures: int=0
var fixture: Dictionary={}
var rows: Array=[]
var native_enabled: bool=false
var timings: Array=[]

## 所有初始化在SceneTree就绪后运行，缺新入口作为明确红灯而非脚本异常。
func _init() -> void:call_deferred("run")
## 断言失败保留标签并令进程失败。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value:failures+=1;printerr("FAIL: ",label)
	return value
## 让真实Main延迟回调完成，不另写时间推进器。
func settle() -> void:
	await process_frame;await process_frame;await process_frame
## 正式内容新开局，不为native构造替代世界。
func opened():
	var k=Kernel.new();var result: Dictionary=k.new_game(fixture.scenario,fixture.catalog)
	if not check(result.ok,"正式开局 "+str(result)):return null
	if native_enabled:k.configure_native_growth_read(true,fixture.source_commit)
	return k
## 使用原公共序号／版本构造真实请求，可指定另一位实际人物。
func request(k,kind: String,params: Dictionary={},actor_id: String="") -> Dictionary:
	var context: Dictionary=k.command_context(actor_id)
	return {"kind":kind,"params":params,"actor_id":context.actor_id,"request_seq":context.next_request_seq,"expected_revision":context.revision}
## 前置始终走真实submit，并记录失败本身。
func send(k,kind: String,params: Dictionary={},actor_id: String="") -> Dictionary:
	var result: Dictionary=k.submit(request(k,kind,params,actor_id));check(result.ok,"公共命令 "+kind+" "+str(result));return result
## 实际Main持该Kernel打开成长页；不造替代按钮或提交逻辑。
func host(k):
	var prepared: Dictionary=Art.prepare_content(k.content())
	if not check(prepared.ok,"正式tiny表现 "+str(prepared)):return null
	Art.commit_content(prepared)
	var m=Main.new();m.kernel=k;m._data=k.content().data;m._started=true;root.add_child(m);await settle()
	# Main启动开发配置后，测试显式配置当前同源已提交世界。
	if native_enabled:k.configure_native_growth_read(true,fixture.source_commit)
	m._open_growth();await settle();return m
## 只记录行动块的真实文字／按钮，整页其它用途不扩大本门。
func controls(m) -> Array:
	var result: Array=[]
	for name in ["GrowthActionStatus","GrowthAdvance"]:
		var node=m.find_child(name,true,false)
		if node is Label:result.append([name,node.text])
		elif node is Button:result.append([name,node.text,node.disabled,node.get_meta("command_kind","")])
	return result
## 同输入旧summary及实际控件冻结，读前后世界与全局RNG都不改变。
func capture(m,label: String) -> void:
	var before: Dictionary=m.kernel.save_context();seed(90877);var expected: int=randi();seed(90877)
	var summary: Dictionary=m.kernel.character_summary();m._render();await settle()
	check(m.kernel.save_context()==before and randi()==expected,"只读保持世界和RNG "+label)
	rows.append({"label":label,"summary":summary,"controls":controls(m),"advance_preview":m.kernel.preview("advance")})
## 原Schema认可的未序列化live世界必须可读；失败先明确输出来源，不以JSON绕过类型差异。
func source_contract() -> void:
	var k=opened();if k==null:return
	check(Schema.validate(k._state,k._catalog,k._registry).ok,"原Factory live初态通过原Schema")
	var strict: Dictionary=Codec.project(k._state,k._catalog,k._registry)
	check(strict.get("code")=="UnsupportedProjectionValue" and "non-String key" in strict.get("path",""),"原strict project继续拒绝原生StringName键，旧协议不暗改")
	var view: Dictionary=k.character_summary();print("LIVE_SOURCE_ACTION=",view.action)
	check(view.action.is_empty(),"同一未经JSON归一化的live世界应成功读出idle")

## 原本真实idle／wait／按钮推进，再用相同revision恢复证明preview不能兜底旧行动。
func original_flow() -> void:
	var a=opened();var b=opened()
	if a==null or b==null:return
	var m=await host(a)
	if m==null:return
	await capture(m,"idle")
	var wait_request: Dictionary=request(a,"wait");check(a.submit(wait_request).ok,"玩家wait rev1")
	await capture(m,"player_wait")
	check(a.snapshot().revision==1 and m.find_child("GrowthAdvance",true,false)!=null,"真实wait成长出现继续半日")
	var npc: String="base::actor.wei"
	if not b._state.actors.has(npc):
		for id in b._state.actors:
			if id!=b._state.player_actor_id and b._state.actors[id].alive and b._state.actors[id].available:npc=id;break
	check(b.submit(request(b,"wait",{},npc)).ok,"另一真实人物wait")
	var replacement: Dictionary=b.snapshot()
	check(replacement.revision==a.snapshot().revision and replacement.actors[replacement.player_actor_id].action_id.is_empty(),"同rev1来源玩家确实idle")
	check(a.restore(replacement).ok,"同Kernel真实restore同rev1")
	check(a.preview("advance").ok,"原advance仍可推进NPC，不能作为错页兜底")
	await capture(m,"same_revision_npc_wait")
	check(m.find_child("GrowthAdvance",true,false)==null and a.character_summary().action.is_empty(),"同rev恢复必须消失旧继续按钮")
	send(a,"advance");send(a,"wait");m._render();await settle()
	var advance: Button=m.find_child("GrowthAdvance",true,false)
	if check(advance!=null and not advance.disabled,"真实Main继续按钮可用"):
		var tick: int=a._state.tick;advance.pressed.emit();await settle()
		check(a._state.tick==tick+1 and a._state.actors[a._state.player_actor_id].action_id.is_empty(),"实际按钮经过Main／Kernel完成玩家wait")
	await capture(m,"button_completed")
	m.queue_free();await settle()
## 复用原作者收费教学，再真正开始两半日修炼，不注入已学技能。
func practice_flow() -> void:
	var k=opened()
	if k==null:return
	if k._state.player_actor_id!="base::actor.player":check(false,"practice门明确pin原玩家作者");return
	send(k,"move_local",{"place_id":"base::west.training"})
	var context: Dictionary=Interaction.for_role(k._state,k._catalog.data,"base::role.wei")
	send(k,"learn_skill",{"skill_id":"base::skill.cleave","context":context});send(k,"advance");send(k,"advance")
	send(k,"practice_skill",{"skill_id":"base::skill.cleave","ticks":2})
	var m=await host(k)
	if m==null:return
	await capture(m,"practice_two")
	check(k.character_summary().action=={"kind":"practice_skill","skill_id":"base::skill.cleave","remaining_ticks":2},"真实training params映射原三个标量")
	var advance: Button=m.find_child("GrowthAdvance",true,false)
	if check(advance!=null and not advance.disabled,"真实practice继续按钮"):
		advance.pressed.emit();await settle();await capture(m,"practice_one")
		check(k.character_summary().action.get("remaining_ticks")==1,"真实半日后native剩余一半日")
	m.queue_free();await settle()
## 计数器只由测试静态类持有，不把Kernel绑定进持久validator。
class SchemaCount extends RefCounted:
	static var hits: int=0
	## 包装真实末尾validator并原样返回，用于证明project只有一次完整Schema。
	static func validate(state: Dictionary,data: Dictionary,original: Callable) -> Dictionary:
		hits+=1;return original.call(state,data)

## 只替真实最后validator的计数包装，不删除任何原校验。
class CountingKernel extends Kernel:
	## 原描述顺序与validator结果完全保留。
	func _module_descriptors() -> Array:
		var descriptors: Array=super._module_descriptors()
		for module in descriptors:
			for entry in module.validators:
				if entry.id=="interaction.sessions":entry.callback=SchemaCount.validate.bind(entry.callback)
		return descriptors

## 首次live读只走一次Schema，缓存读走零次，并且释放Kernel即释放cache／Bridge。
func schema_once_and_release() -> void:
	var k=CountingKernel.new();check(k.new_game(fixture.scenario,fixture.catalog).ok,"Schema计数实际Kernel开局")
	k.configure_native_growth_read(true,fixture.source_commit);SchemaCount.hits=0
	check(k.character_summary().action.is_empty() and SchemaCount.hits==1,"实际project含且仅含一次全Schema，无JSON往返")
	k.character_summary();k.character_summary()
	check(SchemaCount.hits==1,"重复cache读取不重Schema")
	var owner=weakref(k);var cache=weakref(k._backend._native_growth_cache);var bridge=bridge_ref(k)
	k=null
	check(owner.get_ref()==null and cache.get_ref()==null and bridge.get_ref()==null,"Backend／cache没有反持Kernel或永久Callable环")

## 受信validator只持Kernel弱引用；回调内显式失效用于探测旧attempt不能污染新代。
class Reentry extends RefCounted:
	static var armed: bool=false
	static var hits: int=0
	## 保原最后validator结果，只在明确armed时失效一次。
	static func validate(state: Dictionary,data: Dictionary,original: Callable,owner: WeakRef) -> Dictionary:
		var result: Dictionary=original.call(state,data)
		if result.ok and armed:
			armed=false;hits+=1;owner.get_ref().invalidate_native_growth_read()
		return result

## 同步重入只来自真实Schema已有扩展钩子，不用替代codec或Bridge。
class ReentryKernel extends Kernel:
	## 精确包装原validator，引用用WeakRef避免测试自己制造永久环。
	func _module_descriptors() -> Array:
		var descriptors: Array=super._module_descriptors()
		for module in descriptors:
			for entry in module.validators:
				if entry.id=="interaction.sessions":entry.callback=Reentry.validate.bind(entry.callback,weakref(self))
		return descriptors

## 旧attempt中的Schema回调变代后只能报superseded，不能把旧错误／DTO发布到新代。
func reentrant_invalidation() -> void:
	var k=ReentryKernel.new();check(k.new_game(fixture.scenario,fixture.catalog).ok,"重入实际Kernel开局")
	k.configure_native_growth_read(true,fixture.source_commit);var before: Dictionary=k.snapshot()
	var generation: int=diag(k).source_generation;Reentry.hits=0;Reentry.armed=true
	var action: Dictionary=k.character_summary().action;print("REENTRANT_ACTION=",action)
	check(Reentry.hits==1 and diag(k).source_generation==generation+1 and k.snapshot()==before,"真实Schema回调只失效一次，世界未改")
	check(action.get("code")=="NativeGrowthReadSuperseded" and action.get("path")=="/native_growth/generation","旧attempt返回明确superseded")
	check(not diag(k).attempted and not diag(k).has_bridge and diag(k).status=="unattempted","新代仍未尝试且不保留旧Bridge／结果")
	if action.get("code")=="NativeGrowthReadSuperseded":
		var attempts: int=diag(k).counts.attempts
		check(k.character_summary().action.is_empty() and diag(k).counts.attempts==attempts+1 and diag(k).status=="ready","下一次读取才为新代构建一次，重入旧结果不遗留")

## 明确测试候选尾Schema失败，生产apply和validators原样执行。
class TailKernel extends Kernel:
	var fail_tail: bool=false
	## 仅在真实apply后破坏私有候选；返回成功让原末尾Schema实际拒绝。
	func _apply(state: Dictionary,kind: String,params: Dictionary,actor_id: String,offer: Dictionary) -> Dictionary:
		var result: Dictionary=super._apply(state,kind,params,actor_id,offer)
		if result.ok and fail_tail:state.actors[state.player_actor_id].fatigue=-1
		return result

## 只比较生产诊断的有限深副本，阶段次数不拿进程耗时代替。
func diag(k) -> Dictionary:return k.native_growth_read_diagnostics()
## 缓存代不重复project／encode／Bridge安装／query，reads和cache_hits可增加。
func stage_counts(k) -> Array:
	var c: Dictionary=diag(k).counts
	return [c.attempts,c.project,c.encode,c.bridge_creations,c.install,c.query]
## 私有Bridge只作受信寿命观察，测试不安装或改变它。
func bridge_ref(k) -> WeakRef:
	return weakref(k._backend._native_growth_cache._bridge)
## 读完成后收集真正live阶段成本，project已包括唯一Schema调用。
func measured(k,label: String) -> void:
	var d: Dictionary=diag(k);timings.append({"label":label,"diagnostics":d})
	check(d.project_includes_schema and d.total_usec.keys().size()==4,"project总计明确含Schema "+label)
	for key in ["project","encode","install","query"]:
		check(typeof(d.total_usec[key])==TYPE_INT and d.total_usec[key]>=0 and typeof(d.last_usec[key])==TYPE_INT and d.last_usec[key]>=0,"实际阶段计时非负整数 "+key)
## 默认关闭与开启未开页，真实Main两个配置点都不得急建Bridge。
func default_and_main() -> void:
	ProjectSettings.set_setting("development/native_growth_action_enabled",false)
	ProjectSettings.set_setting("development/native_projection_source_commit",fixture.source_commit)
	var k=Kernel.new();check(k.new_game(fixture.scenario,fixture.catalog).ok,"默认off开局")
	var plain: Dictionary=k.character_summary();var d: Dictionary=diag(k)
	check(not d.enabled and stage_counts(k)==[0,0,0,0,0,0],"默认off完整summary零native")
	var off_main=Main.new();off_main.kernel=k;off_main._data=k.content().data;off_main._started=true
	root.add_child(off_main);await settle();off_main._open_growth();await settle()
	check(k.character_summary()==rows[0].summary and controls(off_main)==rows[0].controls,"关闭配置真实idle summary／控件与原oracle相等")
	send(k,"wait");off_main._render();await settle()
	check(k.character_summary()==rows[1].summary and controls(off_main)==rows[1].controls and stage_counts(k)==[0,0,0,0,0,0],"关闭配置真实wait控件原样且所有native阶段零次")
	off_main.queue_free();await settle();send(k,"advance");plain=k.character_summary()
	k.configure_native_growth_read(true,fixture.source_commit)
	check(stage_counts(k)==[0,0,0,0,0,0] and not diag(k).attempted,"开关开但未读仍零native")
	k.character_summary();check(k.character_summary()==plain,"native成功仅替换等价action，其余全部字段保持")
	ProjectSettings.set_setting("development/native_growth_action_enabled",true)
	var m=Main.new();root.add_child(m);await settle()
	check(diag(m.kernel).enabled and stage_counts(m.kernel)==[0,0,0,0,0,0],"真实Main._ready配置但不做原生工作")
	var initial=m.kernel
	var started: Dictionary=m.start_content(fixture.scenario,fixture.catalog);await settle()
	check(started.ok and m.kernel!=initial and diag(m.kernel).enabled and stage_counts(m.kernel)==[0,0,0,0,0,0],"Main.start_content成功换Kernel立即配置，非成长页仍不读")
	m._open_growth();await settle()
	check(stage_counts(m.kernel)==[1,1,1,1,1,1],"真实Main首次成长按需一条native链")
	var original=m.kernel;var before: Dictionary=m.kernel.snapshot();var counts: Array=stage_counts(m.kernel)
	var failed: Dictionary=m.start_content("missing.scenario",fixture.catalog);await settle()
	check(not failed.ok and m.kernel==original and m.kernel.snapshot()==before and stage_counts(m.kernel)==counts,"失败新局保旧Main Kernel和cache，错误重绘不重建")
	m.queue_free();await settle()
## 成功唯一提交才失效；命令重放与首错／Schema失败不失效。
func cache_submission() -> void:
	var k=opened();if k==null:return
	check(stage_counts(k)==[0,0,0,0,0,0],"未开成长页零attempt")
	var first: Dictionary=k.character_summary();var first_diag: Dictionary=diag(k)
	check(stage_counts(k)==[1,1,1,1,1,1] and first_diag.status=="ready","第一次完整真实project/encode/install/query各一次")
	for index in range(3):k.character_summary();k.preview("wait")
	check(stage_counts(k)==[1,1,1,1,1,1] and diag(k).counts.cache_hits>=3,"重复summary与preview命中原缓存")
	var altered: Dictionary=diag(k);altered.counts.project=999;altered.last_usec.project=-1
	first.action["caller_only"]=true
	check(diag(k).counts.project==1 and diag(k).last_usec.project>=0 and k.character_summary().action.is_empty(),"诊断及action均无可写别名")
	var generation: int=diag(k).source_generation;k.configure_native_growth_read(true,fixture.source_commit)
	check(diag(k).source_generation==generation and diag(k).status=="ready","重复相同配置不失效")
	var old_bridge=bridge_ref(k);var command: Dictionary=request(k,"wait");var result: Dictionary=k.submit(command)
	check(result.ok and diag(k).source_generation==generation+1 and not diag(k).attempted and old_bridge.get_ref()==null,"真实submit末尾失效并及时释放旧Bridge")
	check(stage_counts(k)==[1,1,1,1,1,1],"submit本身不构建下一代")
	var busy: Dictionary=k.character_summary();check(busy.action.get("kind")=="wait" and stage_counts(k)==[2,2,2,2,2,2],"次读才重建且行动来自新源")
	generation=diag(k).source_generation;var held=bridge_ref(k);var counts: Array=stage_counts(k)
	var replay: Dictionary=command.duplicate(true);replay.expected_revision=-10
	check(k.submit(replay)==result and diag(k).source_generation==generation and held.get_ref()!=null,"重复请求ok早返回不失效")
	var bad: Dictionary=k.submit(request(k,"wait"));check(not bad.ok and diag(k).source_generation==generation and stage_counts(k)==counts,"失败preview不失效")
	var saved: Dictionary=k.snapshot();saved.actors[saved.player_actor_id].fatigue=-1
	check(not k.restore(saved).ok and diag(k).source_generation==generation and held.get_ref()!=null,"失败restore保原cache／Bridge")
	check(not k.new_game("missing.scenario",fixture.catalog).ok and diag(k).source_generation==generation,"失败new_game不失效")
	send(k,"advance");check(stage_counts(k)==counts and held.get_ref()==null,"成功advance只失效无急建")
	check(k.character_summary().action.is_empty() and stage_counts(k)==[3,3,3,3,3,3],"advance后的唯一lazy重建")
	measured(k,"wait_advance")
	var tail=TailKernel.new();check(tail.new_game(fixture.scenario,fixture.catalog).ok,"尾失败实际Kernel开局")
	tail.configure_native_growth_read(true,fixture.source_commit);tail.character_summary();tail.fail_tail=true
	generation=diag(tail).source_generation;var before: Dictionary=tail.snapshot();command=request(tail,"wait");result=tail.submit(command)
	check(result.get("code")=="InvalidActorState" and tail.snapshot()==before and diag(tail).source_generation==generation,"真实候选Schema尾失败保世界和cache")
	tail.fail_tail=false;check(tail.submit(command).ok and diag(tail).source_generation==generation+1,"原失败序号可重试且只成功时失效")
## 到原有粮私庄实际长休再取消；不往测试世界赠粮或改作者。
func cancel_submission() -> void:
	var rest=opened();if rest==null:return
	send(rest,"move_local",{"place_id":"base::west.estate"})
	send(rest,"long_rest",{"estate_id":"base::estate.riverside","duration":2});rest.character_summary()
	var counts: Array=stage_counts(rest);var generation: int=diag(rest).source_generation;send(rest,"rest_cancel")
	check(diag(rest).source_generation==generation+1 and stage_counts(rest)==counts,"真实取消也只推进来源代次")
	check(rest.character_summary().action.is_empty() and stage_counts(rest)[0]==counts[0]+1,"取消后次读单次重建空闲")
## 所有发布与受信替换触发失效，两个Kernel从不共享Bridge或代次。
func publications_and_trusted() -> void:
	var a=opened();var b=opened();if a==null or b==null:return
	send(a,"wait");a.character_summary();b.character_summary()
	var other: Dictionary=diag(b);var other_state: Dictionary=b.snapshot();var old=bridge_ref(a);var generation: int=diag(a).source_generation
	check(a.new_game(fixture.scenario,fixture.catalog).ok and diag(a).source_generation==generation+1 and old.get_ref()==null,"同Kernel成功new_game清理旧Bridge")
	a.character_summary();var context: Dictionary=b.save_context();old=bridge_ref(a);generation=diag(a).source_generation
	check(a.restore_with_catalog(context.state,context.catalog,context.registry).ok and diag(a).source_generation==generation+1 and old.get_ref()==null,"restore_with_catalog完整发布失效")
	check(diag(b)==other and b.snapshot()==other_state,"另一Kernel世界／缓存计数完全隔离")
	for field in ["_state","_catalog","_registry"]:
		a.character_summary();old=bridge_ref(a);generation=diag(a).source_generation
		a.set(field,a.get(field).duplicate(true))
		check(diag(a).source_generation==generation+1 and old.get_ref()==null and not diag(a).attempted,"受信setter替换失效 "+field)
	send(a,"wait",{},"base::actor.wei");a.character_summary();generation=diag(a).source_generation
	var revision: int=a._state.revision
	var invalid_player: Dictionary=a.snapshot();invalid_player.player_actor_id="base::actor.wei"
	check(a.restore(invalid_player).get("code")=="InvalidSuccessionPlayer" and diag(a).source_generation==generation,"单改player按原Schema拒绝且不失效，不伪造继任")
	var active_id: String=a._state.actors["base::actor.wei"].action_id
	a._state.actions[active_id].params["explicit_invalidation_probe"]="changed"
	a.invalidate_native_growth_read()
	check(Schema.validate(a._state,a._catalog,a._registry).ok and diag(a).source_generation==generation+1 and a._state.revision==revision and a.character_summary().action.is_empty(),"同rev原地改真实action后明确invalidate，当前玩家仍idle")
	old=bridge_ref(a);generation=diag(a).source_generation
	a._state.actors[a._state.player_actor_id].display_name="显式受信新名";a.invalidate_native_growth_read()
	check(old.get_ref()==null and diag(a).source_generation==generation+1,"原地其他投影来源明确失效")
	a.character_summary();old=bridge_ref(a);a.configure_native_growth_read(false,fixture.source_commit)
	check(old.get_ref()==null and not diag(a).enabled,"关闭配置释放旧Bridge")
	var counts: Array=stage_counts(a);a.character_summary();check(stage_counts(a)==counts,"关闭后读取不再做native工作")
## Schema认可开放float，project必须拒而且缓存失败；不会复用上一代wait或装作idle。
func unavailable_and_failures() -> void:
	var k=opened();if k==null:return
	send(k,"wait");var m=await host(k);if m==null:return
	var old=bridge_ref(k);var source: Dictionary=k.snapshot();source.actors[source.player_actor_id]["live_test_extra"]=1.25
	check(Schema.validate(source,k._catalog,k._registry).ok,"合法actor extra float通过原Schema")
	check(k.restore(source).ok and old.get_ref()==null,"合法float来源真实restore并清旧Bridge")
	var committed: Dictionary=send(k,"advance");var after: Dictionary=k.snapshot();var prior: Array=stage_counts(k)
	var failed: Dictionary=k.character_summary().action
	check(failed.get("ok")==false and failed.get("code")=="UnsupportedProjectionValue" and not failed.is_empty(),"codec失败为明确unavailable，不回旧wait／idle")
	check(stage_counts(k)==[prior[0]+1,prior[1]+1,prior[2],prior[3],prior[4],prior[5]] and diag(k).status=="failed","失败project一次缓存，后续encode/Bridge没有发生")
	m._render();await settle();var labels: Array=[];all_labels(m,labels)
	check("原生行动摘要暂不可用" in "\n".join(labels) and m.find_child("GrowthAdvance",true,false)==null,"真实Growth明确不可用且没有继续按钮")
	prior=stage_counts(k);k.character_summary();m._render();await settle()
	check(stage_counts(k)==prior and committed.ok and k.snapshot()==after,"失败缓存不重试，先前成功命令及完整世界保持")
	k._state.actors[k._state.player_actor_id].erase("live_test_extra");k.invalidate_native_growth_read()
	check(k.character_summary().action.is_empty() and diag(k).status=="ready","显式修复失效后恢复真正idle")
	k.configure_native_growth_read(true,"invalid-source-commit");failed=k.character_summary().action;prior=stage_counts(k);k.character_summary()
	check(failed.get("ok")==false and failed.get("code")=="InvalidProjectionProvenance" and stage_counts(k)==prior,"错误完整源commit明确失败并缓存")
	k.configure_native_growth_read(true,fixture.source_commit);send(k,"wait")
	var id: String=k._state.actors[k._state.player_actor_id].action_id
	k._state.actions[id].params["native_string_probe"]=String.chr(0xfeff)+"text"
	check(Schema.validate(k._state,k._catalog,k._registry).ok,"真实wait开放params的BOM可通过原Schema")
	k.invalidate_native_growth_read();prior=stage_counts(k);failed=k.character_summary().action
	check(failed.get("ok")==false and failed.get("code")=="BridgeUnrepresentableString" and stage_counts(k)[5]==prior[5]+1,"真实Bridge query深值拒绝映射unavailable")
	prior=stage_counts(k);k.character_summary();check(stage_counts(k)==prior and not diag(k).has_bridge,"query拒绝缓存失败并释放candidate Bridge")
	measured(k,"failure_stages")
	m.queue_free();await settle()
## 只提取真实标签以确认失败文字，不以特殊测试UI代替Growth。
func all_labels(node: Node,result: Array) -> void:
	if node is Label:result.append(node.text)
	for child in node.get_children():all_labels(child,result)

## 旧源只能首次写oracle；以后用完全同内容读原输出，不用新入口自证相等。
func oracle(args: PackedStringArray) -> void:
	for argument in args:
		if argument.begins_with("--record-oracle="):
			var path: String=argument.trim_prefix("--record-oracle=")
			if not check(failures==0 and rows.size()==6 and fixture.source_commit==BASELINE and not FileAccess.file_exists(path),"只从原915完整源首次记录"):return
			var file=FileAccess.open(path,FileAccess.WRITE);file.store_var({"source_commit":BASELINE,"catalog_sha256":fixture.catalog_sha256,"rows":rows},false);file.close();return
		if argument.begins_with("--oracle="):
			var file=FileAccess.open(argument.trim_prefix("--oracle="),FileAccess.READ)
			var expected: Variant=file.get_var(false) if file!=null else null
			if not check(expected is Dictionary and expected.source_commit==BASELINE and expected.catalog_sha256==fixture.catalog_sha256,"精确原源和内容oracle"):return
			check(expected.rows==rows,"原全部summary／控件／preview逐条保持")
			return
	check(false,"必须指定原oracle路径")
## 全新无扩展进程先证明失败缓存；后来显式加载也必须由明确invalidate开启新attempt。
func missing_extension() -> void:
	check(not ClassDB.class_exists("HanmoWorldBridge"),"本进程起点确未加载扩展")
	native_enabled=true
	var k=opened();if k==null:return
	var action: Dictionary=k.character_summary().action
	check(action.get("code")=="NativeGrowthBridgeUnavailable" and stage_counts(k)==[1,0,0,0,0,0],"缺扩展失败不project或构建")
	k.character_summary();check(stage_counts(k)==[1,0,0,0,0,0],"缺扩展一次失败被缓存")
	check(GDExtensionManager.load_extension(EXTENSION)==OK,"测试明确加载991c扩展")
	check(k.character_summary().action==action and stage_counts(k)==[1,0,0,0,0,0],"外部扩展到位不自行改变失败代次")
	k.invalidate_native_growth_read()
	check(k.character_summary().action.is_empty() and stage_counts(k)==[2,1,1,1,1,1],"明确invalidate后一次成功原生读取")

## 显式加载已冻本地库，不把headless结果冒充GUI自动加载。
func run() -> void:
	var args: PackedStringArray=OS.get_cmdline_user_args();fixture=JSON.parse_string(FileAccess.get_file_as_string("res://data/live-native-growth-fixture.json"))
	print("SOURCE=",fixture.source_commit," CONTENT=",fixture.pack_set_digest," USER_DIR=",OS.get_user_data_dir())
	if "--missing-extension" in args:
		missing_extension();print("LIVE_NATIVE_MISSING: ",checks," checks, ",failures," failures");quit(0 if failures==0 else 1);return
	native_enabled="--native" in args
	if native_enabled:
		var status: int=GDExtensionManager.load_extension(EXTENSION)
		check(status in [OK,GDExtensionManager.LOAD_STATUS_ALREADY_LOADED] and ClassDB.class_exists("HanmoWorldBridge"),"实际加载冻结991c库")
		var k=Kernel.new()
		if not check(k.has_method("configure_native_growth_read") and k.has_method("invalidate_native_growth_read") and k.has_method("native_growth_read_diagnostics"),"live native三入口缺失红门"):
			print("LIVE_NATIVE_GROWTH: ",checks," checks, ",failures," failures");quit(1);return
	if "--cancel-only" in args:
		cancel_submission();print("LIVE_CANCEL: ",checks," checks, ",failures," failures");quit(0 if failures==0 else 1);return
	if "--reentry-only" in args:
		reentrant_invalidation();print("LIVE_REENTRY: ",checks," checks, ",failures," failures");quit(0 if failures==0 else 1);return
	if "--source-contract-only" in args:
		source_contract();print("LIVE_SOURCE_CONTRACT: ",checks," checks, ",failures," failures");quit(0 if failures==0 else 1);return
	await original_flow();await practice_flow();oracle(args)
	if native_enabled:
		await default_and_main();cache_submission();cancel_submission();publications_and_trusted();await unavailable_and_failures();schema_once_and_release();reentrant_invalidation()
		print("LIVE_TIMINGS=",JSON.stringify(timings,"",true))
	print("LIVE_NATIVE_GROWTH: ",checks," checks, ",failures," failures, ",rows.size()," oracle rows")
	quit(0 if failures==0 else 1)
