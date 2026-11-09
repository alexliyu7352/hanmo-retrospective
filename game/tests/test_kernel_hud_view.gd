## 真实HUD文字与控件的固定旧oracle；只查当前人的日期/名字/钱/显示HP，不验证整场战斗或GUI。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Main=preload("res://ui/main.gd")
const Screen=preload("res://ui/screen_composer.gd")
const Personal=preload("res://ui/personal_battle_screen.gd")
const Art=preload("res://ui/art_catalog.gd")
const ORACLE="aa0570db8893bdec9b70c29e0feaa6dfab63721adaed5f296ac9001cb61da44c"
const CASES=["day0","day1","day719","day720","maximum","dynamic_name","missing_name","bad_appearance","personal_priority","personal_fallback","personal_result","personal_closed","military","busy","audio_off"]
var checks: int=0
var failures: int=0
var leaf: GDScript

## 只记录实际snapshot动态派发；不伪造新hud_view实现或人物身份。
class ObservedKernel extends "res://core/world_kernel.gd":
	var snapshots: int=0
	## 完整快照保原义，计数用于证明HUD姓名不再专门复制整态。
	func snapshot() -> Dictionary:
		snapshots+=1; return super.snapshot()

## 在引擎和根节点就绪后运行实际生产HUD。
func _init() -> void: call_deferred("_run")

## 断言与退出码绑定，任何引擎诊断另算失败。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 个人战只提供原active与HUD读取的明确字段；不冒称完整战斗Schema或战术渲染夹具。
func battle(id: String,player: String,controller: String,hp: int,phase: String="planning") -> Dictionary:
	return {"id":id,"kind":"personal","phase":phase,"controller_id":controller,"units":{player:{"hp":hp}}}

## 正式tiny开局后只改变本门源字段；跨年/错误身份案例是只读投影输入，不推进世界。
func fixture(kind: String) -> Dictionary:
	var kernel=ObservedKernel.new()
	check(kernel.new_game("scenario.procurement","res://data/generated/procurement.hmcatalog.json").ok,"正式tiny HUD开局")
	var state: Dictionary=kernel._state; var player: String=state.player_actor_id
	state.actors[player].hp=73; state.wallets[state.actors[player].wallet_id].balance=51
	state.wallets[state.actors[player].wallet_id].reserved={"display.fixture":30}
	if kind.begins_with("day"): state.tick=int(kind.trim_prefix("day"))
	if kind=="maximum": state.tick=2147483647
	if kind=="dynamic_name": state.actors[player].display_name="新取的姓名"
	if kind=="missing_name": state.actors[player].prototype_id="missing"
	if kind=="bad_appearance": state.actors[player].appearance=false
	state.battles={}
	if kind=="personal_priority":
		state.battles.fallback=battle("fallback",player,"actor.chen",19)
		state.battles.controlled=battle("controlled",player,player,12)
		state.battles.later=battle("later",player,"actor.chen",8)
	if kind=="personal_fallback":
		state.battles.earlier=battle("earlier",player,"actor.chen",19)
		state.battles.later=battle("later",player,"actor.chen",8)
	if kind=="personal_result": state.battles.result=battle("result",player,player,0,"result")
	if kind=="personal_closed": state.battles.closed=battle("closed",player,player,1,"closed")
	if kind=="military": state.battles.military={"id":"military","kind":"military","phase":"planning","controller_actor_id":player,"units":{player:{"hp":1}}}
	var main=Main.new()
	for node in [main.storage,main.journey_view,main._ui_font_scale,main._ambience]: main.add_child(node)
	main._ambience.add_child(main._ambience.music); main._ambience.add_child(main._ambience.effect)
	main.kernel=kernel; main._data=kernel.content().data; main._started=true
	main._advancing=kind=="busy"; main._ambience.enabled=kind!="audio_off"
	return {"kernel":kernel,"main":main}

## 用原个人战active选择器决定唯一上下文，原和新HUD均只消费本次明确选择。
func frame(kind: String,finite: bool,capturing: bool) -> Dictionary:
	var f: Dictionary=fixture(kind); var state: Dictionary=f.kernel._state
	var before: Dictionary=state.duplicate(true); var data_before: Dictionary=f.kernel._catalog.data.duplicate(true)
	var selected: String=Personal.active(state)
	var actor: Dictionary=state.actors[state.player_actor_id].duplicate(true)
	if not selected.is_empty(): actor.hp=state.battles[selected].units[state.player_actor_id].hp
	var stage=Control.new(); root.add_child(stage); stage.size=Vector2(1280,720)
	f.kernel.snapshots=0
	var renderer: Callable=Screen.hud
	if finite:
		var view: Dictionary=f.kernel.call("hud_view",selected) if f.kernel.has_method("hud_view") else leaf.query(state,f.main._data,selected)
		if check(view.get("ok",false),"合法HUD来源必须投影成功 "+kind): renderer.callv([f.main,stage,view])
	else: renderer.callv([f.main,stage,state,actor])
	var label: Label=stage.find_child("WorldStatus",true,false)
	check(label!=null,"真实HUD状态标签存在 "+kind)
	check(f.kernel.snapshots==(1 if capturing else 0),"HUD姓名整态复制次数 "+kind+"/"+str(f.kernel.snapshots))
	var labels: Array=[]; var buttons: Array=[]
	for node in stage.find_children("*","Label",true,false): labels.append(node.text)
	for button in stage.find_children("*","Button",true,false): buttons.append({"text":button.text,"kind":button.get_meta("command_kind",""),"target":button.get_meta("target_id",""),"disabled":button.disabled})
	var menu: Control=stage.find_child("SystemMenu",true,false)
	check(menu!=null and not menu.visible,"菜单原先隐藏 "+kind)
	Screen._toggle_menu(menu); check(menu.visible,"菜单原展开 "+kind)
	Screen._toggle_menu(menu); check(not menu.visible,"菜单原收起 "+kind)
	check(f.kernel._state==before and f.kernel._catalog.data==data_before and not f.main.storage._busy,"HUD不写世界/目录/Native "+kind)
	var result: Dictionary={"selected":selected,"text":label.text if label!=null else "","labels":labels,"buttons":buttons}
	stage.free(); f.main.free(); return result

