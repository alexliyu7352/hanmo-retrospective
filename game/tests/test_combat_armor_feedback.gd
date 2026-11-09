## 本人冻结护具的最小可见输入与真实控件反馈；复用原预览桥，不验证存读或全游戏。
extends "res://tests/test_kernel_personal_battle_view.gd"
const ArmorFixture=preload("res://tests/test_personal_armor_inventory.gd")
const EntryFixture=preload("res://tests/test_personal_condition_snapshot.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const BattleView=preload("res://core/views/personal_battle_view.gd")

## 原Main标签的换行策略用于布局门；不启动Main生命周期、原生存读或另造世界入口。
class LayoutHost extends Host:
	## 文本创建与Main相同的自动折行，避免无换行测试替身把实际宽度问题掩盖。
	func _text(parent: Node,words: String,font_size: int) -> Label:
		var label: Label=super._text(parent,words,font_size); label.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART; return label

## 从实际背包、选择与原开战入口冻结护具，不直接为有效甲夹具填写战场防御。
func armored_host(kind: String="equipped",defense: int=6) -> Host:
	var data: Dictionary=ArmorFixture.content(defense); data.actors={"a":{"id":"a"},"b":{"id":"b"}}; data.texts={"name.armor":"布甲"}
	var state: Dictionary=ArmorFixture.source(); state.player_actor_id="a"; state.revision=5
	if kind=="none": state.actors.a.combat.erase("armor_id")
	if kind=="lost": check(Inventory.transfer(state,"bag.a","bag.b","armor",1,[{"kind":"actor","id":"a"}],"",{"kind":"actor","id":"b"}).ok,"真实转出使旧选择入战前失效")
	var entered: Dictionary=Personal.preview(state,data,"a","duel_start",EntryFixture.params())
	check(entered.ok and Personal.begin(state,data,entered).ok,"原个人预览与开战冻结实际护具 "+kind)
	var bridge=Bridge.new(); bridge.current=state; bridge.definitions=data
	var host=Host.new(); host.kernel=bridge; host._data=data; allocated.append(host); return host

## 生产装配与测试手动控件都读取同一装备文本，展开详情只影响页面可见性。
func armor_display_checks(host: Host,expected: Dictionary) -> void:
	var value: Dictionary=BattleView.query(host.kernel.current,host._data)
	if not check(value.ok and value.get("equipment_feedback")==expected,"有限页面仅有本人的精确冻结装备字段"): return
	var view=view_for(host,value)
	if not check(view.has_method("_armor_summary") and view.has_method("_equipment_details"),"护具反馈走实际页面文本出口"): return
	check(view._armor_summary("a")=="护具防御 %d"%expected.armor_defense,"本人防御原值可读")
	check(view._armor_summary("b")=="护具未知","对手无精确护具事实时保持未知")
	check(view._target.text.contains("护具未知"),"真实目标标签不显示敌方甲值")
	var details: String=view._equipment_details()
	check(details.contains("入战时固定") and details.contains("最终伤害"),"说明冻结来源与最终伤害边界")
	check(details.contains("本场无有效护具") if expected.armor_id.is_empty() else details.contains("布甲"),"有甲与本场未生效均有实际文本")
	var before: Dictionary=host.kernel.current.duplicate(true)
	var params: Dictionary=view.draft.duplicate(true); params.battle_id=view.battle.id
	var quote: Dictionary=Personal.preview(before,host._data,"a","battle_plan",params)
	check(quote.ok and view._tactical_preview==quote.tactical_preview and host.kernel.current==before,"护具显示不重算或替换原攻击报价")
	view._submit_plan(); view._submit_plan()
	check(host.commands.size()==1 and host.commands[0].params==params,"正常本轮攻击参数与防重入保持")

## 未知他人护具不读取；本人坏成对快照拒绝，不把坏值默认成无甲。
func armor_visibility_checks() -> void:
	var host=armored_host(); var state: Dictionary=host.kernel.current; var id: String=BattleView.active(state)
	var before: Dictionary=state.duplicate(true); var value: Dictionary=BattleView.query(state,host._data)
	if not check(value.has("equipment_feedback"),"护具投影存在"): return
	value.equipment_feedback.armor_id="forged"; value.battle.units.a.position[0]+=20
	check(state==before,"装备反馈与嵌套战场均不能回写权威源")
	var screen=view_for(host); screen.page_input.equipment_feedback.armor_defense=99
	check(state==before and BattleView.query(state,host._data).equipment_feedback.armor_defense==6,"配置页面再深复制护具来源")
	state.battles[id].units.b.armor_id={"private":"enemy.secret"}; state.battles[id].units.b.armor_defense=["secret"]
	state.battles[id].ai_plans={"secret_plan":{"armor_id":"secret"}}; state.inventories=false; state.actors.a.combat=false
	value=BattleView.query(state,host._data)
	check(value.ok and value.equipment_feedback=={"actor_id":"a","armor_id":"armor","armor_defense":6},"只读本人战场快照，不读敌甲、AI或当前背包选择")
	check(not value.battle.units.b.has("armor_id") and not value.battle.units.b.has("armor_defense") and not JSON.stringify(value).contains("secret"),"敌甲和未来计划没有借表现字段泄漏")
	state=before.duplicate(true); host.kernel.current=state
	state.player_actor_id="b"
	value=BattleView.query(state,host._data)
	check(value.ok and value.equipment_feedback=={"actor_id":"b","armor_id":"","armor_defense":0},"旁观只公开本人，AI控制者不因权限字段扩大装备可见性")
	check(not value.battle.units.a.has("armor_id") and not value.battle.units.a.has("armor_defense"),"控制者同样不获得通用护具字段")
	for patch in [{"armor_defense":false},{"armor_defense":6.0},{"armor_defense":101},{"armor_defense":99},{"armor_id":"missing"},{"armor_id":""}]:
		state=before.duplicate(true); state.battles[id].units.a.merge(patch,true)
		check(BattleView.query(state,host._data).get("code")=="InvalidPersonalBattleViewSource","本人坏快照沿原结构化失败 "+JSON.stringify(patch))
	for field in ["armor_id","armor_defense"]:
		state=before.duplicate(true); state.battles[id].units.a.erase(field)
		check(BattleView.query(state,host._data).get("code")=="InvalidPersonalBattleViewSource","本人单字段缺失不能伪装无甲 "+field)

## 终局来源仍是当场甲；原伤害回执和后来背包失效不会互相覆盖。
func armor_history_checks() -> void:
	var host=armored_host(); var state: Dictionary=host.kernel.current; var id: String=BattleView.active(state)
	host._data.personal_objectives.spar.max_rounds=1
	var before: Dictionary=state.battles[id].duplicate(true)
	var params: Dictionary={"battle_id":id,"destination":before.units.a.position.duplicate(),"heading":0,"action":"wait"}
	var quote: Dictionary=Personal.preview(state,host._data,"a","battle_plan",params)
	check(quote.ok and Personal.apply(state,host._data,"a","battle_plan",quote).ok,"真实本轮产出终局与伤害回执")
	check(Inventory.transfer(state,"bag.a","bag.b","armor",1,[{"kind":"actor","id":"a"}],"",{"kind":"actor","id":"b"}).ok,"终局后实物由原库存接口转出")
	var value: Dictionary=BattleView.query(state,host._data)
	check(value.ok and value.get("equipment_feedback")=={"actor_id":"a","armor_id":"armor","armor_defense":6},"历史反馈保留原冻结甲而不采后来背包")
	var expected: Array=[]
	for event in state.battles[id].last_events:
		if event.has("target_id") and event.has("damage"): expected.append({"target_id":event.target_id,"damage":event.damage})
	check(value.battle.damage_feedback==expected,"伤害浮字保持原始最终回执，不编造护具阻挡量")

## 真实_build的新增文本与折叠控件可见；展开只改变详情，原命令位置、草案和查询次数不变。
func armor_assembled_checks() -> void:
	for extent in [Vector2i(1280,800),Vector2i(1280,720),Vector2i(1024,720)]:
		var original=armored_host(); var host=LayoutHost.new(); allocated.append(host)
		host.kernel=original.kernel; host._data=original._data; host.size=extent; root.add_child(host)
		var view=Screen.render_view(host,host,BattleView.query(host.kernel.current,host._data))
		for step in range(3): await process_frame
		var condition: Label=view.find_child("PersonalBattleCondition",true,false)
		var toggle: Button=view.find_child("PersonalBattleEquipmentToggle",true,false)
		var details: Label=view.find_child("PersonalBattleEquipmentDetails",true,false)
		check(condition!=null and condition.text.contains("甲防6"),"整页本人状态行实际显示冻结防御 "+str(extent))
		check(toggle!=null and details!=null and not details.visible,"护具详情默认收起且有点按入口 "+str(extent))
		check(condition.size.x<=extent.x*0.21+1 and view._target.size.x<=extent.x*0.21+1,"本人及目标新文字服从原侧栏宽度 "+str(extent))
		var position: Vector2=view._submit.position; var draft_before: Dictionary=view.draft.duplicate(true)
		var state_before: Dictionary=host.kernel.current.duplicate(true); var queries: Array=host.kernel.events.duplicate(true)
		toggle.set_pressed(true)
		for step in range(3): await process_frame
		check(details.visible and details.text==view._equipment_details(),"实际展开内容来自同一冻结来源 "+str(extent))
		check(view._submit.position==position and view.draft==draft_before and host.kernel.current==state_before and host.kernel.events==queries and host.commands.is_empty(),"展开不移动原确认、不改草案或再查世界 "+str(extent))
		toggle.set_pressed(false)
		for step in range(2): await process_frame
		check(not details.visible and view._submit.position==position,"重复开合保持原确认位置 "+str(extent))
		host.remove_child(view); view.free(); root.remove_child(host)

## 一次局部门覆盖本人装备显示、敌我边界、历史与原页面失效守卫。
func run() -> void:
	for kind in ["none","equipped","lost"]:
		armor_display_checks(armored_host(kind),{"actor_id":"a","armor_id":"armor" if kind=="equipped" else "","armor_defense":6 if kind=="equipped" else 0})
	armor_display_checks(armored_host("equipped",0),{"actor_id":"a","armor_id":"armor","armor_defense":0})
	armor_display_checks(armored_host("equipped",100),{"actor_id":"a","armor_id":"armor","armor_defense":100})
	armor_visibility_checks(); armor_history_checks()
	await armor_assembled_checks()
	for kind in ["stale_epoch","stale_generation","stale_revision","stale_actor","stale_round","stale_phase","stale_controller","stale_kernel"]:
		var result: Dictionary=await observe(kind,true)
		check(result.stale and result.commands.is_empty(),"新增反馈不绕过原旧页守卫 "+kind)
	for object in allocated:
		if is_instance_valid(object): object.free()
	print("COMBAT_ARMOR_FEEDBACK checks=",checks," failures=",failures); quit(0 if failures==0 else 1)
