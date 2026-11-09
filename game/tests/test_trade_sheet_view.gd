## 真实Main货单运行数据门：有限边界输入调用原Kernel报价，旧文字oracle不重录，不验收资金结算或GUI。
extends SceneTree
const Main=preload("res://ui/main.gd")
const Kernel=preload("res://core/world_kernel.gd")
const Registry=preload("res://core/runtime/module_registry.gd")
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const ORACLE_SHA="22f135e5b8598f87657584bdc8970e7c6c9badd1a37e5468fb89859f5ae25adc"
const VIEW_PATH="res://core/views/trade_sheet.gd"
var checks: int=0
var failures: int=0
var registrations: Dictionary={}
var snapshot_counts: Dictionary={}

## 只观察真实快照次数，报价与全部其它门面保持生产实现。
class CountedKernel extends "res://core/world_kernel.gd":
	var snapshot_calls: int=0
	## 调用原深副本，不以替身或缓存伪造减少次数。
	func snapshot() -> Dictionary:
		snapshot_calls+=1; return super.snapshot()

## 延迟运行，使真实类注册就绪；不启动Main的存储、声音和世界加载。
func _init() -> void: call_deferred("_run")

## 每个失败进入退出码，脚本异常另以原日志审核。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 商品故意逆字母顺序，商店只数自己未预留货物，任务菜单也沿原插入顺序。
func fixture() -> Dictionary:
	var state: Dictionary={"tick":0,"revision":3,"player_actor_id":"buyer","next_instance_id":20,"actors":{},"roles":{"role.shop":{"actor_id":"merchant","enabled":true}},"wallets":{},"inventories":{},"actions":{},"facts":{},"battles":{},"interactions":{},"encounters":{},"tasks":{},"rng":{"world":{"state":"unchanged"}}}
	for id in ["buyer","merchant"]:
		state.actors[id]={"id":id,"alive":true,"available":true,"action_id":"","wallet_id":"wallet."+id,"inventory_id":"bag."+id,"location":{"kind":"Settlement","settlement_id":"city","place_id":"shop"}}
		state.wallets["wallet."+id]={"id":"wallet."+id,"owner":{"kind":"actor","id":id},"balance":10000,"reserved":{}}
		state.inventories["bag."+id]={"id":"bag."+id,"owner":{"kind":"actor","id":id},"capacity":1000,"revision":4,"lots":[]}
	state.actors.merchant.prototype_id="merchant.prototype"; state.actors.merchant.display_name="行商新名"; state.actors.merchant.appearance=false
	var data: Dictionary={"actors":{"buyer":{"id":"buyer","name_key":"buyer.name"},"merchant.prototype":{"id":"merchant.prototype","name_key":"merchant.name"}},"texts":{"buyer.name":"买主","merchant.name":"原型姓名","shop.name":"粮行","grain.name":"粮食","salt.name":"盐","empty.name":"空货","blade.name":"刀"},"places":{"shop":{"id":"shop","name_key":"shop.name","settlement_id":"city"}},"roles":{"role.shop":{"id":"role.shop","place_id":"shop","services":["trade"]}},"shops":{"shop.grain":{"id":"shop.grain","place_id":"shop","representative_roles":["role.shop"],"inventory_id":"bag.merchant","wallet_id":"wallet.merchant"}},"items":{},"offers":{},"required_capabilities":["economy"]}
	for row in [["z.empty","empty.name","commodity"],["m.grain","grain.name","commodity"],["a.salt","salt.name","commodity"],["b.blade","blade.name","weapon"]]: data.items[row[0]]={"id":row[0],"name_key":row[1],"kind":row[2],"base_price":40,"weight":1}
	for row in [["own","m.grain",7,{"kind":"actor","id":"merchant"},""],["reserved","m.grain",50,{"kind":"actor","id":"merchant"},"held"],["foreign","m.grain",90,{"kind":"actor","id":"buyer"},""],["salt","a.salt",2,{"kind":"actor","id":"merchant"},""]]: state.inventories["bag.merchant"].lots.append({"id":row[0],"item_id":row[1],"quantity":row[2],"owner":row[3],"reserved_by":row[4]})
	state.inventories["bag.buyer"].lots=[{"id":"bag-grain","item_id":"m.grain","quantity":10,"owner":{"kind":"actor","id":"buyer"},"reserved_by":""}]
	for row in [["z.ready","buyer","ready","procurement"],["a.active","buyer","active","procurement"],["foreign","merchant","active","procurement"],["done","buyer","success","procurement"],["message","buyer","active","message"]]:
		var id: String=row[0]; var offer: String="offer."+id; var wallet: String="fund."+id
		state.tasks[id]={"id":id,"assignee":row[1],"status":row[2],"offer_id":offer,"budget_wallet_id":wallet}
		data.offers[offer]={"id":offer,"name_key":offer,"template":row[3],"goal":{"item_id":"m.grain"}}
		data.texts[offer]="委托"+id
		state.wallets[wallet]={"id":wallet,"owner":{"kind":"task","id":id},"balance":1000,"reserved":{}}
	var context: Dictionary={"place_id":"shop","provider_actor_id":"merchant","role_id":"role.shop"}
	return {"state":state,"data":data,"view":{"phase":"trade","command":{"shop_id":"shop.grain"},"context":context},"request":{"shop_id":"shop.grain","item_id":"m.grain","quantity":2,"side":"buy","payer_wallet_id":"wallet.buyer","context":context.duplicate(true)}}

