## 青川1.8沈蘅实际生活生产者；只操作当前可见控件，世界冷读仅作原始证据。
extends "../two_actors_main_node/main_entry.gd"

const P = "society::"
var steps: Array = []
var initial: Dictionary = {}
var step_serial: int = 0
var boundary: String = ""

## 交易起止有限财产快照只来自实际冷世界；为独立归账保留双方余额、物品和本人本领。
func assets() -> Dictionary:
	var current: Dictionary=state()
	return {"tick":current.get("tick"),"wallets":current.get("wallets",{}),"inventories":current.get("inventories",{}),"actor":current.get("actors",{}).get(binding.player_actor_id,{}),"history_count":current.get("history",[]).size()}

## 每次命令保留产品原回执及当前本人上下文，不补造提交信封或以差额冒充收据。
func observed_command(kind: String, result: Dictionary, before: Dictionary, after: Dictionary) -> void:
	commands.append({"kind":kind,"result":result.duplicate(true),"before":before.duplicate(true),"after":after.duplicate(true),"current_command_context":main.kernel.command_context().duplicate(true)})
	write("commands.json",encode(commands))

## 只读当前可见文本和按钮，供失败时复核首次确认是否真正发出了原信号。
func visible_tree(node: Node) -> Array:
	var rows: Array=[]
	if node is Control and node.is_visible_in_tree() and not node.is_queued_for_deletion():
		if node is Label: rows.append({"node":node.name,"text":node.text})
		elif node is Button:
			var row: Dictionary={"node":node.name,"text":node.text,"disabled":node.disabled,"path":str(node.get_path()),"metadata":{}}
			for key in node.get_meta_list(): row.metadata[str(key)]=node.get_meta(key)
			rows.append(row)
	for child in node.get_children(): rows.append_array(visible_tree(child))
	return rows

## 步进从当前页按钮开始，前后记录实际上下文与动作；不调用隐藏报价或内部行动入口。
func click(kind: String, target: String="") -> bool:
	if not await settled(): return false
	step_serial+=1
	var before: Dictionary={"step":step_serial,"kind":kind,"target":target,"source":main.kernel.source_identity().duplicate(true),"command_context":main.kernel.command_context().duplicate(true),"interaction":main.kernel.interaction_view().duplicate(true),"action":main.kernel.action_context().duplicate(true),"visible":visible_tree(main.page),"command_count":commands.size()}
	if kind in ["map_confirm","dialogue_confirm","trade","equip_weapon","advance"]: before["assets"]=assets()
	steps.append(before)
	write("steps.json",encode(steps))
	if not press(main.page,kind,target): return false
	if not await settled(): return false
	before["after"]={"source":main.kernel.source_identity().duplicate(true),"command_context":main.kernel.command_context().duplicate(true),"interaction":main.kernel.interaction_view().duplicate(true),"action":main.kernel.action_context().duplicate(true),"command_count":commands.size(),"message":main._message}
	if before.has("assets"): before.after["assets"]=assets()
	write("steps.json",encode(steps))
	return check(main.kernel.command_context().get("actor_id")==binding.player_actor_id,"step_keeps_selected_actor_%d"%step_serial)

## 仅导出当前原生世界，不将证据写回存槽或参与玩法选择。
func state() -> Dictionary:
	var parsed: Variant=JSON.parse_string(world())
	return parsed if parsed is Dictionary else {}

## 表现输入按实际人物查询，等待产品读完成；九槽档案和正式半身/头像各保留原序列化。
func presentation() -> Dictionary:
	var input: Dictionary=main.kernel.character_input(binding.player_actor_id)
	if input.get("code")=="NativeAsyncRequired":
		if not await settled(): return {}
		input=main.kernel.character_input(binding.player_actor_id)
	var art: Script=load("res://ui/art_catalog.gd")
	return {"input":input,"half_body":art.character_input(input,"half_body"),"avatar":art.character_input(input,"avatar")}

## 确认页现有报价来自生产页面缓存；只抄显示过的读结果，不请求额外预览。
func record_quote(label: String) -> bool:
	var row: Dictionary={"source":main.kernel.source_identity(),"context":main.kernel.command_context(),"interaction":main.kernel.interaction_view(),"visible":visible_tree(main.page),"trade_request":main._trade_request.duplicate(true)}
	return write(label+"-quote.json",encode(row))

