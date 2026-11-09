## 同工程真实主壳验收：以XTest经过标题、城镇、设施、对白、菜单和存档，不替换人物或规则状态。
extends "res://tests/test_family_screen.gd"
const Location=preload("res://core/world_location.gd")
const Inventory=preload("res://core/inventory_rules.gd")

## 正式内容从标题按钮启动，连续走入各设施；图形模式必须经过独立系统输入。
func run_checks() -> void:
	if external_input_directory.is_empty() or DisplayServer.get_name()=="headless":
		print("VISUAL_SHELL_SKIPPED: 需要独立XTest输入"); quit(); return
	main=Host.new(); root.add_child(main); main.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	await settle(); await capture("01-title")
	await click("start")
	check(main._started and main._data.scenarios.has("scenario.alpha_life"),"标题真实启程进入当前Alpha")
	if not main._started: await finish(); return
	var state: Dictionary=main.kernel.snapshot(); var player: String=state.player_actor_id
	var origin: String=Location.resolve(state,player).place_id
	await capture("02-town")
	check(button("save")!=null and not button("save").is_visible_in_tree(),"存储入口实际收进系统菜单")
	var initial: Dictionary=state.duplicate(true)
	await click("system_menu"); await capture("03-system-menu")
	check(button("save").is_visible_in_tree() and main.kernel.snapshot()==initial,"菜单展开可达且不改变世界")
	await click("system_menu")
	for tag in ["hall","shop","tavern","inn","house","camp","estate"]:
		var destination: String=""
		for place in main._data.places.values():
			if place.get("visual_tag")==tag and place.id in main._data.places[origin].passages: destination=place.id; break
		if destination.is_empty(): continue
		await click("move_local",destination)
		check(Location.resolve(main.kernel.snapshot(),player).get("place_id")==destination,"实际进入设施："+tag)
		var role: String=""
		for definition in main._data.roles.values():
			if definition.place_id==destination and main.kernel.preview("talk",{"role_id":definition.id}).ok: role=definition.id; break
		if tag=="hall":
			for offer in main._data.offers.values():
				var candidate: String=offer.get("accept_role","")
				if main._data.roles.has(candidate) and main._data.roles[candidate].place_id==destination and main.kernel.preview("talk",{"role_id":candidate}).ok: role=candidate; break
		check(main.find_children("ScenePersonPortrait","TextureRect",true,false).size()<=1,"设施仅一位真实人物焦点："+tag)
		await capture("04-"+tag+"-scene")
		if not role.is_empty():
			await click("talk",role)
			check(main.kernel.interaction_view().get("context",{}).get("role_id")==role,"名单点击带入真实在场上下文："+tag)
			if main.kernel.interaction_view().phase=="scene": await finish(); return
			check(main.find_child("DialoguePaper",true,false)!=null and main.find_child("SpeakerPortrait",true,false)!=null,"真实对白纸笺与说话者："+tag)
			await capture("05-"+tag+"-dialogue")
			if tag in ["shop","inn","house"]:
				await click("dialogue_next")
				if main.kernel.interaction_view().phase=="choice":
					check(main.find_child("DialogueChoices",true,false).columns==2,"真实作者选项采用两列："+tag)
					await capture("06-"+tag+"-choices")
					if tag=="shop" and button("dialogue_choose","goods")!=null:
						await click("dialogue_choose","goods"); await click("dialogue_confirm")
						check(main.kernel.interaction_view().phase=="trade","纸笺通过真实上下文打开货单")
						await purchase_grain()
			await close_dialogue()
		await click("move_local",origin)
	await click("save"); await stored()
	check(main.last_storage_result.get("ok",false),"通过真实菜单保存独立旅途")
	var saved: Dictionary=main.kernel.snapshot()
	await click("load"); await stored()
	check(main.last_storage_result.get("ok",false) and main.kernel.snapshot()==saved,"真实菜单载入同一世界人物、日期和财产")
	await capture("08-town-restored")
	observed.content_digest=saved.content_digest; observed.saved_tick=saved.tick; observed.player_actor_id=saved.player_actor_id
	await finish()