## 真实Main离树；仅安装本用途有限状态和完整受信命令注册，不调用new_game或伪造preview。
func host_for(f: Dictionary):
	var main=Main.new()
	for node in [main.storage,main.journey_view,main._ui_font_scale,main._ambience]: main.add_child(node)
	main._ambience.add_child(main._ambience.music); main._ambience.add_child(main._ambience.effect)
	main.kernel=CountedKernel.new(); main.kernel._state=f.state; main.kernel._catalog={"data":f.data}; main.kernel._registry=registrations
	main._data=f.data
	return main

## 控件元数据和实际选择一起冻结，不能只比较可见文本而漏掉付款身份。
func options(control: OptionButton) -> Dictionary:
	var rows: Array=[]
	for index in range(control.item_count): rows.append({"text":control.get_item_text(index),"metadata":control.get_item_metadata(index)})
	return {"rows":rows,"selected":control.selected}

## 从真实Label读取既有收据/标题/报价，不复制生产排版和文案算法。
func texts(node: Node) -> Array:
	var result: Array=[]
	if node is Label: result.append({"name":node.name,"text":node.text})
	for child in node.get_children(): result.append_array(texts(child))
	return result

## 摘要只绑定实际返回值，不把快照次数混进原菜单语义。
func digest(value: Variant) -> String: return JSON.stringify(value,"",true,true).sha256_text()

## 同一货单、不同付款及已交割收据均从实际Main初始化，最后仍由原preview决定可否成交。
func menu_case(kind: String, capturing: bool) -> Dictionary:
	var f: Dictionary=fixture(); var main=host_for(f)
	if kind in ["selected","pending","completed","public_receipt","sell_receipt","changed_scope","changed_actor"]:
		main._trade_actor_id="buyer"; main._trade_request=f.request.duplicate(true)
		main._trade_request.side="sell"; main._trade_request.quantity=2; main._trade_request.payer_wallet_id="wallet.buyer"
	if kind in ["pending","completed","public_receipt","sell_receipt","changed_scope","changed_actor"]:
		var request: Dictionary=f.request.duplicate(true)
		if kind=="public_receipt": request.payer_wallet_id="fund.z.ready"
		if kind=="sell_receipt": request.side="sell"
		var quote: Dictionary=main.kernel.preview("trade",request)
		check(quote.ok,"收据内容来自真实旧报价 "+kind)
		main._trade_receipt={"action_id":"trade.recent","quote":quote}
	if kind=="pending": f.state.actions["trade.recent"]={"id":"trade.recent","kind":"trade"}; f.state.actors.buyer.action_id="trade.recent"
	if kind=="completed": f.state.wallets["wallet.buyer"].balance=0; main._trade_request.side="buy"
	if kind=="changed_scope": main._trade_request.shop_id="previous.shop"
	if kind=="changed_actor": main._trade_actor_id="previous.actor"
	if kind=="zero_stock": f.state.inventories["bag.merchant"].lots=[]
	if kind=="empty_items": f.data.items={}
	var before: Dictionary=f.duplicate(true); var box=VBoxContainer.new(); root.add_child(box)
	main.kernel.snapshot_calls=0; seed(612); var next_random: int=randi(); seed(612)
	main._trade(box,f.view)
	var used: int=main.kernel.snapshot_calls
	snapshot_counts[kind]=used
	check(used==(2 if capturing else 0),"货单专用snapshot "+kind+"："+str(used))
	check(f==before and main.kernel._state==before.state and randi()==next_random and not main.is_inside_tree() and not main.storage._busy,"真实货单查询不改输入/RNG或启动Main "+kind)
	var labels: Array=[]
	for row in texts(box): labels.append(row.text)
	var output: Dictionary={"labels":labels,"items":options(main._item),"payers":options(main._payer),"side":main._side.selected,"quantity":int(main._quantity.value),"request":main._trade_request.duplicate(true),"receipt":main._trade_receipt.duplicate(true),"trade_disabled":main._trade_button.disabled}
	if kind=="initial":
		check(output.items.rows==[{"text":"空货 · 店存 0","metadata":"z.empty"},{"text":"粮食 · 店存 7","metadata":"m.grain"},{"text":"盐 · 店存 2","metadata":"a.salt"}] and output.items.selected==1 and output.quantity==7,"原顺序/零库存/首个有货商品及默认数量保持")
		check(output.payers.rows.size()==3 and output.payers.rows[1].metadata=="fund.z.ready" and output.payers.rows[2].metadata=="fund.a.active","个人优先、本人采购active/ready保持原顺序")
		check(output.labels[0].contains("行商新名"),"动态姓名不受坏appearance影响")
	box.free(); main.free()
	return output