## 新薄口只返回八个标量字段；空名降级、坏画像无关性与个人战绑定由固定旧oracle保留。
func leaf_checks() -> void:
	check(leaf.query({},{} )=={"ok":false,"code":"NoWorld"},"空世界明确拒绝")
	var f: Dictionary=fixture("personal_priority"); var state: Dictionary=f.kernel._state; var data: Dictionary=f.main._data
	var before: Dictionary=state.duplicate(true); seed(722); var expected_random: int=randi(); seed(722)
	var value: Dictionary=leaf.query(state,data,"controlled")
	var keys: Array=value.keys(); keys.sort()
	check(keys==["actor_id","balance","day_index","display_name","half_day","hp","ok","revision"],"有限八键，无actor/wallet/battle快照")
	check(value.hp==12 and leaf.query(state,data).hp==73 and value.balance==51,"个人battleHP和worldHP分离，钱保持总额")
	value.hp=999; value.balance=0; value.display_name="forged"
	check(leaf.query(state,data,"controlled").hp==12 and state==before and randi()==expected_random,"返回值隔离且不耗随机")
	for fault in ["tick","actor","wallet","balance","kind","phase","unit","hp","missing_battle"]:
		var altered: Dictionary=before.duplicate(true); var context: String="controlled"
		match fault:
			"tick": altered.tick=false
			"actor": altered.actors[altered.player_actor_id]=[]
			"wallet": altered.wallets.erase(altered.actors[altered.player_actor_id].wallet_id)
			"balance": altered.wallets[altered.actors[altered.player_actor_id].wallet_id].balance=[]
			"kind": altered.battles.controlled.kind=false
			"phase": altered.battles.controlled.phase="closed"
			"unit": altered.battles.controlled.units.erase(altered.player_actor_id)
			"hp": altered.battles.controlled.units[altered.player_actor_id].hp=true
			"missing_battle": context="absent"
		var unchanged: Dictionary=altered.duplicate(true)
		check(leaf.query(altered,data,context)=={"ok":false,"code":"InvalidHudViewSource"} and altered==unchanged,"坏必要源不伪造HUD "+fault)
	f.main.free()

## 摘要固定原HUD完整文字/按钮/选择输出，正常运行绝不重新生成预期。
func digest(value: Variant) -> String: return JSON.stringify(value,"",true,true).sha256_text()

## 先采原生产HUD与选择器，再在同输入运行有限口；不启动完整Main或存储生命周期。
func _run() -> void:
	root.size=Vector2i(1280,720)
	var kernel=Kernel.new(); check(kernel.new_game("scenario.procurement","res://data/generated/procurement.hmcatalog.json").ok,"正式tiny准备HUD")
	var ready: Dictionary=Art.prepare_content(kernel.content()); if not check(ready.ok,"原tiny资源可用"): quit(1); return
	Art.commit_content(ready)
	var capture: String=""
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--capture-oracle="): capture=arg.trim_prefix("--capture-oracle=")
	var available: bool=FileAccess.file_exists("res://core/views/hud_view.gd")
	if available:
		leaf=load("res://core/views/hud_view.gd")
		if not check(leaf!=null and leaf.has_method("query"),"HUD叶可加载"): quit(1); return
	var outputs: Dictionary={}
	for kind in CASES: outputs[kind]=frame(kind,available,not capture.is_empty())
	if not capture.is_empty():
		var file=FileAccess.open(capture,FileAccess.WRITE)
		file.store_string(JSON.stringify({"composer_sha256":FileAccess.get_sha256("res://ui/screen_composer.gd"),"selector_sha256":FileAccess.get_sha256("res://ui/personal_battle_screen.gd"),"sha256":digest(outputs),"outputs":outputs},"\t",true,true)+"\n"); file.close()
		print("ORIGINAL_HUD_ORACLE=",digest(outputs))
	else:
		check(digest(outputs)==ORACLE,"原真实HUD文字/按钮/个人战选择oracle保持")
		check(available,"有限HUD叶存在"); check(Kernel.new().has_method("hud_view"),"实际Kernel薄口存在")
		if available: leaf_checks()
	print("KERNEL_HUD_VIEW_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
