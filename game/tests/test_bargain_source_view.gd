## 真实货单入口来源门：Main报价刷新、议价控件、Session和Kernel报价保持；不重跑成交/存档矩阵。
extends SceneTree
const Quotes=preload("res://core/domains/economy/bargain_quote.gd")
const Registry=preload("res://core/runtime/module_registry.gd")
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const Strict=preload("res://core/strict_json.gd")
const ORACLE_SHA="bf59ef838ad924a541089498eb15dcf26804e85801fd9fb2ba4e603b3a2e2b1c"
var checks: int=0
var failures: int=0
var registrations: Dictionary={}
var snapshot_counts: Dictionary={}

## 只停用Main启动资源和存储准备；报价刷新、身份、文本、按钮及所有Session方法仍为原实现。
class InputHost extends "res://ui/main.gd":
	## 专用输入已给定，不启动另一个世界、音频或Native存储。
	func _ready() -> void: pass

## 观察实际快照、预览、题面查询和提交次数，不替换原规则返回值。
class ObservedKernel extends "res://core/world_kernel.gd":
	var snapshots: int=0
	var events: Array=[]
	var submissions: int=0
	## 保留真实深复制，只计数当前挂载用途。
	func snapshot() -> Dictionary:
		snapshots+=1; return super.snapshot()
	## 原输入和完整报价进入oracle，未知/旧错误不会被测试改成成功。
	func preview(kind: String, params: Dictionary={}, actor_id: String="") -> Dictionary:
		var result: Dictionary=super.preview(kind,params,actor_id)
		events.append({"call":"preview","kind":kind,"params":params.duplicate(true),"actor_id":actor_id,"result":result.duplicate(true)})
		return result
	## 同一原有限题面在旧两个时点读取，不能合并或移到按钮预览之前。
	func trade_bargain(actor_id: String, quote_id: String) -> Dictionary:
		var result: Dictionary=super.trade_bargain(actor_id,quote_id)
		events.append({"call":"trade_bargain","actor_id":actor_id,"quote_id":quote_id,"result":result.duplicate(true)})
		return result
	## 旧页面guard应在此之前拒绝；若误到提交则仍运行原事务而非伪造成功。
	func submit(command: Dictionary) -> Dictionary:
		submissions+=1; return super.submit(command)

## 引擎就绪后执行短用途门。
func _init() -> void: call_deferred("_run")

## 每项真实失败计入非零退出码；同时审阅日志中的脚本错误。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 沿既有test_trade_bargain的两钱包/两库存/真实岗位和公开条件夹具，补Kernel/Session必要身份。
func fixture() -> Dictionary:
	var state: Dictionary={"tick":0,"revision":4,"player_actor_id":"buyer","request_sequences":{"buyer":0,"merchant":0},"next_instance_id":3,"actors":{},"roles":{"role.shop":{"actor_id":"merchant","enabled":true}},"wallets":{},"inventories":{},"actions":{},"facts":{},"battles":{},"interactions":{},"encounters":{},"tasks":{}}
	for id in ["buyer","merchant"]:
		state.actors[id]={"id":id,"alive":true,"available":true,"hp":100,"action_id":"","skills":{"commerce":80},"attributes":{"administration":60},"wallet_id":"wallet."+id,"inventory_id":"bag."+id,"location":{"kind":"Settlement","settlement_id":"city","place_id":"shop"}}
		state.wallets["wallet."+id]={"id":"wallet."+id,"owner":{"kind":"actor","id":id},"balance":10000,"reserved":{}}
		state.inventories["bag."+id]={"id":"bag."+id,"owner":{"kind":"actor","id":id},"capacity":1000,"revision":0,"lots":[]}
	state.inventories["bag.merchant"].lots=[{"id":"lot:1","item_id":"grain","quantity":100,"owner":{"kind":"actor","id":"merchant"},"reserved_by":""}]
	var definition: Dictionary=Strict.parse_text(FileAccess.get_file_as_string("res://../tests/fixtures/bargain_challenge/definition.json")).value
	var sources: Array=[{"condition_id":"small","source":"stock_covers_quantity"},{"condition_id":"batch","source":"quantity_at_most","value":5},{"condition_id":"cash","source":"cash_payment"},{"condition_id":"unmatched","source":"quantity_at_most","value":5}]
	var data: Dictionary={"required_capabilities":["economy"],"places":{"shop":{"settlement_id":"city"}},"roles":{"role.shop":{"place_id":"shop","services":["trade"]}},"shops":{"shop.grain":{"id":"shop.grain","place_id":"shop","representative_roles":["role.shop"],"inventory_id":"bag.merchant","wallet_id":"wallet.merchant","bargaining":{"challenge_id":definition.id,"condition_sources":sources}}},"items":{"grain":{"id":"grain","kind":"commodity","base_price":40,"weight":1}},"offers":{},"bargain_challenges":{definition.id:definition},"texts":Strict.parse_text(FileAccess.get_file_as_string("res://../tests/fixtures/bargain_challenge/texts.json")).value}
	var request: Dictionary={"shop_id":"shop.grain","item_id":"grain","quantity":10,"side":"buy","payer_wallet_id":"wallet.buyer","context":{"place_id":"shop","provider_actor_id":"merchant","role_id":"role.shop"}}
	var random=RandomNumberGenerator.new(); random.seed=6480
	state.rng={"minigame":{"seed":str(random.seed),"state":str(random.state)}}
	return {"state":state,"data":data,"request":request}

