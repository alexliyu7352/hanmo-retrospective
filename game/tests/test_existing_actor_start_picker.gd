## 已有人物启程叶的真实作者/原PNG窄门；作者转索引夹具不冒充正式Catalog、Main或新世界准入。
extends SceneTree
const Picker=preload("res://ui/existing_actor_start_picker.gd")
const Registry=preload("res://ui/presentation_registry.gd")
const Snapshot=preload("res://core/scenario_snapshot.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: Array=[]
var confirmations: Array=[]
var cancellations: int=0

## 引擎就绪后操作实际控件，不申请桌面或鼠标权限。
func _init() -> void: call_deferred("run")

## 每项失败进入退出码，缺源不能被后续部分通过掩盖。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures.append(label); printerr("FAIL ",label)
	return value

## 记录确认副本，不替宿主补交世界命令。
func confirmed(value: Dictionary) -> void: confirmations.append(value)

## 记录取消次数，不写世界或银行。
func cancelled() -> void: cancellations+=1

## 按公开元数据寻找实际按钮；测试不直接调用私有选择函数。
func button(node: Node, action: String, actor_id: String="") -> Button:
	if node is Button and node.get_meta("existing_actor_action","")==action and (actor_id.is_empty() or node.get_meta("player_actor_id","")==actor_id): return node
	for child in node.get_children():
		var found: Button=button(child,action,actor_id)
		if found!=null: return found
	return null

## 只将真实作者数组转成字典索引，并登记中央匹配schema；保留原数值和快照。
func fixture(path: String, schema_path: String) -> Dictionary:
	var text: String=FileAccess.get_file_as_string(path)
	var source: Dictionary=Strict.parse_text(text)
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
	return {"ok":true,"data":data,"schema_sha256":schema_text.sha256_text(),"pack_set_digest":"leaf-fixture:"+text.sha256_text()}

## 完整pack清单不裁剪；实际图片、人物选择、取消和旧代次全部经生产叶运行。
func run() -> void:
	var args: PackedStringArray=OS.get_cmdline_user_args()
	if args.size()!=3: printerr("需要正文、中央schema、完整presentation三参数"); quit(2); return
	var catalog: Dictionary=fixture(args[0],args[1])
	if not check(catalog.get("ok",false),"real_author_and_central_schema"): quit(1); return
	var original: Dictionary=catalog.duplicate(true)
	var registry=Registry.new(); var ready: Dictionary=registry.load_from_path(args[2])
	if not check(ready.ok,"complete_pack_registry_original_pngs "+str(ready)): quit(1); return
	var scenario: String="scenario.qingchuan_spring_reserve"
	var picker: Control=Picker.new(); root.add_child(picker)
	picker.confirmed.connect(confirmed); picker.cancelled.connect(cancelled)
	var result: Dictionary=picker.configure(catalog,scenario,registry)
	if not check(result.ok,"actual_leaf_configure "+str(result)): picker.free(); quit(1); return
	check(result.count==2 and picker.actors().size()==2,"two_people")
	check(picker.selected().is_empty() and button(picker,"confirm").disabled,"no_default_selection")
	button(picker,"confirm").pressed.emit(); check(confirmations.is_empty(),"unselected_confirm_rejected")
	var rows: Dictionary={}
	for row in picker.actors(): rows[row.actor_id]=row
	check(rows["actor.apprentice"].display_name=="程禾" and rows["actor.study_swordswoman"].display_name=="沈蘅","actual_names")
	check(rows["actor.apprentice"].age_years==22 and rows["actor.study_swordswoman"].age_years==43,"actual_ages")
	check(rows["actor.apprentice"].personal_balance==240 and rows["actor.study_swordswoman"].personal_balance==8,"personal_balances_not_estate")
	for id in rows:
		check(rows[id].place_id=="place.courtyard" and rows[id].initial_goods==[{"item_id":"item.millet","quantity":2}],"snapshot_place_and_goods_"+id)
		var resolved: Dictionary=Snapshot.resolve(catalog,scenario,id)
		var actor: Dictionary=resolved.catalog.data.actors[id]
		check(rows[id].attributes==actor.attributes and rows[id].skills==actor.skills,"original_attributes_and_skills_"+id)
		check(rows[id].presentation_profile_id==actor.get("presentation_profile_id","") and rows[id].portrait_kind==actor.get("portrait_kind","") and rows[id].appearance.base_features==actor.appearance_definition.base_features,"original_avatar_binding_"+id)
		check(button(picker,"select",id).is_visible_in_tree(),"visible_person_button_"+id)
		var avatar: Control=null
		for child in picker.get_children():
			if child.get_meta("existing_actor_id","")==id: avatar=child.get_node("ExistingActorAvatar")
		check(avatar!=null and avatar.is_visible_in_tree() and avatar.image_copy()!=null and avatar.image_copy().get_size()==Vector2i(96,96) and avatar.describe().actor_id==id,"real_original_avatar_pixels_visible_"+id)
	check(rows["actor.apprentice"].appearance_policy==Picker.COMPOSITION and rows["actor.study_swordswoman"].appearance_policy==Picker.PRESERVE and rows["actor.study_swordswoman"].skills.martial==1200,"original_shen_skill_and_explicit_modes")
	button(picker,"select","actor.study_swordswoman").pressed.emit()
	var expected: Dictionary={"scenario_id":scenario,"pack_set_digest":catalog.pack_set_digest,"actor_id":"actor.study_swordswoman","appearance_policy":Picker.PRESERVE}
	check(picker.selected()==expected and not button(picker,"confirm").disabled,"exact_explicit_actor_binding")
	var copy: Dictionary=picker.selected(); copy.actor_id="outside"; check(picker.selected()==expected,"selection_copy_isolated")
	var old_select: Button=button(picker,"select","actor.apprentice")
	var old_confirm: Button=button(picker,"confirm"); var old_cancel: Button=button(picker,"cancel")
	check(picker.configure(catalog,scenario,registry).ok,"new_epoch")
	old_select.pressed.emit(); old_confirm.pressed.emit(); old_cancel.pressed.emit()
	check(picker.selected().is_empty() and confirmations.is_empty() and cancellations==0 and not picker._closed,"old_controls_inert")
	button(picker,"select","actor.study_swordswoman").pressed.emit(); button(picker,"confirm").pressed.emit(); button(picker,"confirm").pressed.emit(); button(picker,"cancel").pressed.emit()
	check(confirmations.size()==1 and confirmations[0]==expected and cancellations==0,"single_confirmation_without_appearance_request")
	confirmations[0].actor_id="outside"; check(picker.selected()==expected,"confirmation_copy_isolated")
	check(picker.configure(catalog,scenario,registry).ok,"reopen")
	button(picker,"select","actor.apprentice").pressed.emit(); button(picker,"cancel").pressed.emit(); picker.confirm_selection(); picker.close()
	check(picker.selected().is_empty() and confirmations.size()==1 and cancellations==1,"cancel_discards_and_blocks_late_confirm")
	var invalid: Dictionary=catalog.duplicate(true); invalid.data.scenarios[scenario].playable_actor_appearance_policies[1].actor_id="outside"
	check(not picker.configure(invalid,scenario,registry).ok and picker.get_child_count()==0 and picker.selected().is_empty(),"bad_policy_coverage_atomic_reject")
	invalid=catalog.duplicate(true); invalid.data.scenarios[scenario].playable_actor_appearance_policies[1].policy="outside"
	check(not picker.configure(invalid,scenario,registry).ok,"unknown_policy_reject")
	check(picker.configure(catalog,scenario,registry).ok,"reopen_after_reject")
	button(picker,"select","actor.apprentice").pressed.emit(); root.remove_child(picker); picker.confirm_selection()
	check(confirmations.size()==1,"detached_confirm_reject")
	check(catalog==original,"author_and_snapshot_never_modified")
	picker.free(); await process_frame
	print("EXISTING_ACTOR_PICKER_CHECKS=",checks," FAILURES=",failures.size()," MAIN_WORLD=NOT_RUN BANK=NOT_RUN GUI=NOT_RUN")
	quit(0 if failures.is_empty() else 1)