## 经当面武师对白选择作者已有服务，当前说话者繁忙或按钮禁用时保留实际阻挡。
func sword_option(option: String) -> bool:
	if main.kernel.interaction_view().get("phase")!="scene":
		if control(main.page,"command_kind","end_interaction")!=null:
			if not await click("end_interaction"): return false
	if not await click("talk",P+"role.sword_master"): return false
	for ignored in range(6):
		var view: Dictionary=main.kernel.interaction_view()
		if view.get("phase")=="choice": return await click("dialogue_choose",option)
		if not check(view.get("phase")=="say","actual_sword_dialogue_say"): return false
		if not await click("dialogue_next"): return false
	return check(false,"sword_dialogue_bounded_choice")

## 普通短行动仅走当前原继续按钮；需要本人输入时立即保留边界，绝不预热确认。
func finish_action(kind: String) -> bool:
	for ignored in range(12):
		var action: Dictionary=main.kernel.action_context()
		if action.get("action_id","").is_empty(): return true
		if not check(action.get("action_kind")==kind,"same_original_action_"+kind): return false
		if not check(not main.kernel.action_needs_input(),"action_has_no_unhandled_personal_input"): return false
		if not await click("advance"): return false
	return check(false,"original_action_duration_bound")

## 原交易选项只发用户选择信号，重绘后重新获取控件；不修改报价请求或钱包。
func select_option(field: String, value: String) -> bool:
	var option: OptionButton=main.get(field)
	if not check(is_instance_valid(option) and option.is_visible_in_tree(),"visible_option_"+field): return false
	for index in range(option.item_count):
		if option.get_item_metadata(index)==value:
			option.select(index); option.item_selected.emit(index)
			return await settled()
	return check(false,"missing_option_"+value)

## 一把剑由货单原数量控件选择，不能假定库存默认数量就是购买数量。
func choose_one_sword() -> bool:
	if not await select_option("_item",P+"item.practice_sword"): return false
	if not check(is_instance_valid(main._quantity) and main._quantity.is_visible_in_tree(),"actual_sword_quantity_control"): return false
	main._quantity.value=1
	if not await settled(): return false
	return check(main._trade_request.get("quantity")==1,"one_actual_sword_quantity")

## 从实际武学行囊选持有之剑再换装；装备变更仍走产品原生命令。
func equip_sword() -> bool:
	if not await click("end_interaction") or not await click("system_menu") or not await click("growth"): return false
	var option: OptionButton=main.page.find_child("GrowthWeaponChoice",true,false)
	if not check(option!=null and option.is_visible_in_tree(),"actual_growth_weapon_choice"): return false
	var found: bool=false
	for index in range(option.item_count):
		if option.get_item_metadata(index)==P+"item.practice_sword":
			option.select(index); option.item_selected.emit(index); found=true; break
	if not check(found,"purchased_sword_in_equipment_menu") or not await settled(): return false
	return await click("equip_weapon",P+"item.practice_sword") and await click("growth_close")

## 由当前舆图真实目的地选择与确认，逐半日走原路，不替旅行输入决策或改起点。
func travel_to_sword_yard() -> bool:
	if not await click("map") or not await click("map_select",P+"settlement.sword_yard") or not await click("map_confirm"): return false
	if not await finish_action("travel"): return false
	if control(main.page,"command_kind","map_close")!=null and not await click("map_close"): return false
	var place: String=main.kernel.presentation_location().get("place_id","")
	return check(main._data.places.get(place,{}).get("settlement_id")==P+"settlement.sword_yard","real_sword_yard_arrival")

## 银行目录只读遍历；文件由正常菜单存档生成，不由驱动写造。
func bank_hashes() -> Dictionary:
	var result: Dictionary={}
	for file in DirAccess.get_files_at(bank_directory): result[file]=FileAccess.get_sha256(bank_directory.path_join(file))
	return result