## 真实owner创建冻结实例，只作为显示输入准备；不伪造种子、分数或正式成交。
func open_quote(f: Dictionary, mode: String) -> String:
	var params: Dictionary={"trade":f.request,"mode":mode}
	var offer: Dictionary=Quotes.preview_open(f.state,f.data,"buyer",params)
	if not check(offer.ok,"原无折扣真实报价用于准备 "+mode): return ""
	var opened: Dictionary=Quotes.open(f.state,f.data,"buyer",params,offer)
	if not check(opened.ok and Quotes.validate_state(f.state,f.data).ok,"原owner建立自洽冻结实例"): return ""
	return opened.details.quote_id

## 只装入货单输入控件，随后调用真正Main._update_quote；不复制其议价调用条件或报价算法。
func host_for(f: Dictionary):
	var main=InputHost.new(); main.kernel=ObservedKernel.new(); main.kernel._state=f.state; main.kernel._catalog={"data":f.data}; main.kernel._registry=registrations
	main._data=f.data; main._started=true; main._scene_counter=1; root.add_child(main)
	main._item=OptionButton.new(); main._item.add_item("粮食"); main._item.set_item_metadata(0,"grain"); main.add_child(main._item)
	main._side=OptionButton.new(); main._side.add_item("买入"); main._side.add_item("卖出"); main.add_child(main._side)
	main._payer=OptionButton.new(); main._payer.add_item("私人钱包"); main._payer.set_item_metadata(0,"wallet.buyer"); main.add_child(main._payer)
	main._quantity=SpinBox.new(); main._quantity.min_value=1; main._quantity.max_value=500; main._quantity.value=f.request.quantity; main.add_child(main._quantity)
	main._trade_button=Button.new(); main.add_child(main._trade_button)
	main._quote_label=Label.new(); main.add_child(main._quote_label)
	main._bargain_box=VBoxContainer.new(); main.add_child(main._bargain_box)
	main._trade_request=f.request.duplicate(true)
	return main

## 释放未启动的Main构造节点，避免把隔离音频/存储对象泄漏当作门成功。
func release(main) -> void:
	for node in [main.storage,main.journey_view,main._ui_font_scale,main._ambience.music,main._ambience.effect,main._ambience]: node.free()
	main.free()

