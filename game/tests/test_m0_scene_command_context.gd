## 真实SceneHost生成的场景请求沿原Main/Kernel执行；只核M0标量读取和旧守卫，不冒充GUI或存读验收。
extends SceneTree
const Host=preload("res://ui/scene_host.gd")
const Art=preload("res://ui/art_catalog.gd")
var checks: int=0
var failures: int=0
var paths_reported: bool=false

## 所有规则仍委托实际Kernel，只观察新查询、快照和提交载荷。
class ObservedKernel extends "res://core/world_kernel.gd":
	var contexts: Array=[]
	var snapshots: int=0
	var commands: Array=[]
	var previews: int=0
	var quote_fault_at: int=0
	var quote_fault_kind: String=""
	## 默认玩家参数不能被按钮提供的人物替换。
	func command_context(actor_id: String="") -> Dictionary:
		contexts.append(actor_id); return super.command_context(actor_id)
	## 原结果尾部仍有快照，本门只消除命令组装的复制。
	func snapshot() -> Dictionary:
		snapshots+=1; return super.snapshot()
	## 预览继续运行正式领域规则，不以测试成功替代权限校验。
	func preview(kind: String, params: Dictionary={}, actor_id: String="") -> Dictionary:
		previews+=1
		var result: Dictionary=super.preview(kind,params,actor_id)
		if result.ok and previews==quote_fault_at:
			if quote_fault_kind=="revision": result.revision+=1
			else: result={"ok":false,"code":"SceneProbeDenied","revision":result.revision}
		return result
	## 原请求副本用于对照，实际提交仍由完整原Kernel负责。
	func submit(value: Dictionary) -> Dictionary:
		commands.append(value.duplicate(true)); return super.submit(value)
	## 只清观察记录，不改世界、序号或缓存。
	func reset_observations() -> void:
		contexts=[]; snapshots=0; commands=[]; previews=0

## Main必须真实入树供祖先/可见性判断，但本门不初始化音频、存储、整个界面或自动推进。
class SceneMain extends "res://ui/main.gd":
	var renders: int=0
	## 请求专测不运行完整Main启动副作用。
	func _ready() -> void:
		set_process(false)
	## 实际结果收尾仍调用本方法，只记录重绘而不重造被测Host。
	func _render() -> void:
		renders+=1

## 等待引擎/真实场景树就绪后建立Host。
func _init() -> void:
	call_deferred("_run")

## 所有断言独立计数并参与进程退出码。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 释放未启用的Main成员Node与真实Host，不留下音频/字体/存储孤儿。
func dispose(main) -> void:
	main._ambience.music.free(); main._ambience.effect.free(); main._ambience.free()
	main.journey_view.free(); main.storage.free(); main._ui_font_scale.free()
	if main.get_parent()!=null: main.get_parent().remove_child(main)
	main.free()

## 原作者经正常stage并完整Art准备，真实Layered/Host激活可达候选；不手造成功Host或绕过validate_request。
func fixture() -> Dictionary:
	var kernel=ObservedKernel.new()
	if not check(kernel.new_game("scenario.procurement","res://data/generated/procurement.hmcatalog.json").ok,"正式场景fixture开局"): return {}
	var prepared: Dictionary=Art.prepare_content(kernel.content())
	if not check(prepared.ok,"真实表现模板/原图通过校验 "+str(prepared.get("code",""))): return {}
	Art.commit_content(prepared)
	var main=SceneMain.new(); main.kernel=kernel; main._started=true; main._data=kernel.content().data; main.size=Vector2(1280,720)
	root.add_child(main)
	if not paths_reported:
		print("M0_SCENE_PATHS ",JSON.stringify({"user_data_dir":OS.get_user_data_dir(),"computed_native_directory":main.storage.native.directory,"native_operations":0}))
		paths_reported=true
	var host=Host.new(); main.add_child(host); host.size=Vector2(1280,720)
	var configured: Dictionary=host.configure(kernel.snapshot(),main._data,"west.street","city",kernel.preview)
	if not check(configured.ok and configured.mode=="layered","真实Host配置为分层入口 "+str(configured)): dispose(main); return {}
	var candidate: String=""
	for value in host.describe_projection().hotspot_candidates:
		if value.command_kind=="move_local" and value.params.place_id=="west.office": candidate=value.id
	if not check(not candidate.is_empty(),"真实投影含可达官署候选"): dispose(main); return {}
	var emitted: Array=[]
	host.command_requested.connect(func(value): emitted.append(value.duplicate(true)))
	var activated: Dictionary=host.activate_candidate(candidate)
	if not check(activated.ok and emitted.size()==1 and activated.request==emitted[0],"实际Layered与Host生成同一完整请求 "+str(activated)): dispose(main); return {}
	var before: Dictionary=kernel.snapshot(); kernel.reset_observations()
	main._rest_auto=true; main.journey_view.auto_running=true
	return {"kernel":kernel,"main":main,"host":host,"request":activated.request,"before":before}