## 商品装备后立即走真实存档菜单，另存原银行副本供失败恢复及中央保全，不覆盖产品槽。
func save_equipment_checkpoint() -> bool:
	var raw: String=world()
	var source: Dictionary=main.kernel.source_identity().duplicate(true)
	if control(main.page,"command_kind","save")==null and not press(main.page,"system_menu"): return false
	if not press(main.page,"save") or not await confirm_slot() or not await settled(): return false
	if not check(main.last_storage_result.get("ok",false) and world()==raw,"equipment_checkpoint_normal_save"): return false
	for file in DirAccess.get_files_at(bank_directory):
		var target=FileAccess.open(output.path_join("equipment-bank-"+file),FileAccess.WRITE)
		if target==null: return check(false,"equipment_bank_copy")
		target.store_buffer(FileAccess.get_file_as_bytes(bank_directory.path_join(file))); target.close()
	write("equipment-world.json",raw)
	write("equipment-checkpoint.json",encode({"source":source,"bank_directory":bank_directory,"banks":bank_hashes()}))
	print("QINGCHUAN_ACTIVE_EQUIPMENT_BANK_READY ",output.path_join("equipment-checkpoint.json"))
	if control(main.page,"command_kind","save")!=null: return await click("system_menu")
	return true

## 经费不足是实际玩法限制：先取消当面报价，按原岗位再做一份工，再重新当面核价。
func affordable_sword_option(option: String, label: String) -> bool:
	for index in range(5):
		if not await sword_option(option) or not record_quote(label+"-%d"%index): return false
		var confirm: Button=control(main.page,"command_kind","dialogue_confirm")
		if not check(confirm!=null,"actual_"+label+"_confirmation"): return false
		if not confirm.disabled: return true
		var words: String=""
		for row in visible_tree(main.page): words+=str(row.get("text",""))+"\n"
		if not check(words.contains("经费不足"),"only_visible_insufficient_funds_can_request_more_work"): return false
		if not await click("dialogue_cancel") or not await sword_option("work") or not record_quote(label+"-extra-work-%d"%index) or not await click("dialogue_confirm") or not await finish_action("service"): return false
	return check(false,"funding_more_than_observed_representative_chain")

## 最终稳定点沿正常菜单写物理银行，交付完整world、外貌、动作及来源原件。
func save_boundary() -> bool:
	var raw: String=world()
	var actual: Dictionary=JSON.parse_string(raw)
	var source: Dictionary=main.kernel.source_identity().duplicate(true)
	var shown: Dictionary=await presentation()
	var battle: Dictionary=main.kernel.personal_battle_view().duplicate(true)
	if battle.get("code")=="NativeAsyncRequired":
		if not await settled(): return false
		battle=main.kernel.personal_battle_view().duplicate(true)
	if control(main.page,"command_kind","save")==null and not press(main.page,"system_menu"): return false
	if not press(main.page,"save") or not await confirm_slot() or not await settled(): return false
	if not check(main.last_storage_result.get("ok",false) and main.kernel.source_identity()==source and world()==raw,"normal_menu_saves_exact_active_world"): return false
	if not write("saved-world.json",raw) or not write("saved-appearance.json",encode(actual.actors[binding.player_actor_id].appearance)) or not write("saved-presentation.json",encode(shown)) or not write("saved-source.json",encode(source)) or not write("saved-interaction.json",encode(main.kernel.interaction_view())): return false
	var manifest: Dictionary={"format":"HM-QINGCHUAN-ACTIVE-PRODUCER/1","producer_pid":OS.get_process_id(),"binding_sha256":FileAccess.get_sha256(OS.get_cmdline_user_args()[0]),"bank_directory":bank_directory,"banks":bank_hashes(),"source":source,"actor_id":binding.player_actor_id,"boundary":boundary,"action":main.kernel.action_context(),"interaction":main.kernel.interaction_view(),"personal_battle":battle,"world_path":output.path_join("saved-world.json"),"appearance_path":output.path_join("saved-appearance.json"),"presentation_path":output.path_join("saved-presentation.json"),"steps_path":output.path_join("steps.json"),"commands_path":output.path_join("commands.json")}
	print("QINGCHUAN_ACTIVE_BANK_READY ",output.path_join("save-manifest.json"))
	return write("save-manifest.json",encode(manifest))