## 记录真实文案与每个ticket的参数/完整报价，自动节点ID和Kernel实例号不进入语义oracle。
func controls(node: Node) -> Array:
	var output: Array=[]
	if node is Label: output.append({"text":node.text})
	if node is Button: output.append({"button":node.text,"kind":node.get_meta("command_kind",""),"target":node.get_meta("target_id",""),"disabled":node.disabled,"tooltip":node.tooltip_text,"params":node.get_meta("activity_params",{}),"quote":node.get_meta("activity_quote",{})})
	if node.has_meta("bargain_quote_id"): output.append({"quote_id":node.get_meta("bargain_quote_id")})
	if node.has_meta("activity_scope"): output.append({"scope":node.get_meta("activity_scope"),"source_id":node.get_meta("activity_source_id")})
	for child in node.get_children(): output.append_array(controls(child))
	return output

## 完整输出绑定旧实现，不在普通运行接受新期望。
func digest(value: Variant) -> String: return JSON.stringify(value,"",true,true).sha256_text()

## 实例、失败和重复分支都由原Main挂载；人为坏事实只测原查找/早退边界，不称合法世界。
func menu_case(kind: String, capturing: bool) -> Dictionary:
	var f: Dictionary=fixture(); var quote_id: String=""
	if kind not in ["new","not_offered","advancing"]: quote_id=open_quote(f,"automatic" if kind in ["completed","consumed"] else "manual")
	if kind=="consumed":
		var receipt: Dictionary=Quotes.quoted_trade(f.state,f.data,"buyer",{"bargain_quote_id":quote_id})
		check(receipt.ok,"显示用历史回执来自原正式折扣报价")
		f.state.facts[quote_id].consumed_action_id="action:9"; f.state.facts[quote_id].receipt=receipt
		check(Quotes.validate_state(f.state,f.data).ok,"历史回执显示输入满足原议价事实结构；未宣称执行成交")
	if kind in ["duplicate","duplicate_funds"]: f.state.facts["quote:99"]=f.state.facts[quote_id].duplicate(true)
	if kind=="duplicate_funds": f.state.wallets["wallet.buyer"].balance=0
	if kind=="foreign": f.state.facts[quote_id].actor_id="merchant"
	if kind=="changed_quantity": f.request.quantity=9
	if kind=="stale_tick": f.state.tick=1
	if kind=="stale_market": f.state.inventories["bag.merchant"].revision+=1
	if kind=="bad_fact": f.state.facts[quote_id].seed_origin.state="invalid"
	if kind=="not_offered": f.data.shops["shop.grain"].bargaining={}
	var main=host_for(f)
	if kind=="advancing": main._advancing=true
	var before: Dictionary=f.duplicate(true); seed(967); var next_random: int=randi(); seed(967)
	main._update_quote()
	snapshot_counts[kind]=main.kernel.snapshots
	var has_instance: bool=kind in ["active","completed","consumed","bad_fact"]
	var expected: int=(4 if has_instance else 2) if capturing else (2 if has_instance else 1)
	check(main.kernel.snapshots==expected,"议价挂载实际snapshot "+kind+"="+str(main.kernel.snapshots))
	check(f==before and main.kernel.submissions==0 and randi()==next_random,"挂载不改钱货/历史/RNG且无提交 "+kind)
	var output: Dictionary={"controls":controls(main._bargain_box),"events":main.kernel.events.duplicate(true),"trade_request":main._trade_request.duplicate(true),"trade_label":main._quote_label.text,"trade_disabled":main._trade_button.disabled}
	if kind=="duplicate_funds": check(output.events.size()==2 and output.events[1].result.code=="InsufficientFunds","原manual preview先于重复事实说明")
	if kind=="new":
		var box: Control=main._bargain_box.get_child(0); var session=box.get_meta("activity_session")
		main._scene_counter+=1
		check(session.activate(1).code=="StaleActivitySession" and main.kernel.submissions==0 and f==before,"真实旧页面guard拒绝迟到ticket，不刷新页面后提交")
	release(main)
	return output

