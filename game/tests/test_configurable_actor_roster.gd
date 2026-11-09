## 可配置名单的真实快照与控件窄门；无图时只显示同人文本，不建立世界或改写银行。
extends SceneTree
const Picker=preload("res://ui/existing_actor_start_picker.gd")
const Snapshot=preload("res://core/scenario_snapshot.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: Array=[]
var selections: Array=[]
var cancellations: int=0

## 空美术目录用于真实缺图场景；不伪造头像、人物或规则成功结果。
class EmptyArt extends RefCounted:
	var nonempty: bool=false
	func _init(has_empty_groups: bool=false) -> void: nonempty=has_empty_groups
	func describe() -> Dictionary: return {"character_profiles":{},"portraits":{}} if nonempty else {}
	func texture(_id: String) -> Texture2D: return null

## 无窗口启动后的实际控件连接和返回流程仍由Godot执行。
func _init() -> void: call_deferred("run")

## 每个失败进入最终退出码，不把部分通过冒充UI验收。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures.append(label); printerr("FAIL ",label)
	return value

## 确认信号只记录冻结ID，不调用世界初始化或补默认选择。
func selected(value: Dictionary) -> void: selections.append(value)

## 取消计数只检查本地交互，不触碰任何人物状态。
func cancelled() -> void: cancellations+=1

## 遍历实际控件的公开动作标记，避免测试直接执行私有选人函数。
func button(node: Node, action: String, actor: String="") -> Button:
	if node is Button and node.get_meta("existing_actor_action","")==action and (actor.is_empty() or node.get_meta("player_actor_id","")==actor): return node
	for child in node.get_children():
		var found: Button=button(child,action,actor)
		if found!=null: return found
	return null

## 仅将真实作者源转为正常快照所需索引；本门不声称全目录校验或原生初始化通过。
func fixture(content_path: String, schema_path: String) -> Dictionary:
	var raw: String=FileAccess.get_file_as_string(content_path)
	var source: Dictionary=Strict.parse_text(raw)
	var schema_text: String=FileAccess.get_file_as_string(schema_path)
	var schema: Dictionary=Strict.parse_text(schema_text)
	if not source.get("ok",false) or not schema.get("ok",false): return {"ok":false}
	var registered: Dictionary=Snapshot.register_schema(schema_text)
	if not registered.ok: return registered
	var data: Dictionary={}
	for table in schema.value["x-hanmo-tables"]:
		data[table]={}
		for row in source.value.get(table,[]): data[table][row.id]=row.duplicate(true)
	for key in ["texts","flags","required_capabilities"]: data[key]=source.value[key].duplicate(true)
	return {"ok":true,"data":data,"schema_sha256":schema_text.sha256_text(),"pack_set_digest":"roster-leaf:"+raw.sha256_text()}

## 三个原人物、老年、缺图、取消重开及过期回调必须保持同一身份和输入。
func run() -> void:
	var args: PackedStringArray=OS.get_cmdline_user_args()
	if args.size()!=2: printerr("需要作者正文与中央schema路径"); quit(2); return
	var catalog: Dictionary=fixture(args[0],args[1])
	if not check(catalog.get("ok",false),"real_author_snapshot_fixture"): quit(1); return
	var scenario_id: String="scenario.qingchuan_spring_reserve"
	var scenario: Dictionary=catalog.data.scenarios[scenario_id]
	var ids: Array=[]
	for row in scenario.initial.actors:
		if row.alive and row.available and row.age_years>=18 and row.age_years<=120:
			ids.append(row.actor_id)
			if ids.size()==3: break
	if not check(ids.size()==3,"three_existing_adults"): quit(1); return
	scenario.dynamic_world.founder_policy=Picker.ROSTER_POLICY; scenario.player_actor=ids[0]
	scenario.playable_actor_ids=ids; scenario.playable_actor_appearance_policies=[]
	scenario.erase("player_start_descriptions"); scenario.erase("player_appearance_choices")
	for id in ids: scenario.playable_actor_appearance_policies.append({"actor_id":id,"policy":Picker.PRESERVE})
	for row in scenario.initial.actors:
		if row.actor_id==ids[-1]: row.age_years=120
	var original: Dictionary=catalog.duplicate(true)
	var picker: Control=Picker.new(); root.add_child(picker); picker.confirmed.connect(selected); picker.cancelled.connect(cancelled)
	var art=EmptyArt.new()
	var result: Dictionary=picker.configure(catalog,scenario_id,art)
	if not check(result.get("ok",false),"real_three_person_controls "+str(result)): picker.free(); quit(1); return
	check(result.count==3 and picker.actors().size()==3,"all_authored_people_listed")
	check(picker.selected().is_empty() and button(picker,"confirm").disabled,"no_implicit_choice")
	for child in picker.get_children():
		var id: String=child.get_meta("existing_actor_id","")
		if not id.is_empty(): check(child.get_node("ExistingActorAvatar").get_meta("identity_text_actor_id","")==id,"text_portrait_keeps_"+id)
	button(picker,"select",ids[-1]).pressed.emit()
	check(picker.selected().actor_id==ids[-1],"old_age_actor_is_selected_unchanged")
	var old_select: Button=button(picker,"select",ids[0]); var old_confirm: Button=button(picker,"confirm")
	button(picker,"cancel").pressed.emit(); check(cancellations==1 and selections.is_empty() and picker.selected().is_empty(),"cancel_drops_only_local_choice")
	result=picker.configure(catalog,scenario_id,art); check(result.get("ok",false),"reopen_same_roster")
	old_select.pressed.emit(); old_confirm.pressed.emit(); check(picker.selected().is_empty() and selections.is_empty(),"old_controls_cannot_choose_new_roster")
	button(picker,"select",ids[1]).pressed.emit(); button(picker,"confirm").pressed.emit(); button(picker,"confirm").pressed.emit()
	check(selections.size()==1 and selections[0].actor_id==ids[1],"confirm_once_original_second_actor")
	check(catalog==original,"all_catalog_inputs_unchanged")
	var missing: Dictionary=catalog.duplicate(true)
	missing.data.actors[ids[-1]].presentation_profile_id="missing_profile"; missing.data.actors[ids[-1]].portrait_kind="missing_portrait"
	result=picker.configure(missing,scenario_id,EmptyArt.new(true)); check(result.get("ok",false),"nonempty_manifest_missing_this_person")
	for child in picker.get_children():
		if child.get_meta("existing_actor_id","")==ids[-1]: check(child.get_node("ExistingActorAvatar").get_meta("identity_text_actor_id","")==ids[-1],"empty_successful_descriptor_uses_real_name")
	var legacy: Dictionary=catalog.duplicate(true); legacy.data.scenarios[scenario_id].dynamic_world.founder_policy=Picker.POLICY
	check(not picker.configure(legacy,scenario_id,art).get("ok",false),"legacy_policy_keeps_two_person_limit")
	picker.free(); print("CONFIGURABLE_ACTOR_ROSTER ",checks," checks ",failures.size()," failures"); quit(1 if not failures.is_empty() else 0)