## 单次正常标题选择原貌沈蘅，按真实武师短工、货单、武学与切磋到规划存点。
func active_chain() -> bool:
	if not open_people() or not choose_person(): return false
	var frozen: Dictionary=main._existing_actor_picker.selected().duplicate(true)
	if not check(frozen.appearance_policy=="preserve_existing_v1","selected_original_appearance_policy") or not press(main._existing_actor_picker,"confirm","","existing_actor_action") or not await settled(): return false
	main.kernel.command_completed.connect(observed_command)
	initial=state()
	var actor: Dictionary=initial.actors[binding.player_actor_id]
	if not check(initial.player_actor_id==binding.player_actor_id and actor.birth_tick==-43*720 and actor.skills.martial==1200 and initial.wallets[P+"wallet.study_swordswoman"].balance==8,"actual_43_year_old_shen_heng_start"): return false
	if not check(not is_instance_valid(main._appearance_picker) and native_backends_seen==1,"one_original_person_one_native_world"): return false
	if not write("initial-world.json",world()) or not write("expanded-catalog.json",encode(prepared)) or not write("person-selection.json",encode(frozen)): return false
	if not await travel_to_sword_yard() or not await click("move_local",P+"place.sword_court"): return false
	# 必要费用先读实际货单；束脩和切磋仍在后续各自确认页核价。
	if not await sword_option("gear") or not await click("dialogue_confirm") or not await choose_one_sword(): return false
	if not record_quote("initial-sword"): return false
	var sword_cost: int=int(main._trade_request.get("quoted_total",0))
	# 余额不足时货单仍显示当前价格；若产品没有成功报价，不从base_price猜交易金额。
	if sword_cost<=0:
		var label: String=main._quote_label.text
		if not write("initial-unaffordable-sword.txt",label): return false
	if not await click("end_interaction"): return false
	# 三份作者既有30钱短工覆盖一剑、一招及8钱切磋；每份都单独核实际回执。
	for index in range(3):
		if not await sword_option("work") or not record_quote("work-%d"%index) or not await click("dialogue_confirm") or not await finish_action("service"): return false
		if not write("work-%d-state.json"%index,world()): return false
	if not await sword_option("gear") or not await click("dialogue_confirm") or not await choose_one_sword() or not record_quote("purchase-sword"): return false
	if not await click("trade") or not await finish_action("trade"): return false
	if not write("purchase-receipt.json",encode(main._trade_receipt)) or not await equip_sword(): return false
	if not await save_equipment_checkpoint(): return false
	if not await affordable_sword_option("cut","learn-cut") or not await click("dialogue_confirm") or not await finish_action("learn_skill"): return false
	if not await affordable_sword_option("spar","spar") or not await click("dialogue_confirm"): return false
	var battle: Dictionary=main.kernel.personal_battle_view()
	if battle.get("code")=="NativeAsyncRequired":
		if not await settled(): return false
		battle=main.kernel.personal_battle_view()
	if not check(battle.get("actor_id")==binding.player_actor_id and battle.get("battle",{}).get("phase")=="planning","real_selected_person_sparring_planning"): return false
	boundary="personal_battle_planning"
	return await save_boundary()

## 失败在仍可正常操作的稳定世界中另留原存点；成功标记不因保存失败边界而改写。
func produce() -> bool:
	var ok: bool=await active_chain()
	if not ok and main._started and main._resident_mode and not main.kernel.is_busy():
		boundary="blocked_at_visible_control"
		await save_boundary()
	return ok

## 无论通过或失败都保留当前页面和完整世界，失败不得改输入重放掩盖原边界。
func finish(ok: bool) -> bool:
	if is_instance_valid(main) and main._started:
		write("terminal-world.json",world())
		write("terminal-page.json",encode({"source":main.kernel.source_identity(),"command_context":main.kernel.command_context(),"interaction":main.kernel.interaction_view(),"action":main.kernel.action_context(),"visible":visible_tree(main.page),"message":main._message}))
	write("steps.json",encode(steps)); write("commands.json",encode(commands))
	return super.finish(ok)
