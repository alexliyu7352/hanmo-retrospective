## 正式组合控件的窄测试；真实来源与按钮验证草案、双图失败和代次，不代替GUI或世界存读验收。
extends SceneTree
const Picker=preload("res://ui/regional_appearance_picker.gd")
const Accepted=preload("res://ui/character_appearance_adapter.gd")
const Composition=preload("res://ui/adult_appearance_composition.gd")
const Registry=preload("res://ui/presentation_registry.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0
var confirmations: Array=[]
var cancellations: int=0

## 无PNG协议门只替换绘制出口，不产生图像；真实控件和完整作者校验仍执行，结果不算像素验收。
class ProtocolPicker extends Picker:
	var render_calls: int=0
	var reject_render: bool=false
	## 记录候选准备请求并可注入失败；不访问世界、RNG、资源或任何存储。
	func _render(_selection: Dictionary) -> Dictionary:
		render_calls+=1
		return {"ok":false,"code":"ProtocolRenderRejected"} if reject_render else {"ok":true}

## 只在清单公开副本中撤销组合登记；生产解析与真实PNG/配对合成仍全部执行。
class FaultRegistry extends Registry:
	var missing_composition: bool=false
	var texture_reads: int=0
	var fail_texture_at: int=-1
	## 真实纹理读取计数仅用于最后一次头像读取故障；其余图像保持正式PNG原字节。
	func texture(group: String, id: String) -> Texture2D:
		texture_reads+=1
		if texture_reads==fail_texture_at: return null
		return super.texture(group,id)
	## 描述失败注入不修改已验证生产清单，也不触及磁盘资产。
	func describe() -> Dictionary:
		var result: Dictionary=super.describe()
		if missing_composition: result.erase("adult_appearance_composition")
		return result

## 引擎就绪后再运行实际控件信号，不申请图形桌面或鼠标权限。
func _init() -> void:
	call_deferred("_run")

## 任一断言失败均影响退出码，并保留失败边界标签。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 测试只保留正式成人配方和真实源PNG；组合登记仍取唯一清单，不另造资源白名单。
func manifest() -> Dictionary:
	var data: Dictionary=Strict.parse_text(FileAccess.get_file_as_string("res://data/presentation.json")).value
	for id in data.portraits.keys():
		if not id.begins_with("accepted_adult_"): data.portraits.erase(id)
	for id in data.character_profiles.keys():
		if not id.begins_with("accepted_adult.") and id!=Accepted.PROFILE_ID: data.character_profiles.erase(id)
	data.components={}; data.scene_templates={}
	data.default_portrait=data.portraits.keys()[0]
	data.backgrounds={"source_fixture":data.portraits[data.default_portrait].duplicate(true)}
	data.default_background="source_fixture"
	return data

## 作者表剥离标签与绑定，进入picker的只有完整语义元组及作者项ID。
func rows_for(records: Array) -> Array:
	var rows: Array=[]
	for record in records: rows.append({"id":record.id,"base_features":record.base_features.duplicate(true),"attire_style":record.attire_style})
	return rows

## 使用真实下拉信号选择token，和用户选择一样同步控件显示项。
func choose(picker: Control, slot: String, token: String) -> void:
	var control: OptionButton=picker._slot_controls[slot]
	for index in range(control.item_count):
		if control.get_item_metadata(index)==token:
			control.select(index); control.item_selected.emit(index); return
	check(false,"下拉未提供token "+slot+"/"+token)

## 记录确认次数及信号副本，供一次性应用断言使用。
func on_confirmed(value: Dictionary) -> void:
	confirmations.append(value)

## 记录取消次数，不代替宿主创建或修改世界。
func on_cancelled() -> void:
	cancellations+=1

## 双图、草案与所有输入都必须在失败后保留最近一次成功值。
func _run() -> void:
	if "--protocol-only" in OS.get_cmdline_user_args():
		await _run_protocol()
		return
	if "--legacy-real-only" in OS.get_cmdline_user_args():
		await _run_legacy_real()
		return
	var records: Array=Composition.records()
	check(records.size()==192,"正式组合目录192项完整")
	if records.size()!=192: quit(1); return
	var rows: Array=rows_for(records)
	var presets: Array=rows_for(Accepted.records())
	var actor: Dictionary={"id":"actor.apprentice","display_name":"学徒","origin":"fictional","presentation_profile_id":Accepted.PROFILE_ID,"appearance_definition":{"kind":"fixed","generator_version":"appearance-v1","base_features":presets[0].base_features.duplicate(true),"attire_style":presets[0].attire_style}}
	var untouched_actor: String=JSON.stringify(actor)
	var untouched_rows: String=JSON.stringify(rows)
	var registry=FaultRegistry.new()
	var registered: Dictionary=registry.configure(manifest())
	check(registered.ok,"正式来源通过生产注册表 "+str(registered))
	if not registered.ok: quit(1); return
	var picker: Control=Picker.new(); root.add_child(picker)
	picker.confirmed.connect(on_confirmed); picker.cancelled.connect(on_cancelled)
	check(not picker.configure(actor.id,actor,24,registry,rows).ok,"省略policy仍为旧十二代表，不开放192组合")
	check(not picker.configure(actor.id,actor,24,registry,presets,Composition.POLICY).ok,"新policy不接受仅12项的部分覆盖")
	# 首个非preset候选必须先完成真实合成；缺工坊源时只报告该依赖失败，不继续伪称后续控件断言有效。
	var probe_actor: Dictionary=actor.duplicate(true)
	for row in rows:
		var is_preset: bool=false
		for preset in presets:
			if row.base_features==preset.base_features and row.attire_style==preset.attire_style: is_preset=true; break
		if not is_preset:
			probe_actor.appearance_definition.base_features=row.base_features.duplicate(true)
			break
	var probe: Dictionary=picker.configure(actor.id,probe_actor,24,registry,rows,Composition.POLICY)
	check(probe.ok,"非preset真实组合来源就绪 "+str(probe))
	if not probe.ok: picker.free(); quit(1); return
	var configured: Dictionary=picker.configure(actor.id,actor,24,registry,rows,Composition.POLICY)
	check(configured.ok,"真实组合入口完成默认双图 "+str(configured))
	if not configured.ok: picker.free(); quit(1); return
	check(picker._face_buttons.size()==12 and picker._slot_controls.size()==4,"十二骨相与四项独立控件")
	for slot in picker._slot_controls: check(picker._slot_controls[slot].item_count==2,"每槽两项 "+slot)
	var initial: Dictionary=picker.draft()
	var stale_face: Button=picker._face_buttons[presets[1].base_features.face_shape]
	var stale_option: OptionButton=picker._slot_controls.eyes
	var stale_confirm: Button=picker._confirm
	var stale_epoch: int=picker._epoch
	check(picker.configure(actor.id,actor,24,registry,rows,Composition.POLICY).ok,"原对象重配进入新代次")
	stale_face.pressed.emit(); stale_option.item_selected.emit(1); stale_confirm.pressed.emit()
	picker.cancel_selection(stale_epoch)
	check(picker.draft()==initial and confirmations.is_empty() and cancellations==0 and not picker._closed,"旧骨相/五官/确认/取消回调全部无效")
	for preset in presets:
		var before: Dictionary=picker.draft()
		picker._face_buttons[preset.base_features.face_shape].pressed.emit()
		var expected: Dictionary=before.duplicate(true); expected.base_features.face_shape=preset.base_features.face_shape
		check(picker.draft()==expected,"骨相切换只改变骨相 "+preset.id)
		check(picker._half.describe().identity_key==picker._avatar.describe().identity_key,"骨相切换双图同一身份 "+preset.id)
	picker._face_buttons[presets[0].base_features.face_shape].pressed.emit()
	for slot in picker._slot_controls:
		var control: OptionButton=picker._slot_controls[slot]
		var before: Dictionary=picker.draft()
		var token: String=str(control.get_item_metadata(1-control.selected))
		choose(picker,slot,token)
		var expected: Dictionary=before.duplicate(true); expected.base_features[slot]=token
		check(picker.draft()==expected,"独立五官仅改变本槽 "+slot)
	choose(picker,"eyes","narrow")
	var previous: Dictionary=picker.draft()
	var half: Dictionary=picker._half.describe(); var avatar: Dictionary=picker._avatar.describe()
	var half_pixels: PackedByteArray=picker._half.image_copy().get_data()
	var avatar_pixels: PackedByteArray=picker._avatar.image_copy().get_data()
	registry.missing_composition=true
	var target: String="neutral" if previous.base_features.mouth=="smile" else "smile"
	choose(picker,"mouth",target)
	check(picker.draft()==previous,"组合资源登记缺失保持旧草案")
	check(picker._half.describe()==half and picker._avatar.describe()==avatar and picker._half.image_copy().get_data()==half_pixels and picker._avatar.image_copy().get_data()==avatar_pixels,"新组合失败保持原整对描述与像素")
	var mouth: OptionButton=picker._slot_controls.mouth
	check(mouth.get_item_metadata(mouth.selected)==previous.base_features.mouth,"失败下拉恢复最后成功项")
	picker.confirm_selection()
	check(confirmations.is_empty() and not picker._closed,"确认资源失效拒绝提交且允许恢复")
	registry.missing_composition=false
	picker.confirm_selection(); picker.confirm_selection(); picker.cancel_selection()
	check(confirmations.size()==1 and confirmations[0]==previous and cancellations==0,"资源恢复后仅一次确认完整九槽与衣着")
	check(JSON.stringify(actor)==untouched_actor and JSON.stringify(rows)==untouched_rows,"预览与确认不修改作者输入")
	var invalid: Array=rows.duplicate(true); invalid[191].base_features.hair="loose"
	check(not picker.configure(actor.id,actor,24,registry,invalid,Composition.POLICY).ok and picker.draft().is_empty() and picker.get_child_count()==0,"末项错误整表拒绝并清理旧配置")
	for age in [17,60]: check(not picker.configure(actor.id,actor,age,registry,rows,Composition.POLICY).ok,"组合年龄越界拒绝 "+str(age))
	check(picker.configure(actor.id,actor,59,registry,rows,Composition.POLICY).ok,"成人上界可重开")
	picker.cancel_selection(); picker.confirm_selection()
	check(cancellations==1 and confirmations.size()==1 and picker.draft().is_empty(),"取消丢弃完整草案且晚确认无效")
	picker.free()
	await process_frame
	print("REGIONAL_COMPOSITION_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)

## 按公开元数据递归查找真实按钮，保存的旧节点用于重配后的迟到信号。
func find_button(node: Node, key: String, value: String) -> Button:
	if node is Button and node.get_meta(key, "")==value: return node
	for child in node.get_children():
		var found: Button=find_button(child,key,value)
		if found!=null: return found
	return null

## PNG尚未恢复时独立跑作者表和实际控件；不替代默认真实图像门或宿主世界零写验证。
func _run_protocol() -> void:
	var records: Array=Composition.records()
	var presets: Array=rows_for(Accepted.records())
	check(records.size()==192 and presets.size()==12,"协议源目录192组合与12代表完整")
	if records.size()!=192 or presets.size()!=12: quit(1); return
	var rows: Array=rows_for(records)
	var actor: Dictionary={"id":"actor.apprentice","display_name":"学徒","origin":"fictional","presentation_profile_id":Accepted.PROFILE_ID,"appearance_definition":{"kind":"fixed","generator_version":"appearance-v1","base_features":presets[0].base_features.duplicate(true),"attire_style":presets[0].attire_style}}
	var original_actor: Dictionary=actor.duplicate(true)
	var original_rows: Array=rows.duplicate(true)
	var picker: Control=ProtocolPicker.new(); root.add_child(picker)
	picker.confirmed.connect(on_confirmed); picker.cancelled.connect(on_cancelled)
	check(picker.configure(actor.id,actor,24,null,rows,Composition.POLICY).ok,"新政策生成实际组合控件")
	check(picker._face_buttons.size()==12 and picker._slot_controls.size()==4,"12骨相及4个实际下拉")
	var reached: Dictionary={}
	for record in records:
		picker._face_buttons[record.base_features.face_shape].pressed.emit()
		for slot in Composition.FEATURES: choose(picker,slot,record.base_features[slot])
		var expected: Dictionary={"base_features":record.base_features,"attire_style":record.attire_style}
		check(picker.draft()==expected,"实际信号可达完整元组 "+record.id)
		if picker.draft()==expected: reached[record.id]=true
	check(reached.size()==192,"实际骨相与四下拉完整可达192候选")
	check(confirmations.is_empty() and cancellations==0,"192次预览没有确认或取消信号")
	var before: Dictionary=picker.draft()
	var copied: Dictionary=picker.draft(); copied.base_features.hair="outside"
	check(picker.draft()==before,"草案副本不能反写控件")
	var stale_face: Button=picker._face_buttons[presets[1].base_features.face_shape]
	var stale_option: OptionButton=picker._slot_controls.eyes
	var stale_confirm: Button=find_button(picker,"appearance_action","confirm")
	var stale_cancel: Button=find_button(picker,"appearance_action","cancel")
	var stale_epoch: int=picker._epoch
	check(picker.configure(actor.id,actor,24,null,rows,Composition.POLICY).ok,"同对象重配生成新代次")
	before=picker.draft()
	var calls: int=picker.render_calls
	stale_face.pressed.emit(); stale_option.item_selected.emit(1); stale_confirm.pressed.emit(); stale_cancel.pressed.emit()
	picker.cancel_selection(stale_epoch)
	check(picker.draft()==before and picker.render_calls==calls and confirmations.is_empty() and cancellations==0 and not picker._closed,"旧骨相/下拉/确认/取消在渲染前失效")
	var eye: OptionButton=picker._slot_controls.eyes
	eye.item_selected.emit(-1); eye.item_selected.emit(eye.item_count)
	picker._select_feature("hair","loose",picker._epoch)
	picker._select_feature("face_shape","outside",picker._epoch)
	check(picker.draft()==before and picker.render_calls==calls,"无效索引和未授权槽在渲染前拒绝")
	picker.reject_render=true
	var half: Control=picker._half; var avatar: Control=picker._avatar
	choose(picker,"eyes",str(eye.get_item_metadata(1-eye.selected)))
	check(picker.draft()==before and picker._half==half and picker._avatar==avatar,"准备失败保留草案和两控件引用，未声称像素验证")
	check(eye.get_item_metadata(eye.selected)==before.base_features.eyes,"准备失败恢复下拉选中项")
	find_button(picker,"appearance_action","confirm").pressed.emit()
	check(confirmations.is_empty() and not picker._closed,"确认准备失败不提交且窗口可恢复")
	picker.reject_render=false
	find_button(picker,"appearance_action","confirm").pressed.emit()
	find_button(picker,"appearance_action","confirm").pressed.emit()
	find_button(picker,"appearance_action","cancel").pressed.emit()
	check(confirmations.size()==1 and confirmations[0]==before and cancellations==0,"实际按钮最终只确认一次完整元组")
	confirmations[0].base_features.hair="outside"
	check(picker.draft()==before,"信号副本不能回写已确认草案")
	check(picker.configure(actor.id,actor,24,null,presets).ok,"旧默认政策仍接受原12代表")
	check(not picker._composition and picker._slot_controls.is_empty() and picker._face_buttons.is_empty(),"旧12路径没有混搭控件")
	for preset in presets:
		find_button(picker,"appearance_choice_id",preset.id).pressed.emit()
		check(picker.draft()=={"base_features":preset.base_features,"attire_style":preset.attire_style},"旧完整代表按钮保留原元组 "+preset.id)
	find_button(picker,"appearance_action","cancel").pressed.emit()
	picker.confirm_selection(); picker.cancel_selection()
	check(cancellations==1 and confirmations.size()==1 and picker.draft().is_empty(),"实际取消清空草案且重复终结无效")
	for candidate in [{"policy":"fixed_actor","rows":presets},{"policy":"fixed_actor_appearance_v1","rows":rows},{"policy":Composition.POLICY,"rows":presets}]:
		calls=picker.render_calls
		check(not picker.configure(actor.id,actor,24,null,candidate.rows,candidate.policy).ok and picker.get_child_count()==0 and picker.render_calls==calls,"跨政策候选在渲染前原子拒绝 "+candidate.policy)
	var invalid: Array=rows.duplicate(true); invalid[-1].base_features.hair="loose"
	check(not picker.configure(actor.id,actor,24,null,invalid,Composition.POLICY).ok and picker.draft().is_empty(),"末项坏元组无部分配置")
	for age in [17,60]: check(not picker.configure(actor.id,actor,age,null,rows,Composition.POLICY).ok,"成人域外拒绝 "+str(age))
	check(picker.configure(actor.id,actor,59,null,rows,Composition.POLICY).ok,"成人上界可重新配置")
	root.remove_child(picker); picker.confirm_selection()
	check(confirmations.size()==1,"离树后不能确认")
	check(actor==original_actor and rows==original_rows,"所有选择、失败、确认取消不修改作者输入")
	picker.free()
	await process_frame
	print("REGIONAL_COMPOSITION_PROTOCOL_CHECKS=",checks," FAILURES=",failures," REACHED=",reached.size()," PIXELS=NOT_RUN WORLD=NOT_RUN")
	quit(0 if failures==0 else 1)

## 已恢复的旧十二原PNG独立验证双图准备失败与原政策；无需工坊新层，不合成替代测试图片。
func _run_legacy_real() -> void:
	var presets: Array=rows_for(Accepted.records())
	check(presets.size()==12,"真实原图门包含12个旧代表")
	if presets.size()!=12: quit(1); return
	var registry=FaultRegistry.new()
	var registered: Dictionary=registry.configure(manifest())
	check(registered.ok,"旧原PNG由正式注册表完整加载 "+str(registered))
	if not registered.ok: quit(1); return
	var actor: Dictionary={"id":"actor.apprentice","display_name":"学徒","origin":"fictional","presentation_profile_id":Accepted.PROFILE_ID,"appearance_definition":{"kind":"fixed","generator_version":"appearance-v1","base_features":presets[0].base_features.duplicate(true),"attire_style":presets[0].attire_style}}
	var picker: Control=Picker.new(); root.add_child(picker)
	picker.confirmed.connect(on_confirmed); picker.cancelled.connect(on_cancelled)
	check(picker.configure(actor.id,actor,24,registry,presets).ok,"旧政策完成真实默认半身头像")
	if picker._closed: picker.free(); quit(1); return
	var identities: Dictionary={}
	for preset in presets:
		find_button(picker,"appearance_choice_id",preset.id).pressed.emit()
		check(picker.draft()=={"base_features":preset.base_features,"attire_style":preset.attire_style},"真实旧代表完整选择 "+preset.id)
		var half: Dictionary=picker._half.describe(); var avatar: Dictionary=picker._avatar.describe()
		check(half.identity_key==avatar.identity_key and not half.has("adult_composition") and not avatar.has("adult_composition"),"旧绑定保持原渲染路径及同一身份 "+preset.id)
		check(picker._half.image_copy()!=null and picker._avatar.image_copy()!=null,"原PNG产出整对图像 "+preset.id)
		identities[half.identity_key]=true
	check(identities.size()==12 and confirmations.is_empty(),"12原身份完整预览且尚未确认")
	find_button(picker,"appearance_choice_id",presets[0].id).pressed.emit()
	var reads: int=registry.texture_reads
	find_button(picker,"appearance_choice_id",presets[1].id).pressed.emit()
	var pair_reads: int=registry.texture_reads-reads
	check(pair_reads>1,"故障注入位置来自真实整对纹理读取")
	find_button(picker,"appearance_choice_id",presets[0].id).pressed.emit()
	var before: Dictionary=picker.draft()
	var half: Control=picker._half; var avatar: Control=picker._avatar
	var half_description: Dictionary=half.describe(); var avatar_description: Dictionary=avatar.describe()
	var half_bytes: PackedByteArray=half.image_copy().get_data(); var avatar_bytes: PackedByteArray=avatar.image_copy().get_data()
	registry.fail_texture_at=registry.texture_reads+pair_reads
	find_button(picker,"appearance_choice_id",presets[1].id).pressed.emit()
	check(picker.draft()==before and picker._half==half and picker._avatar==avatar,"最后头像读取失败保持原草案与整对节点")
	check(half.describe()==half_description and avatar.describe()==avatar_description and half.image_copy().get_data()==half_bytes and avatar.image_copy().get_data()==avatar_bytes,"最后头像失败保留原两图全部描述及真实RGBA")
	registry.fail_texture_at=registry.texture_reads+1
	find_button(picker,"appearance_action","confirm").pressed.emit()
	check(confirmations.is_empty() and not picker._closed,"确认时真实来源失效不提交")
	registry.fail_texture_at=-1
	find_button(picker,"appearance_action","confirm").pressed.emit()
	find_button(picker,"appearance_action","confirm").pressed.emit()
	check(confirmations.size()==1 and confirmations[0]==before,"真实来源恢复只确认一次")
	check(picker.configure(actor.id,actor,24,registry,presets).ok,"真实旧政策可重新打开")
	find_button(picker,"appearance_action","cancel").pressed.emit()
	picker.confirm_selection()
	check(cancellations==1 and confirmations.size()==1 and picker.draft().is_empty(),"真实旧政策取消后无晚确认")
	picker.free()
	await process_frame
	print("REGIONAL_COMPOSITION_LEGACY_REAL_CHECKS=",checks," FAILURES=",failures," NEW_COMPOSITION_PIXELS=NOT_RUN WORLD=NOT_RUN")
	quit(0 if failures==0 else 1)