## 从可见货单选择实有粮食并交付十份，输入只经过鼠标与键盘，核对双方真实财产和半日。
func purchase_grain() -> void:
	var before: Dictionary=main.kernel.snapshot()
	var player: Dictionary=before.actors[before.player_actor_id]
	var shop: Dictionary=main._data.shops[main._trade_request.shop_id]
	var stock: Dictionary=before.inventories[shop.inventory_id]
	var index: int=-1
	for option in range(main._item.item_count):
		var id: String=main._item.get_item_metadata(option)
		if "粮" in main._name(main._data.items[id]) and Inventory.count(stock,id,[stock.owner])>=10: index=option; break
	check(index>=0,"实际商户存在十份可购买粮食")
	if index<0: await click("close_trade"); return
	if main._item.selected!=index:
		await mouse(main._item.get_global_rect().get_center(),"打开真实商品列表")
		var focused: int=main._item.get_popup().get_focused_item()
		var keys: Array=[]
		for _step in range(index+1 if focused<0 else absi(index-focused)): keys.append("Down" if focused<0 or index>focused else "Up")
		keys.append("Return"); await external_input({"kind":"keys","keys":keys}); await settle()
	if int(main._quantity.value)!=10:
		await mouse(main._quantity.get_line_edit().get_global_rect().get_center(),"输入真实采购数量")
		await external_input({"kind":"chord","keys":["Control_L","a"]})
		await external_input({"kind":"type","text":"10"}); await external_input({"kind":"keys","keys":["Return"]}); await settle()
	var request: Dictionary=main._trade_request.duplicate(true)
	var quote: Dictionary=main.kernel.preview("trade",request)
	check(quote.ok and request.quantity==10 and request.side=="buy","货单显示十粮有效同源报价")
	check(main._quote_label.text.contains("交割与装货耗时半日"),"货单可见真实耗时中文说明")
	await capture("07-shop-trade")
	if not quote.ok: await click("close_trade"); return
	await click("trade")
	var after: Dictionary=main.kernel.snapshot()
	check(after.tick==before.tick+1 and after.actors[player.id].action_id.is_empty(),"实际交易交割只经过一个半日并完成行动")
	var owner: Dictionary=before.inventories[player.inventory_id].owner
	check(Inventory.count(after.inventories[player.inventory_id],request.item_id,[owner])==Inventory.count(before.inventories[player.inventory_id],request.item_id,[owner])+10,"十份粮食进入本人实际行囊")
	check(Inventory.count(after.inventories[shop.inventory_id],request.item_id,[stock.owner])==Inventory.count(stock,request.item_id,[stock.owner])-10,"实际商店库存减少十粮")
	check(after.wallets[player.wallet_id].balance==before.wallets[player.wallet_id].balance-quote.total,"本人真实钱包扣除显示报价")
	check(after.wallets[shop.wallet_id].balance==before.wallets[shop.wallet_id].balance+quote.total,"商户真实钱包收到同一交割款")
	observed.trade={"item_id":request.item_id,"quantity":10,"total":quote.total,"tick_before":before.tick,"tick_after":after.tick,"wallet_before":before.wallets[player.wallet_id].balance,"wallet_after":after.wallets[player.wallet_id].balance}
	await capture("07-shop-delivered")
	if button("close_trade")!=null: await click("close_trade")

## 根据当前真实对白退出；不直接清空上下文或调用隐藏结束按钮。
func close_dialogue() -> void:
	for _step in range(5):
		if main.kernel.interaction_view().phase=="scene": return
		if button("end_interaction")!=null: await click("end_interaction")
		elif button("dialogue_choose","leave")!=null: await click("dialogue_choose","leave")
		elif button("dialogue_cancel")!=null: await click("dialogue_cancel")
		elif button("dialogue_next")!=null: await click("dialogue_next")
		else: check(false,"当前对白必须有真实可达的返回入口"); return
	check(main.kernel.interaction_view().phase=="scene","退出对白回到实际场所")

## 每个取证帧同时检查主壳与纸笺几何；可滚动内容由点击助手真实滚入可视区。
func capture(name: String) -> void:
	var viewport: Rect2=Rect2(Vector2.ZERO,Vector2(root.size))
	for key in ["WorldStatus","PlaceTitle","DialogueScroll","SpeakerPortrait","ScenePersonPortrait"]:
		var control: Control=main.find_child(key,true,false)
		if control!=null: check(viewport.encloses(control.get_global_rect()),"主壳区域在屏幕内："+key+" "+name)
	await super.capture(name)

## 保存本次真实输入与世界观测，失败保持非零退出码。
func finish() -> void:
	observed.checks=checks; observed.failures=failures; observed.native_input=true; observed.display=DisplayServer.get_name()
	if not capture_directory.is_empty():
		var file=FileAccess.open(capture_directory.path_join("observed.json"),FileAccess.WRITE); file.store_string(JSON.stringify(observed,"  ")); file.close()
	print("VISUAL_SHELL_CHECKS=%d FAILURES=%d"%[checks,failures])
	if is_instance_valid(main): main.queue_free(); main=null
	await settle(); quit(0 if failures==0 else 1)
