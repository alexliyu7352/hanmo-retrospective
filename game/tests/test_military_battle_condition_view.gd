## 条件检查栏的单一代表门：原训练/行军来源、有限页权限、独立副本与真实展开控件；可另接同节点真实native输入核逐字段对照。
extends "res://tests/test_military_march_conditions.gd"
const Page=preload("res://core/views/military_battle_page.gd")
const Screen=preload("res://ui/military_battle_screen.gd")
const Combat=preload("res://core/military_combat.gd")
var native_catalog_path: String=""
var native_state_path: String=""

## 只替代通用文字装配；条件控件、折叠信号与来源格式均调用真正Screen，不替代世界或条件查询。
class TextHost extends RefCounted:
	## 有界Label装配保持原宿主参数，供检查栏独立入树，不宣称完整Main实景验收。
	func _text(parent: Node, text: String, size: int) -> Label:
		var label: Label=Label.new(); label.text=text; label.add_theme_font_size_override("font_size",size); parent.add_child(label); return label

## 两次真实四课与一次已完成行军形成九条来源；接敌沿既有领域夹具边界建立，不把夹具搬位称为宏观行军验收。
func condition_fixture() -> Dictionary:
	var f: Dictionary=march_fixture(true,7,100000); commission(f)
	for _course in range(2):
		var action: Dictionary=start(f)
		if action.is_empty(): return {}
		for _lesson in range(4):
			if not course_one(f,action).ok: return {}
	muster(f)
	if not f.has("army_a") or f.army_a.is_empty(): return {}
	var enemy: Dictionary=MF.mobilize(f,true,false)
	check(enemy.ok and enemy.get("success",false),"敌军沿真实任官与编组流程建立")
	if not enemy.ok or not enemy.get("success",false): return {}
	f.army_b=enemy.details.army_id
	var movement: Dictionary=begin_march(f,[6,5])
	if movement.is_empty() or not march_one(f,movement).ok: return {}
	check(not f.state.actions.has(movement.id),"一段真实短行军已经到达并释放行动")
	var request: Dictionary=MF.contact(f,[f.army_a,f.army_b],[f.unit,f.enemy_unit],f.actor)
	var opened: Dictionary=Combat.create(f.state,f.data,request)
	check(opened.ok,"既有桥接器从明确领域接敌来源建立会战")
	if not opened.ok: return {}
	f.battle_id=opened.details.battle_id
	return f

## 逐条追溯真实事实中的前后值与稳定身份；不把页面中的自报来源当作验证依据。
func source_checks(f: Dictionary, condition: Dictionary) -> void:
	check(condition.training==f.state.military_units[f.unit].training and condition.fatigue==f.state.military_units[f.unit].fatigue,"当前条件严格取真实部队而非人物疲劳/兵种默认")
	check(condition.source=={"kind":"military_unit","unit_id":f.unit},"有限当前来源具有真实部队身份")
	check(condition.initial_source=={"garrison_id":f.camp,"unit_id":f.unit,"training":10,"fatigue":7},"作者初值与原驻军身份独立显示")
	check(condition.history_count==9 and condition.history.size()==6,"八课加一次真实行军保总数，输出只保最近六条")
	var previous_tick: int=f.state.tick+1; var kinds: Dictionary={}
	for row in condition.history:
		check(row.tick<previous_tick,"历史按真实半日逆序且没有复制事件"); previous_tick=row.tick
		check(f.state.facts.has(row.source_id) and f.state.facts.has(row.supply_fact_id),"来源与供给都引用实际永久事实")
		var fact: Dictionary=f.state.facts[row.source_id]; kinds[row.kind]=true
		if row.kind=="military_training":
			var lessons: Array=fact.lessons.filter(func(lesson): return lesson.tick==row.tick)
			check(fact.kind==row.kind and fact.terms.unit_id==f.unit and lessons.size()==1,"每条课时绑定原部队和唯一半日")
			if lessons.size()==1:
				for key in ["supply_fact_id","training_before","training_after","fatigue_before","fatigue_after"]: check(row[key]==lessons[0][key],"原训练来源数值 "+key)
		else:
			check(row.kind=="military_march" and row.rule=="terrain_march_v1" and row.action_id==fact.movement.action_id and row.plan_id==fact.movement.plan_id,"行军只显示真实原执行/计划/模型身份")
			check(row.training_before==fact.units[f.unit].training and row.training_after==row.training_before and row.fatigue_before==fact.units[f.unit].fatigue_before and row.fatigue_after==fact.units[f.unit].fatigue_after,"行军前后值逐字段引用原收据")
		for secret in ["terms","fit_ids","movement","uncut_execution","army_id","commander_actor_id","fee_paid"]: check(not row.has(secret),"有限历史不泄露整事实字段 "+secret)
	check(kinds.has("military_training") and kinds.has("military_march"),"最近来源同时包含实际训练与行军，没有虚构休整")