## 真实Kernel仍沿原首错；菜单查询不能把这些失败改成新的选择过滤或先行授权。
func quote_errors() -> Dictionary:
	var outputs: Dictionary={}
	for kind in ["shape","person","representative","item","wallet","public","stale","stock","funds","capacity"]:
		var f: Dictionary=fixture(); var main=host_for(f); var request: Dictionary=f.request.duplicate(true)
		match kind:
			"shape": request.quantity=0; f.state.actors.merchant.location.place_id="away"
			"person": f.state.actors.merchant.location.place_id="away"; request.payer_wallet_id="missing"
			"representative": f.data.shops["shop.grain"].representative_roles=[]; request.payer_wallet_id="missing"
			"item": request.item_id="b.blade"; request.payer_wallet_id="missing"
			"wallet": request.payer_wallet_id="missing"; f.state.inventories["bag.merchant"].lots=[]
			"public": request.payer_wallet_id="fund.z.ready"; request.item_id="a.salt"; request.quoted_total=-1
			"stale": request.quoted_total=-1; request.market_revision=-1; f.state.inventories["bag.merchant"].lots=[]; f.state.wallets["wallet.buyer"].balance=0
			"stock": f.state.inventories["bag.merchant"].lots=[]; f.state.wallets["wallet.buyer"].balance=0
			"funds": f.state.wallets["wallet.buyer"].balance=0; f.state.inventories["bag.buyer"].capacity=0
			"capacity": f.state.inventories["bag.buyer"].capacity=0
		var before: Dictionary=f.duplicate(true)
		outputs[kind]=main.kernel.preview("trade",request)
		var expected: String={"shape":"InvalidTrade","person":"PersonAbsent","representative":"WrongShopRepresentative","item":"NotTradeable","wallet":"UnknownWallet","public":"PublicGoodsRestricted","stale":"StaleQuote","stock":"InsufficientStock","funds":"InsufficientFunds","capacity":"InsufficientCapacity"}[kind]
		check(outputs[kind].get("code")==expected and f==before,"旧报价原首错和只读 "+kind)
		main.free()
	return outputs

## 有限字段与深副本隔离；姓名查询没有外观档案前置，公款选项不按当前商品或余额抢先过滤。
func query_contract(leaf) -> void:
	var f: Dictionary=fixture(); var before: Dictionary=f.duplicate(true)
	var view: Dictionary=leaf.query(f.state,f.data,"shop.grain","merchant","")
	var keys: Array=view.keys(); keys.sort()
	check(keys==["actor_id","items","personal_wallet_id","provider_display_name","receipt_action_pending","task_payers"],"有限六字段，不含state/私密人物/钱包余额")
	check(view.actor_id=="buyer" and view.personal_wallet_id=="wallet.buyer" and view.provider_display_name=="行商新名" and not view.receipt_action_pending,"原身份、姓名与收据不存在状态")
	check(view.items==[{"item_id":"z.empty","stock":0},{"item_id":"m.grain","stock":7},{"item_id":"a.salt","stock":2}] and view.task_payers==[{"offer_id":"offer.z.ready","wallet_id":"fund.z.ready"},{"offer_id":"offer.a.active","wallet_id":"fund.a.active"}],"原商品/付款筛选保持完整行与顺序")
	view.items[0].stock=999; view.task_payers[0].wallet_id="forged"
	check(f==before and leaf.query(f.state,f.data,"shop.grain","merchant","").items[0].stock==0,"输出行隔离原件及下次查询")
	f.state.wallets["fund.z.ready"].balance=0
	check(leaf.query(f.state,f.data,"shop.grain","merchant","").task_payers.size()==2,"零余额公款不被菜单提前隐藏")
	var main=host_for(f)
	check(main.kernel.has_method("trade_view"),"正式Kernel货单门面存在")
	if main.kernel.has_method("trade_view"):
		main.kernel.snapshot_calls=0
		check(main.kernel.call("trade_view","shop.grain","merchant","")==leaf.query(f.state,f.data,"shop.grain","merchant","") and main.kernel.snapshot_calls==0,"正式Kernel直接有限查询不取snapshot")
	main.free()