## 成功场景请求必须保留真实目标/epoch与原revision，只补原玩家下一序号。
func accepted_scene() -> void:
	var f: Dictionary=fixture()
	if f.is_empty(): return
	var request: Dictionary=f.request.duplicate(true); var expected: Dictionary=request.duplicate(true)
	expected.request_seq=f.before.request_sequences[f.before.player_actor_id]+1
	var result: Dictionary=f.main._scene_request(request,f.host)
	check(result.ok,"真实Main接受Host候选并完成MoveLocal "+str(result))
	check(f.kernel.contexts==[""] and f.kernel.snapshots==1,"仅默认玩家context组装，原结果尾部保留一次快照")
	check(f.kernel.previews==2 and f.kernel.commands==[expected] and request==f.request,"Host与Main两次真实preview且完整请求未被刷新或改写")
	var after: Dictionary=f.kernel.snapshot()
	check(after.actors[after.player_actor_id].location.place_id=="west.office" and after.revision==f.before.revision+1 and after.tick==f.before.tick,"原移动、revision与零额外半日保持")
	check(not f.main._rest_auto and not f.main.journey_view.auto_running,"成功预览后原auto停止点保持")
	f.before=after; f.kernel.reset_observations(); f.main._rest_auto=true; f.main.journey_view.auto_running=true
	rejected(f,f.main._scene_request(request,f.host),"StalePreview",1,0,"旧请求重复点击")
	dispose(f.main)

## 每个失败都留住原世界、请求与auto授权；只有达到相应边界才可读取默认玩家context或预览。
func rejected(f: Dictionary,result: Dictionary,code: String,queries: int,previews: int,label: String) -> void:
	check(not result.get("ok",false) and result.get("code")==code,label+"：原错误顺序 "+str(result))
	check(f.kernel.contexts==([""] if queries==1 else []) and f.kernel.snapshots==0,label+"：默认查询且无组装快照")
	check(f.kernel.previews==previews and f.kernel.commands.is_empty(),label+"：预览边界与零提交")
	check(f.kernel.snapshot()==f.before and f.main._rest_auto and f.main.journey_view.auto_running,label+"：世界与auto不变")

## 真实生命周期、树归属及隐藏状态先于busy/请求字段，不能用伪造成功Host代替这些门。
func host_guards() -> void:
	for kind in ["not_started","null","outside_tree","not_ancestor","hidden","queued"]:
		var f: Dictionary=fixture()
		if f.is_empty(): continue
		var host: Control=f.host
		var detached: bool=false
		if kind=="not_started": f.main._started=false
		if kind=="null": host=null
		if kind in ["outside_tree","not_ancestor"]:
			f.main.remove_child(host); detached=true
			if kind=="not_ancestor": root.add_child(host)
		if kind=="hidden": host.hide()
		if kind=="queued": host.queue_free()
		f.main._advancing=true
		var bad: Dictionary=f.request.duplicate(true); bad.actor_id=false
		rejected(f,f.main._scene_request(bad,host),"StaleSceneRequest",0,0,"Host首门 "+kind)
		if detached: host.free()
		dispose(f.main)
	for kind in ["busy","storage"]:
		var f: Dictionary=fixture()
		if f.is_empty(): continue
		if kind=="busy": f.main._advancing=true
		else: f.main._storage_intent="load"
		var bad: Dictionary=f.request.duplicate(true); bad.kind=false
		rejected(f,f.main._scene_request(bad,f.host),"ActorBusy",0,0,"忙碌先于类型 "+kind)
		dispose(f.main)