## 只改变独立反例副本的控制边界；原有门的每个拒绝条件都必须同时移除条件数值与来源历史。
func privacy_checks(f: Dictionary) -> void:
	var page: Dictionary=Page.query(f.state,f.data,f.battle_id)
	check(not page.units[f.enemy_unit].has("condition"),"敌军即使有真实profile也无私有条件历史")
	for patch in [{"command_unit_ids":[]},{"controller_side":1},{"command_scope":"unit"},{"command_army_id":"unowned"},{"phase":"result"}]:
		var state: Dictionary=f.state.duplicate(true); var battle: Dictionary=state.battles[f.battle_id]; battle.merge(patch,true)
		if patch.get("command_scope")=="unit": battle.units[f.unit].commander_actor_id=f.enemy
		check(not Page.query(state,f.data,f.battle_id).units[f.unit].has("condition"),"旧控制门拒绝时条件不可见 "+str(patch))
	for key in ["withdrawn","fit","morale"]:
		var state: Dictionary=f.state.duplicate(true); state.battles[f.battle_id].units[f.unit][key]={"withdrawn":true,"fit":[],"morale":0}[key]
		check(not Page.query(state,f.data,f.battle_id).units[f.unit].has("condition"),"失去当前行动控制时不增私有读取口 "+key)
	var legacy: Dictionary=f.state.duplicate(true); legacy.military_units[f.unit].erase("training"); legacy.military_units[f.unit].erase("fatigue")
	check(not Page.query(legacy,f.data,f.battle_id).units[f.unit].has("condition"),"无profile的旧来源不凭空初始化训练50/疲劳0")

## 原Screen实际创建摘要与来源Label并响应toggle；纯展开不修改有限输入，不借宿主的state/data。
func inspector_checks(condition: Dictionary) -> void:
	var source: Dictionary=condition.duplicate(true); var screen=Screen.new(); screen.host=TextHost.new(); root.add_child(screen)
	screen._inspection=VBoxContainer.new(); screen.add_child(screen._inspection); screen._inspect_condition(condition)
	var summary: Label=screen.find_child("MilitaryConditionSummary",true,false)
	var details: Label=screen.find_child("MilitaryConditionHistory",true,false)
	var toggle: Button=screen.find_child("MilitaryConditionHistoryToggle",true,false)
	check(summary!=null and summary.text.contains(str(condition.training)) and summary.text.contains(str(condition.fatigue)),"真实检查栏摘要显示当前条件")
	check(details!=null and not details.visible and details.text.contains("较早 3 条") and details.text.contains(condition.history[0].source_id),"折叠来源含真实ID及明确省略标签")
	toggle.set_pressed(true)
	check(details.visible and condition==source,"真实展开信号只改变显示，不写条件或世界")
	toggle.set_pressed(false); check(not details.visible,"来源记录可再次收起")
	screen.free()

