## 真实交易与行囊界面回归：钱粮归属来自正式命令，重绘只保存当前货单选择与显示收据。
extends "res://tests/test_growth_screen.gd"
const F=preload("res://tests/companion_fixture.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const Codec=preload("res://core/save_codec.gd")

## 从当前控件元数据选择实际商品与付款人，不改世界库存或钱包。
func choose_trade(item_id: String, quantity: int, payer: String, side: int=0) -> void:
	for index in range(main._item.item_count):
		if main._item.get_item_metadata(index)==item_id: main._item.select(index); main._item.item_selected.emit(index); break
	for index in range(main._payer.item_count):
		if main._payer.get_item_metadata(index)==payer: main._payer.select(index); main._payer.item_selected.emit(index); break
	main._side.select(side); main._side.item_selected.emit(side)
	main._quantity.value=quantity
	await settle()

## 从当前场所正常进入商户对白与货单，所有办理都经过真实Main与Kernel。
func open_shop() -> void:
	if main.kernel.snapshot().actors[main.kernel.snapshot().player_actor_id].location.get("place_id")!="west.street": await perform("move_local",{"place_id":"west.street"})
	await perform("move_local",{"place_id":"west.grain"})
	await perform("talk",{"role_id":"role.merchant"})
	await perform("dialogue_next")
	await perform("dialogue_choose",{"option_id":"goods"})
	await perform("dialogue_confirm")
	check(main.kernel.interaction_view().phase=="trade","真实商人对白打开货单")

## 只读收集已生成标签，核对玩家能看到的数量、产权和预留说明。
func labels(node: Node) -> String:
	var result: String=node.text+"\n" if node is Label else ""
	for child in node.get_children(): result+=labels(child)
	return result

## 读取本人实际所有的非预留粮食；代管公物必须另按其产权核对。
func owned_grain(state: Dictionary) -> int:
	var player: Dictionary=state.actors[state.player_actor_id]
	return Inventory.count(state.inventories[player.inventory_id],"item.grain",[{"kind":"actor","id":player.id}])

## 成交后重绘、失败重试、存读与公款采购均覆盖；界面不得增减数量或改写物品产权。
func run_checks() -> void:
	main=Host.new(); root.add_child(main); main.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	await settle()
	check(main.start_content("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json").ok,"正常Main装载Alpha")
	if not main._started: finish(); return
	await open_shop()
	var before: Dictionary=main.kernel.snapshot(); var player: Dictionary=before.actors[before.player_actor_id]
	await choose_trade("item.grain",10,player.wallet_id)
	var request: Dictionary=main._trade_request.duplicate(true)
	var quote: Dictionary=main.kernel.preview("trade",request)
	check(quote.ok and quote.total==473,"十粮来自正式473钱报价")
	main._render(); await settle()
	check(main._trade_request==request and main.kernel.snapshot()==before,"同商户重绘保留粮食、数量和私人付款来源，不改世界")
	main._confirm_trade()
	var pending: Dictionary=main.kernel.snapshot()
	check(owned_grain(pending)==owned_grain(before)+10 and pending.tick==before.tick,"成功提交已将十粮交入本人行囊，装货尚未走半日")
	check(control("TradeReceipt")!=null and control("TradeReceipt").text.contains("装货尚未完成"),"实际已交割而未完工时收据如实说明等待装货")
	await settle()
	var after: Dictionary=main.kernel.snapshot()
	check(owned_grain(after)==owned_grain(before)+10 and after.tick==before.tick+1,"完成交易后本人实际粮食净增十份且只用一个半日")
	check(after.wallets[player.wallet_id].balance==before.wallets[player.wallet_id].balance-quote.total,"私人真实钱包只扣成交报价")
	check(main._trade_request.item_id=="item.grain" and main._trade_request.quantity==10 and main._trade_request.payer_wallet_id==player.wallet_id,"交易重绘后仍为同一十粮货单，不跳回竹木弓")
	check(control("TradeReceipt")!=null and control("TradeReceipt").text.contains("粮食 ×10") and control("TradeReceipt").text.contains("473钱") and control("TradeReceipt").text.contains("交易完成"),"真实完成收据明确列出十粮、473钱与结果")
	check(main._quote_label.text.begins_with("下一笔暂不能成交"),"余额不足明确指向下一笔，不能掩盖上一笔已成功")
	await capture("trade-completed-receipt")
	var receipt: Dictionary=main._trade_receipt.duplicate(true)
	main._perform("trade",main._trade_request.duplicate(true)); await settle()
	check(main.kernel.snapshot()==after and main._trade_receipt==receipt,"失败重试不扣款、不增粮，也不生成新成功收据")
	await choose_trade("item.grain",2,player.wallet_id,1)
	main._render(); await settle()
	check(main._trade_request.side=="sell" and main._trade_request.quantity==2 and main._trade_request.item_id=="item.grain","卖出方向与数量也在同场重绘后保留")
	main._save(); await settle()
	check(main.last_storage_result.get("ok",false),"当前成交世界可以通过正式存储保存")
	var sell_quote: Dictionary=main.kernel.preview("trade",main._trade_request)
	main._confirm_trade(); await settle()
	var sold: Dictionary=main.kernel.snapshot()
	check(sell_quote.ok and owned_grain(sold)==8 and sold.wallets[player.wallet_id].balance==after.wallets[player.wallet_id].balance+sell_quote.total and sold.tick==after.tick+1,"正常卖出两粮按正式报价交回实物并收款")
	check(control("TradeReceipt")!=null and control("TradeReceipt").text.contains("已卖出 粮食 ×2") and main._trade_receipt.quote.side=="sell","后来成功的卖出收据替换上一笔买入，数量与方向如实显示")
	main._load(); await settle()
	check(main.last_storage_result.get("ok",false) and main.kernel.snapshot()==after,"读取恢复准确钱粮和原货单会话")
	check(main._trade_receipt.is_empty() and control("TradeReceipt")==null,"读档不把载入前的旧收据伪装成新成交")
	var import_path: String="user://trade-inventory-import.hmsave"
	var import_file=FileAccess.open(import_path,FileAccess.WRITE)
	import_file.store_string(Codec.encode(after,0,"trade-inventory-import",main.kernel.content())); import_file.close()
	await choose_trade("item.grain",1,player.wallet_id,1)
	main._confirm_trade(); await settle()
	check(not main._trade_receipt.is_empty(),"导入前确实存在后来成功的交易收据")
	var imported: Dictionary=main.import_snapshot(import_path); await settle()
	check(imported.ok and main.kernel.snapshot()==after and main._trade_receipt.is_empty() and control("TradeReceipt")==null,"正式文件导入恢复旧世界并清除后来交易收据")
	await perform("close_trade")
	check(main._trade_request.is_empty() and main._trade_receipt.is_empty(),"收起货单清除当前交易表现状态")
	await perform("end_interaction")
	main._open_growth(); await settle(); await click("GrowthTabInventory")
	var inventory_text: String=labels(control("GrowthInventory"))
	check(inventory_text.contains("粮食 ×10") and inventory_text.contains("本人财物 · 未预留"),"行囊页看得到实际十粮及本人产权")
	check(control("GrowthInventoryWeight").text.contains("%d / %d"%[Inventory.weight(after.inventories[player.inventory_id],main._data),after.inventories[player.inventory_id].capacity]),"携带重量与容量来自同一真实库存")
	var viewed: Dictionary=main.kernel.snapshot()
	await click("GrowthTabSkills"); await click("GrowthTabInventory")
	root.size=Vector2i(1024,768); await settle()
	check(main.kernel.snapshot()==viewed,"切换行囊、武学及缩放窗口完全只读")
	check(control("GrowthScreen").get_global_rect().end.x<=root.get_visible_rect().size.x+1 and control("GrowthScreen").get_global_rect().end.y<=root.get_visible_rect().size.y+1,"较窄窗口仍容纳行囊面板")
	await capture("trade-private-inventory")
	await click("GrowthClose")
	await open_shop()
	await choose_trade("item.grain",1,player.wallet_id,1)
	main._confirm_trade(); await settle()
	check(not main._trade_receipt.is_empty(),"新局前确实保留上一局最新成功交易收据")
	await public_inventory()
	finish()

## 正常接采购委托并用任务钱包买粮，回报预留通过Kernel提交以观察尚未结算的真实批次。
func public_inventory() -> void:
	check(main.start_content("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json").ok,"新游戏建立另一真实旅途")
	check(main._trade_receipt.is_empty() and main._trade_request.is_empty(),"新局不会沿用上一世界货单或收据")
	await perform("move_local",{"place_id":"west.office"})
	main._perform("inspect",{"prop_id":"notice.grain"}); await settle()
	check(not main.kernel.snapshot().knowledge[main.kernel.snapshot().player_actor_id].is_empty(),"正式读告示获知采购任务")
	await perform("talk",{"role_id":"role.office"})
	await perform("dialogue_next")
	await perform("dialogue_choose",{"option_id":"accept"})
	await perform("dialogue_confirm")
	await perform("end_interaction")
	var state: Dictionary=main.kernel.snapshot(); var task: Dictionary=state.tasks.values()[0]
	await open_shop()
	await choose_trade("item.grain",10,task.budget_wallet_id)
	main._render(); await settle()
	check(main._trade_request.payer_wallet_id==task.budget_wallet_id,"重绘保留当前委托钱包，不偷换个人经费")
	main._confirm_trade(); await settle()
	state=main.kernel.snapshot(); var player: Dictionary=state.actors[state.player_actor_id]
	check(owned_grain(state)==0 and Inventory.count(state.inventories[player.inventory_id],"item.grain",[{"kind":"task","id":task.id}])==10,"公款十粮在本人容器中仍归委托所有")
	check(control("TradeReceipt")!=null and control("TradeReceipt").text.contains("公物已入行囊"),"公款收据不谎称取得私人物品")
	check(not main._trade_receipt.is_empty(),"收起公款货单前确实有当前成交收据")
	await perform("close_trade")
	check(main._trade_receipt.is_empty() and main._trade_request.is_empty(),"收起货单会真正清除刚才的成功收据与选择")
	await perform("end_interaction")
	await perform("move_local",{"place_id":"west.street"}); await perform("move_local",{"place_id":"west.office"})
	var submitted: Dictionary=F.send(main.kernel,"report_offer",{"offer_id":task.offer_id,"context":F.context(main.kernel,"role.office")})
	check(submitted.ok,"正式回报提交实际任务粮食预留："+str(submitted))
	main._open_growth(); await settle()
	if control("GrowthInventory")==null: await click("GrowthTabInventory")
	var reserved: Dictionary=main.kernel.snapshot()
	var text: String=labels(control("GrowthInventory"))
	check(text.contains("粮食 ×10") and text.contains("委托公物") and text.contains("已预留 · 委托交接"),"行囊准确显示公物产权与真实回报预留")
	main._render(); await settle()
	check(main.kernel.snapshot()==reserved,"查看预留公物不提前交货或解除预留")
	await capture("trade-public-reserved")

## Headless只代表控件与真实规则回归，图形截图必须单独使用实际渲染驱动。
func finish() -> void:
	if is_instance_valid(main):
		main._ambience.music.stop(); main._ambience.effect.stop()
		main._ambience.music.stream=null; main._ambience.effect.stream=null
		main.queue_free()
	await settle()
	print("TRADE_INVENTORY_SCREEN_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