## 原六字段类型检查先于身份，尤其布尔和浮点不能成为版本或epoch整数。
func request_types() -> void:
	for row in [["kind",false],["params",[]],["actor_id",["actor.player"]],["expected_revision",true],["expected_revision",0.0],["target_ref","west.office"],["scene_epoch",true],["scene_epoch",0.0]]:
		var f: Dictionary=fixture()
		if f.is_empty(): continue
		var bad: Dictionary=f.request.duplicate(true); bad[row[0]]=row[1]
		rejected(f,f.main._scene_request(bad,f.host),"InvalidSceneRequest",0,0,"原请求类型 "+str(row))
		dispose(f.main)

## 有效NPC也不是当前玩家；Main的actor/revision先于Host旧epoch，与topic入口顺序不同。
func identity_guards() -> void:
	for actor in ["actor.chen","missing"]:
		var f: Dictionary=fixture()
		if f.is_empty(): continue
		var bad: Dictionary=f.request.duplicate(true); bad.actor_id=actor; bad.expected_revision=-1; bad.scene_epoch=-1
		rejected(f,f.main._scene_request(bad,f.host),"StaleSceneRequest",1,0,"人物先于版本/epoch "+actor)
		dispose(f.main)
	var f: Dictionary=fixture()
	if f.is_empty(): return
	var bad: Dictionary=f.request.duplicate(true); bad.expected_revision=-1; bad.scene_epoch=-1
	rejected(f,f.main._scene_request(bad,f.host),"StalePreview",1,0,"Main旧版本先于Host旧epoch")
	dispose(f.main)

## 实际Host复核完整候选和自己的epoch，目标/参数/多余字段都不能被Main重建时默默丢掉。
func candidate_guards() -> void:
	for kind in ["epoch","disabled","reconfigured","target","params","extra","missing_method"]:
		var f: Dictionary=fixture()
		if f.is_empty(): continue
		var request: Dictionary=f.request.duplicate(true); var host: Control=f.host
		var code: String="InvalidSceneCandidate"
		if kind=="epoch": request.scene_epoch+=1; code="SceneClosed"
		if kind=="disabled": host.set_input_enabled(false); code="SceneInputUnavailable"
		if kind=="reconfigured":
			check(host.configure(f.before,f.main._data,"west.street","city",f.kernel.preview).ok,"真实Host同版本重开")
			code="SceneClosed"
		if kind=="target": request.target_ref.id="west.grain"
		if kind=="params": request.params.place_id="west.grain"
		if kind=="extra": request.untrusted=true
		if kind=="missing_method":
			host=Control.new(); f.main.add_child(host); code="InvalidSceneHost"
		f.kernel.reset_observations()
		rejected(f,f.main._scene_request(request,host),code,1,0,"Host候选门 "+kind)
		dispose(f.main)

## 真实preview只注入拒绝或revision故障，分别证明Host与Main两层守卫，不创造假的领域成功。
func preview_guards() -> void:
	for stage in [1,2]:
		for kind in ["revision","denied"]:
			var f: Dictionary=fixture()
			if f.is_empty(): continue
			f.kernel.quote_fault_at=stage; f.kernel.quote_fault_kind=kind
			var code: String="SceneProbeDenied" if kind=="denied" else ("StaleSceneProjection" if stage==1 else "StalePreview")
			rejected(f,f.main._scene_request(f.request,f.host),code,1,stage,"原preview边界 "+str(stage)+"/"+kind)
			dispose(f.main)

## 全部案例均从实际可达Host请求出发；只检查本片入口，不调用存读或图形输入矩阵。
func _run() -> void:
	accepted_scene(); host_guards(); request_types(); identity_guards(); candidate_guards(); preview_guards()
	print("M0_SCENE_COMMAND_CONTEXT_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
