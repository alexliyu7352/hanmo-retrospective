## 自然成长存档经真实文件选择器进入Main，再以两次独立进程核对成年继任与持续行旅。
extends "res://tests/test_family_screen.gd"
const SaveFiles=preload("res://storage/save_files.gd")
const SaveCodec=preload("res://core/save_codec.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const Location=preload("res://core/world_location.gd")
var natural_save: String=""
var expected_save: String=""
var resumed: bool=false

## 必须提供真实自然存档和独立原生输入；无前置时明确跳过，不能把跳过计为流程通过。
func run_checks() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--natural-save="): natural_save=arg.trim_prefix("--natural-save=")
		if arg.begins_with("--expected-save="): expected_save=arg.trim_prefix("--expected-save=")
		if arg=="--resume": resumed=true
	if natural_save.is_empty() or external_input_directory.is_empty() or DisplayServer.get_name()=="headless":
		print("FAMILY_SUCCESSION_SCREEN_SKIPPED: 需要自然成长存档与X11原生输入")
		quit(); return
	main=Host.new(); root.add_child(main); main.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	await settle()
	if resumed: await resume_world()
	else: await inherit_world()
	await finish()

## 首进程通过文件选择器输入用户提供的真实路径，不调用restore、导入回调或改写状态。
func import_file() -> bool:
	await click("import")
	var dialogs: Array=main.find_children("*","FileDialog",true,false)
	check(dialogs.size()==1,"真实导入文件选择器已经打开")
	if dialogs.size()!=1: return false
	var dialog: FileDialog=dialogs[0]
	var field: LineEdit=dialog.get_line_edit()
	var point: Vector2=field.get_global_rect().get_center()+Vector2(dialog.position)
	await mouse(point,"导入文件路径输入框")
	await external_input({"kind":"chord","keys":["Control_L","a"]})
	await external_input({"kind":"type","text":natural_save})
	await external_input({"kind":"capture","name":"02-import-file-dialog"})
	await external_input({"kind":"keys","keys":["Return"]})
	await settle()
	check(main._started and main._message.begins_with("已导入旅途"),"原生文件选择器读取并正式导入自然成长存档："+main._message)
	return main._started and main._message.begins_with("已导入旅途")

## 指定、退隐、领粮和外出均点击现有界面，保留原世界的公共钱包与前代身份。
func inherit_world() -> void:
	await capture("01-import-title")
	if not await import_file(): return
	var decoded: Dictionary=SaveFiles.read_world(natural_save,main.kernel.content())
	check(decoded.ok,"输入文件由正式Codec校验，可只读比较")
	if not decoded.ok: return
	var source: Dictionary=decoded.state
	var current: Dictionary=main.kernel.snapshot()
	check(current==source,"实际导入完整保留自然存档的同一世界")
	var parent_id: String=current.player_actor_id
	var child: String=adult_child(current,parent_id)
	check(not child.is_empty(),"自然存档包含已达到约定年岁且尚未指定的真实子女")
	if child.is_empty(): return
	var private_estate: String=owned_here(current,parent_id)
	check(not private_estate.is_empty(),"当面交接发生在本人实际私庄")
	if private_estate.is_empty(): return
	var public_before: Dictionary=public_wallets(current)
	var child_knowledge: Dictionary=current.knowledge[child].duplicate(true)
	var child_combat: Dictionary=current.actors[child].combat.duplicate(true)
	await click("family_open")
	check(main.find_child("FamilyPlayerAge",true,false).text.begins_with(str(Identity.age_years(current,parent_id))+"岁"),"本人实际年龄与自然存档一致")
	await capture("03-adult-before-designation")
	var designation: Dictionary=main.kernel.preview("family_designate",{"heir_actor_id":child})
	check(designation.ok,"实际成年子女可当面指定")
	await click("family_designate",child)
	current=main.kernel.snapshot()
	check(current.tick==source.tick and current.family.designations.has(parent_id),"指定产生真实事实且不暗加日期")
	check(current.facts[current.family.designations[parent_id]].heir_actor_id==child,"指定指向真实动态人物")
	await capture("04-adult-designated")
	var retirement: Dictionary=main.kernel.preview("family_retire",{})
	check(retirement.ok,"正式预览允许当前私产交接")
	if not retirement.ok: return
	var cash: int=current.wallets[current.actors[child].wallet_id].balance
	for entry in retirement.family.assets.wallets: cash+=entry.amount
	await click("family_retire")
	current=main.kernel.snapshot()
	check(current.player_actor_id==child and current.tick==source.tick,"退隐后由同一世界成年子女接掌，日期保持")
	check(current.actors.has(parent_id) and current.actors[parent_id].alive and current.actors[parent_id].birth_tick==source.actors[parent_id].birth_tick,"前代真人仍存活，身份与出生日期没有被替换")
	check(current.family.successions.size()==source.family.successions.size()+1,"新增永久继任事实而不是仅交换人物ID")
	check(public_wallets(current)==public_before,"公共组织钱柜在指定和退隐前后逐项保持一致")
	check(current.estates[private_estate].owner_actor_id==child and current.wallets[current.actors[child].wallet_id].balance==cash,"实际私庄与报价私钱移交到继任人")
	check(current.knowledge[child]==child_knowledge and current.actors[child].combat==child_combat,"继任不复制前代知识或武学")
	check(main.find_child("FamilyPlayerName",true,false).text==main._name(Identity.definition(current,main._data,child)),"家庭页立即显示真实继任人动态姓名")
	await capture("05-successor-family")
	await click("family_close")
	await click("estate_open",private_estate)
	check(main.find_child("EstateScreen",true,false)!=null,"继任人通过原场所打开同一私庄")
	await capture("06-successor-estate")
	await click("estate_tab","storage")
	var definition: Dictionary=main._data.estates[private_estate]
	var bag: String=current.actors[child].inventory_id
	var owner: Dictionary={"kind":"actor","id":child}
	var carried: int=Inventory.count(current.inventories[bag],definition.food_item_id,[owner])
	var stored_food: int=Inventory.count(current.inventories[definition.inventory_id],definition.food_item_id,[owner])
	await click("estate_withdraw",private_estate)
	current=main.kernel.snapshot()
	check(Inventory.count(current.inventories[bag],definition.food_item_id,[owner])==carried+10 and Inventory.count(current.inventories[definition.inventory_id],definition.food_item_id,[owner])==stored_food-10,"继任人从实际继承私仓领出十份口粮")
	check(public_wallets(current)==public_before,"领粮操作没有支取公共钱柜")
	await capture("07-successor-withdraw")
	await click("estate_close")
	var street: String=main._data.places[definition.place_id].parent_place
	await click("move_local",street)
	check(Location.resolve(main.kernel.snapshot(),child).get("place_id")==street,"新人物实际离庄到街道，没有重开世界")
	await capture("08-successor-outside")
	await click("map")
	var from_settlement: String=main._data.places[street].settlement_id
	var destination: String=""
	for id in main._data.settlements:
		if id!=from_settlement and main.kernel.preview("travel",{"destination":id}).ok: destination=id; break
	check(not destination.is_empty(),"当前继任人存在合法出城路线")
	if destination.is_empty(): return
	await click("map_select",destination)
	await click("map_confirm")
	var departure: Dictionary=main.kernel.snapshot()
	check(not departure.actors[child].action_id.is_empty(),"地图确认建立实际旅行行动")
	await click("advance")
	var encounter: Dictionary=Encounters.pending_for(main.kernel.snapshot(),child)
	if not encounter.is_empty(): await click("encounter_leave")
	current=main.kernel.snapshot()
	check(current.tick==source.tick+1 and Location.resolve(current,child).get("kind")=="Wilderness","真实续行半日后在同一地图的途中位置")
	check(public_wallets(current)==public_before,"外出后公共钱包仍保持独立")
	await capture("09-successor-travelling")
	await click("save")
	await stored()
	check(main.last_storage_result.get("ok",false),"第一进程真实存储服务保存旅行中的继任人")
	var saved: Dictionary=main.kernel.snapshot()
	var expected: String=capture_directory.path_join("expected-after-succession.hmsave")
	var encoded: String=SaveCodec.encode(saved,0,"gui-restart-reference",main.kernel.content())
	check(not encoded.is_empty(),"重启对照由正式Codec编码当前真实世界")
	if not encoded.is_empty():
		var file: FileAccess=FileAccess.open(expected,FileAccess.WRITE); file.store_string(encoded); file.close()
	observed.parent_actor_id=parent_id; observed.heir_actor_id=child; observed.source_tick=source.tick; observed.saved_tick=saved.tick
	observed.public_wallets=public_before; observed.estate_id=private_estate; observed.heir_money=saved.wallets[saved.actors[child].wallet_id].balance; observed.expected_save=expected
	await capture("10-successor-saved")

## 第二进程从标题页继续同一物理存档槽；先完整比较世界，再通过实际旅行按钮续行。
func resume_world() -> void:
	check(not expected_save.is_empty(),"第二进程提供第一进程的正式存档对照")
	if expected_save.is_empty(): return
	await capture("01-restart-title")
	await click("continue")
	await stored()
	check(main._started and main.last_storage_result.get("ok",false),"新进程通过继续旅途读取同一个独立存档槽")
	if not main._started: return
	var decoded: Dictionary=SaveFiles.read_world(expected_save,main.kernel.content())
	check(decoded.ok,"第一进程对照文件通过正式Codec")
	if not decoded.ok: return
	var expected: Dictionary=decoded.state
	var current: Dictionary=main.kernel.snapshot()
	check(current==expected,"跨进程完整恢复同一世界、日期、行程、私产与继任事实")
	var source: Dictionary=SaveFiles.read_world(natural_save,main.kernel.content()).state
	var parent_id: String=source.player_actor_id
	var child: String=current.player_actor_id
	check(child!=parent_id and current.actors[parent_id].alive and current.actors[parent_id].birth_tick==source.actors[parent_id].birth_tick,"重启后控制继任人，前代仍是原来的存活人物")
	check(public_wallets(current)==public_wallets(source),"重启后公共组织钱柜与自然存档逐项一致")
	check(Location.resolve(current,child)==Location.resolve(expected,child) and current.actors[child].action_id==expected.actors[child].action_id,"续行起点和行动身份真实恢复，没有返回城门或瞬移")
	await click("map")
	await capture("02-restarted-world")
	var at_tick: int=current.tick
	var at_location: Dictionary=Location.resolve(current,child)
	await click("advance")
	var encounter: Dictionary=Encounters.pending_for(main.kernel.snapshot(),child)
	if not encounter.is_empty(): await click("encounter_leave")
	current=main.kernel.snapshot()
	check(current.tick==at_tick+1 and Location.resolve(current,child)!=at_location,"重启后再实际前进半日，从保存的途中位置继续")
	check(current.actors[parent_id].alive and current.player_actor_id==child,"续行后仍保留同世界前代和继任控制")
	await capture("03-restarted-continued")
	await click("save")
	await stored()
	check(main.last_storage_result.get("ok",false),"重启续行后的同一存档槽再次成功保存")
	observed.parent_actor_id=parent_id; observed.heir_actor_id=child; observed.restored_tick=at_tick; observed.final_tick=main.kernel.snapshot().tick; observed.public_wallets=public_wallets(current)
	await capture("04-restarted-saved")

## 成年候选取自真实家庭收养来源与冻结门槛，不硬编码动态人物编号。
func adult_child(state: Dictionary, guardian: String) -> String:
	if state.family.designations.has(guardian): return ""
	for child in state.family.children:
		var fact: Dictionary=state.facts[state.family.children[child]]
		if fact.guardian_actor_id==guardian and Identity.age_years(state,child)>=fact.terms.policy.heir_min_age: return child
	return ""

## 操作庄园由当前所有者和真实地点共同确定，不用固定庄名赋予权限。
func owned_here(state: Dictionary, actor: String) -> String:
	for id in state.estates:
		if state.estates[id].owner_actor_id==actor and main._data.estates[id].place_id==Location.resolve(state,actor).get("place_id"): return id
	return ""

## 公共财产按所有者类型整体比较，避免只检查某一个预知的钱柜。
func public_wallets(state: Dictionary) -> Dictionary:
	var result: Dictionary={}
	for id in state.wallets:
		if state.wallets[id].owner.kind=="organization": result[id]=state.wallets[id].duplicate(true)
	return result

## 两个进程分别输出真实观察及退出码；对照文件只记录实际状态，从不作为直接恢复入口。
func finish() -> void:
	observed.checks=checks; observed.failures=failures; observed.native_input=not external_input_directory.is_empty(); observed.resumed=resumed; observed.pid=OS.get_process_id()
	if not capture_directory.is_empty():
		var file: FileAccess=FileAccess.open(capture_directory.path_join("observed.json"),FileAccess.WRITE); file.store_string(JSON.stringify(observed,"  ")); file.close()
	print("FAMILY_SUCCESSION_SCREEN_CHECKS=%d FAILURES=%d RESUMED=%s"%[checks,failures,str(resumed)])
	if is_instance_valid(main): main.queue_free()
	for _frame in range(8): await process_frame
	quit(0 if failures==0 else 1)