## 有限源口只执行原三条件匹配；原插入序第二项即早退，不遍历坏尾项或偷做_owned校验。
func source_contract() -> void:
	var state: Dictionary={"facts":{}}
	check(Quotes.new().call("find_for_basis",state,"buyer","basis")=={"quote_id":"","duplicate":false},"无事实时只返回未找到的两个标量")
	state.facts={"other-kind":{"kind":"training","actor_id":"buyer","basis_sha256":"basis"},"other-person":{"kind":"trade_bargain","actor_id":"merchant","basis_sha256":"basis"},"other-basis":{"kind":"trade_bargain","actor_id":"buyer","basis_sha256":"different"},"quote:90":{"kind":"trade_bargain","actor_id":"buyer","basis_sha256":"basis"}}
	var before: Dictionary=state.duplicate(true)
	var found: Dictionary=Quotes.new().call("find_for_basis",state,"buyer","basis")
	var keys: Array=found.keys(); keys.sort()
	check(keys==["duplicate","quote_id"] and found=={"quote_id":"quote:90","duplicate":false},"过滤其它kind/人物/basis，不要求完整quote就返回唯一原ID")
	found.quote_id="forged"; found["duplicate"]=true
	check(state==before and Quotes.new().call("find_for_basis",state,"buyer","basis").quote_id=="quote:90","两个返回标量不会暴露或改变事实引用")
	state.facts["quote:1"]=state.facts["quote:90"].duplicate(true); state.facts["aaa.invalid.tail"]=false
	check(Quotes.new().call("find_for_basis",state,"buyer","basis")=={"quote_id":"","duplicate":true},"原插入第二匹配即返回，未重排或扫描坏尾项")
	var f: Dictionary=fixture(); var main=host_for(f)
	main.kernel._state.facts=before.facts.duplicate(true)
	var expected: Dictionary=main.kernel._state.duplicate(true)
	seed(289); var next_random: int=randi(); seed(289)
	check(main.kernel.call("trade_bargain_source","buyer","basis")=={"quote_id":"quote:90","duplicate":false},"真实Kernel薄口返回原经济owner匹配")
	check(main.kernel.snapshots==0 and main.kernel.events==[] and main.kernel.submissions==0 and main.kernel._state==expected and randi()==next_random,"来源查询不取snapshot、不报价、不写状态或消费RNG")
	release(main)

## 已有小夹具支持旧输出冻结；正常运行绑定同一摘要和新有限来源API。
func _run() -> void:
	registrations=Registry.build(Builtin.descriptors())
	if not check(registrations.get("ok",false),"完整受信注册表用于真实Kernel报价"): quit(1); return
	var capture: String=""
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--capture-oracle="): capture=arg.trim_prefix("--capture-oracle=")
	var outputs: Dictionary={}
	for kind in ["new","active","completed","consumed","duplicate","duplicate_funds","foreign","changed_quantity","stale_tick","stale_market","bad_fact","not_offered","advancing"]: outputs[kind]=menu_case(kind,not capture.is_empty())
	if not capture.is_empty():
		var file=FileAccess.open(capture,FileAccess.WRITE); file.store_string(JSON.stringify({"outputs":outputs,"sha256":digest(outputs),"snapshots":snapshot_counts},"\t",true,true)+"\n"); file.close(); print("BARGAIN_SOURCE_ORACLE=",digest(outputs))
	else:
		check(digest(outputs)==ORACLE_SHA,"原Main议价文字/按钮ticket/完整真实报价与题面时序保持")
		check(Quotes.new().has_method("find_for_basis"),"既有经济owner提供有限实例匹配")
		var main=host_for(fixture()); var has_api: bool=main.kernel.has_method("trade_bargain_source")
		check(has_api,"Kernel有限来源门面存在"); release(main)
		if has_api and Quotes.new().has_method("find_for_basis"): source_contract()
	await process_frame
	print("BARGAIN_SOURCE_SNAPSHOTS=",JSON.stringify(snapshot_counts,"",true))
	print("BARGAIN_SOURCE_VIEW_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