## 可选真实整态输入只作同节点native接口对照；安装失败立即报告，不降级到替身或伪造支持。
func native_checks() -> void:
	if native_catalog_path.is_empty() and native_state_path.is_empty(): return
	check(not native_catalog_path.is_empty() and not native_state_path.is_empty(),"native对照须同时给定目录与实际状态")
	if native_catalog_path.is_empty() or native_state_path.is_empty(): return
	var catalog_read: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(native_catalog_path)); var state_read: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(native_state_path))
	check(catalog_read.ok and state_read.ok and ClassDB.class_exists("HanmoResidentBridge"),"同节点真实目录、状态及resident扩展可用")
	if not catalog_read.ok or not state_read.ok or not ClassDB.class_exists("HanmoResidentBridge"): return
	var catalog: Dictionary=catalog_read.value; var state: Dictionary=state_read.value
	var expected: Dictionary=Page.query(state,catalog.data)
	check(expected.get("active",false),"native对照输入确实处于玩家会战")
	if not expected.get("active",false): return
	var controlled: int=0
	for unit in expected.units.values():
		if unit.has("condition"): controlled+=1
	check(controlled>0,"native对照输入含真实可控条件来源，不能用空页冒充")
	var bridge: Object=ClassDB.instantiate("HanmoResidentBridge")
	var installed: Dictionary=bridge.install(JSON.stringify(catalog,"",false,true).to_utf8_buffer(),JSON.stringify(state,"",false,true).to_utf8_buffer())
	check(installed.get("ok",false),"真实native冷装输入 "+str(installed))
	if not installed.get("ok",false): return
	var before: Dictionary=bridge.debug_export_state()
	var accepted: Dictionary=bridge.enqueue_live_operation({"kind":"query","source":installed.identity,"payload":{"query_id":"military_battle_page","args":{}}})
	check(accepted.get("ok",false),"真实worker接纳有限会战查询")
	if not accepted.get("ok",false): return
	var reply: Dictionary={}; var deadline: int=Time.get_ticks_msec()+30000
	while Time.get_ticks_msec()<deadline:
		reply=bridge.poll_live_operation(accepted.reference)
		if not reply.get("ok",false) or reply.get("ready",false): break
		await process_frame
	check(reply.get("ok",false) and reply.get("ready",false),"有限查询在真实worker完成")
	if reply.get("ok",false) and reply.get("ready",false):
		check(reply.source_identity==installed.identity and reply.current_identity==installed.identity,"读取保持同一世界/epoch/revision/tick")
		check(JSON.stringify(reply.result,"",false,true)==JSON.stringify(expected,"",false,true),"GD/native有限会战页类型、数值与字段序完全相同")
	check(bridge.debug_export_state()==before,"真实native查询不改整态/随机/ID")
	bridge=null

## 开发期仅解析本脚本；下一集成节点执行一个真实来源代表门，可通过参数追加同节点原生页对照。
func _run() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--native-catalog="): native_catalog_path=arg.trim_prefix("--native-catalog=")
		elif arg.begins_with("--native-state="): native_state_path=arg.trim_prefix("--native-state=")
	var f: Dictionary=condition_fixture()
	if not f.is_empty():
		var before: Dictionary=f.state.duplicate(true); var page: Dictionary=Page.query(f.state,f.data,f.battle_id)
		check(page.get("ok",false) and page.units[f.unit].has("condition"),"原有限战场页为当前可控部队提供条件")
		if page.get("ok",false) and page.units[f.unit].has("condition"):
			source_checks(f,page.units[f.unit].condition); privacy_checks(f); inspector_checks(page.units[f.unit].condition)
			page.units[f.unit].condition.history[0].fatigue_after=-100; page.units[f.unit].condition.initial_source.training=-100
			check(Page.query(f.state,f.data,f.battle_id).units[f.unit].condition.history[0].fatigue_after==f.state.military_units[f.unit].fatigue,"返回容器深独立，改页不改原来源")
		check(f.state==before,"查询与控件展开均不改全世界、战斗、随机、钱粮或来源")
	await native_checks()
	print("MILITARY_BATTLE_CONDITION_VIEW_CHECKS=",checks," FAILURES=",failures," NATIVE_INPUT=",not native_state_path.is_empty())
	quit(0 if failures==0 else 1)