## 显式提供既有正式目录时，只补真实new_game后的有限查询，不重复采购/议价/存读矩阵。
func formal_query(path: String, scenario: String) -> void:
	var kernel=CountedKernel.new()
	var opened: Dictionary=kernel.new_game(scenario,path)
	if not check(opened.ok,"现有正式目录new_game相邻门 "+str(opened)): return
	if not check(kernel.has_method("trade_view"),"正式新世界货单门面存在"): return
	var state: Dictionary=kernel.snapshot(); var data: Dictionary=kernel.content().data
	if not check(not data.shops.is_empty(),"正式内容有真实商铺"): return
	var shop: Dictionary=data.shops.values()[0]
	var provider: String=state.roles[shop.representative_roles[0]].actor_id
	kernel.snapshot_calls=0
	var value: Dictionary=kernel.call("trade_view",shop.id,provider,"")
	check(value.actor_id==state.player_actor_id and value.personal_wallet_id==state.actors[state.player_actor_id].wallet_id and not value.receipt_action_pending,"有限结果来自本局真实人物和钱包")
	var expected: Dictionary=value.duplicate(true)
	if not value.items.is_empty(): value.items[0].stock=999999
	value.task_payers.append({"offer_id":"forged","wallet_id":"forged"})
	check(kernel.call("trade_view",shop.id,provider,"")==expected and kernel.snapshot_calls==0,"正式门面不取snapshot且返回行隔离")
	check(kernel._state==state and kernel._catalog.data==data,"正式查询和返回值修改不改已验世界/目录")

## 旧capture仅在改生产前运行一次；正常门不更新冻结摘要或接受新实现重录期望。
func _run() -> void:
	registrations=Registry.build(Builtin.descriptors())
	if not check(registrations.get("ok",false),"完整受信注册表用于真实Kernel报价 "+str(registrations.get("code",""))): quit(1); return
	var capture: String=""
	var formal_path: String=""; var formal_scenario: String=""
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--capture-oracle="): capture=arg.trim_prefix("--capture-oracle=")
		elif arg.begins_with("--formal-catalog="): formal_path=arg.trim_prefix("--formal-catalog=")
		elif arg.begins_with("--formal-scenario="): formal_scenario=arg.trim_prefix("--formal-scenario=")
	var outputs: Dictionary={}
	for kind in ["initial","selected","pending","completed","public_receipt","sell_receipt","changed_scope","changed_actor","zero_stock","empty_items"]: outputs[kind]=menu_case(kind,not capture.is_empty())
	outputs.quote_errors=quote_errors()
	if not capture.is_empty():
		var file=FileAccess.open(capture,FileAccess.WRITE); file.store_string(JSON.stringify({"outputs":outputs,"sha256":digest(outputs)},"\t",true,true)+"\n"); file.close(); print("TRADE_SHEET_ORACLE=",digest(outputs))
	else:
		check(digest(outputs)==ORACLE_SHA,"15246真实旧菜单/元数据/选择/收据及quote首错逐值保持")
		var exists: bool=FileAccess.file_exists(VIEW_PATH); check(exists,"有限货单叶存在")
		if exists: query_contract(load(VIEW_PATH))
		if not formal_path.is_empty(): formal_query(formal_path,formal_scenario)
	registrations={}
	print("TRADE_SHEET_SNAPSHOT_COUNTS=",JSON.stringify(snapshot_counts))
	print("TRADE_SHEET_VIEW_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
