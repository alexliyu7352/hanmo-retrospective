## M1首个只读资产片：实际采购来源、原查询对照、精确产权和批次隔离；不宣称原生或迁移完成。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Schema=preload("res://core/state_schema.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const LEAF="res://core/backends/legacy_asset_projection.gd"
const PLAYER="actor.player"
const MAXIMUM: int=2147483647
const SOURCE_FIELDS=["ruleset_id","content_id","content_version","content_digest","engine_build","rng_backend","scenario_id","tick","revision"]
var AssetProjection
var checks: int=0
var failures: int=0
var examples: Array=[]
var purchase: Dictionary={}
var reporting: Dictionary={}
var zero_reserved: Dictionary={}

## 先形成可达来源反例，再延迟查新叶，缺实现也能以明确断言结束而非脚本异常。
func _init() -> void: call_deferred("run")

## 保留每个局部判断与真实失败退出码。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 正式目录开局，不给世界补钱、物品、事实或占用。
func fresh(zero: bool=false):
	var kernel=Kernel.new()
	var made: Dictionary=kernel.new_game("scenario.procurement","res://data/generated/zero_budget.hmcatalog.json" if zero else "res://data/generated/procurement.hmcatalog.json")
	if not check(made.ok,"实际tiny采购目录初始化："+str(made)): return null
	return kernel

## 所有游戏变化使用原公共请求字段，不改变生产实现或跳过校验。
func send(kernel,kind: String,params: Dictionary={}) -> Dictionary:
	var state: Dictionary=kernel.snapshot()
	return kernel.submit({"kind":kind,"params":params,"actor_id":PLAYER,"request_seq":state.request_sequences[PLAYER]+1,"expected_revision":state.revision})

## 仅完成已经真实创建的行动，半日与消费由同一个Kernel推进。
func finish_action(kernel) -> bool:
	for unused in range(4):
		if kernel.snapshot().actors[PLAYER].action_id.is_empty(): return true
		var result: Dictionary=send(kernel,"advance")
		if not check(result.ok,"真实行动完成半日："+str(result)): return false
	return check(false,"局部来源行动应已完成")

## 场所移动仍走真实通路，不能传送到办事人身边。
func move_to(kernel,place: String) -> bool:
	var result: Dictionary=send(kernel,"move_local",{"place_id":place})
	return check(result.ok,"实际移动到"+place+"："+str(result)) and finish_action(kernel)

## 当面岗位引用只来自既有查询。
func context(kernel,role: String) -> Dictionary:
	return Interaction.for_role(kernel.snapshot(),kernel.content().data,role)

## 保存的是同一真实Kernel一次返回的完整三元组，资产片不能拿简化字典自证合法。
func capture(kernel,label: String) -> Dictionary:
	var bundle: Dictionary=kernel.save_context()
	if check(Schema.validate(bundle.state,bundle.catalog,bundle.registry).ok,"实际来源完整旧Schema通过："+label): examples.append({"label":label,"bundle":bundle})
	return bundle

## 实际受托、公款买13份、私款买2份、交接10份，显现私人容器／任务货／拆批预留的差异。
func reachable_sources() -> bool:
	var kernel=fresh()
	if kernel==null: return false
	capture(kernel,"空私人包及多个原钱包")
	if not move_to(kernel,"west.office"): return false
	if not check(send(kernel,"inspect",{"prop_id":"notice.grain"}).ok,"读取真实告示取得委托知识"): return false
	var accepted: Dictionary=send(kernel,"accept_offer",{"offer_id":"offer.grain","context":context(kernel,"role.office")})
	if not check(accepted.ok,"本人实际受托："+str(accepted)): return false
	capture(kernel,"预算尚预留而未交付")
	if not finish_action(kernel): return false
	var state: Dictionary=kernel.snapshot(); var task: Dictionary=state.tasks.values()[0]
	if not move_to(kernel,"west.street") or not move_to(kernel,"west.grain"): return false
	var request: Dictionary={"shop_id":"shop.grain","item_id":"item.grain","quantity":13,"side":"buy","payer_wallet_id":task.budget_wallet_id,"context":context(kernel,"role.merchant")}
	var traded: Dictionary=send(kernel,"trade",request)
	if not check(traded.ok,"使用真实任务预算买13份："+str(traded)): return false
	purchase=capture(kernel,"交易start已转钱货但action未完成")
	check(purchase.state.actions[purchase.state.actors[PLAYER].action_id].kind=="trade" and purchase.state.wallets[task.budget_wallet_id].balance<650,"待办交易不代表尚未扣款")
	if not finish_action(kernel): return false
	request.payer_wallet_id=kernel.snapshot().actors[PLAYER].wallet_id; request.quantity=2
	if not check(send(kernel,"trade",request).ok,"另外以本人钱包买2份") or not finish_action(kernel): return false
	var mixed: Dictionary=capture(kernel,"同包任务13份与私人2份")
	var bag: Dictionary=mixed.state.inventories[mixed.state.actors[PLAYER].inventory_id]
	var owner: Dictionary={"kind":"actor","id":PLAYER}; var task_owner: Dictionary={"kind":"task","id":task.id}
	var carried: int=0
	for lot in bag.lots: carried+=lot.quantity
	check(carried==15 and Inventory.count(bag,"item.grain",[owner])==2 and Inventory.count(bag,"item.grain",[task_owner])==13,"反例：按容器产权把15份归私有会吞掉13份任务公物")
	print("REACHABLE_OWNER_COUNTEREXAMPLE carried=",carried," private=",Inventory.count(bag,"item.grain",[owner])," entrusted=",Inventory.count(bag,"item.grain",[task_owner]))
	if not move_to(kernel,"west.street") or not move_to(kernel,"west.office"): return false
	var reported: Dictionary=send(kernel,"report_offer",{"offer_id":"offer.grain","context":context(kernel,"role.office")})
	if not check(reported.ok,"真实交接预留10份："+str(reported)): return false
	reporting=capture(kernel,"真实部分批次拆分与报酬预留")
	bag=reporting.state.inventories[reporting.state.actors[PLAYER].inventory_id]
	var action_id: String=reporting.state.actors[PLAYER].action_id
	check(bag.lots.size()==3 and bag.lots[0].quantity==3 and bag.lots[1].quantity==2 and bag.lots[2].quantity==10,"真实拆批保留原剩余批、私人批并追加预留批")
	check(Inventory.count(bag,"item.grain",[task_owner])==3 and Inventory.count(bag,"item.grain",[task_owner],action_id)==10 and Inventory.count(bag,"item.grain",[owner])==2,"未预留／预留／私人量彼此分离")
	if not finish_action(kernel): return false
	capture(kernel,"交接后原任务实例和剩余批次")
	var zero=fresh(true)
	if zero==null: return false
	if not move_to(zero,"west.office") or not check(send(zero,"inspect",{"prop_id":"notice.grain"}).ok,"零预算作者仍须真实获知来源"): return false
	if not check(send(zero,"accept_offer",{"offer_id":"offer.grain","context":context(zero,"role.office")}).ok,"零预算作者正常开始受托"): return false
	zero_reserved=capture(zero,"作者预算0由真实行动写入零reservation")
	var zero_action: String=zero_reserved.state.actors[PLAYER].action_id
	check(zero_reserved.state.wallets["wallet.office"].reserved.has(zero_action) and zero_reserved.state.wallets["wallet.office"].reserved[zero_action]==0,"真实零项不能因为不影响可花金额而丢失")
	return true

## 将已投影行仅按ID建查询索引；不改任何批次或owner字段。
func indexed(rows: Array) -> Dictionary:
	var result: Dictionary={}
	for row in rows: result[row.id]=row
	return result

## 比较精确字段、原批次及原Inventory查询，而不是让新投影自己计算自己的期望。
func compare(bundle: Dictionary,label: String) -> Dictionary:
	var before: Dictionary=bundle.duplicate(true)
	var projected: Dictionary=AssetProjection.project(bundle.state,bundle.catalog,bundle.registry)
	if not check(projected.get("ok",false),"已验来源能投影："+label+" "+str(projected)): return {}
	var keys: Array=projected.keys(); keys.sort()
	check(keys==["actors","inventories","ok","scope","source","version","wallets"] and projected.scope=="legacy_assets_readonly" and projected.version==1,"精确只读资产范围和版本，无原生／world_id／第二余额字段")
	var source: Dictionary={}
	for key in SOURCE_FIELDS: source[key]=bundle.state[key]
	check(projected.source==source,"来源只保留原9项戳，不冒充世界唯一身份")
	for pair in [["actors",["id","wallet_id","inventory_id"]],["wallets",["id","owner","balance","reserved"]],["inventories",["id","owner","capacity","revision","lots"]]]:
		var ids: Array=bundle.state[pair[0]].keys(); ids.sort(); var actual_ids: Array=[]
		for row in projected[pair[0]]: actual_ids.append(row.id)
		check(actual_ids==ids,"外层按原ID完整排序："+pair[0]+"/"+label)
		for row in projected[pair[0]]:
			var expected: Dictionary={}
			for field in pair[1]: expected[field]=bundle.state[pair[0]][row.id][field]
			check(row==expected,"保留已声明行及完整owner/lot："+pair[0]+"/"+row.id)
	var wallets: Dictionary=indexed(projected.wallets); var inventories: Dictionary=indexed(projected.inventories)
	for id in wallets: check(Inventory.spendable(wallets[id])==Inventory.spendable(bundle.state.wallets[id]),"原spendable查询保持："+id)
	for id in inventories:
		var original: Dictionary=bundle.state.inventories[id]; var copied: Dictionary=inventories[id]
		check(Inventory.weight(copied,bundle.catalog.data)==Inventory.weight(original,bundle.catalog.data),"原承重包含所有owner/预留："+id)
		for lot in original.lots:
			check(Inventory.count(copied,lot.item_id,[lot.owner],lot.reserved_by)==Inventory.count(original,lot.item_id,[lot.owner],lot.reserved_by),"原精确owner/预留计数保持："+lot.id)
	check(bundle==before and AssetProjection.project(bundle.state,bundle.catalog,bundle.registry)==projected,"完整输入／RNG／ID／请求／事实不变且重复读一致："+label)
	return projected

## 额外键是完整旧Schema接受的纯边界，不声称通过玩法产生；完整owner精确匹配不能被归一化。
func accepted_extensions() -> void:
	var bundle: Dictionary=examples[0].bundle.duplicate(true)
	var rich_owner: Dictionary={"kind":"organization","id":"org.grain","legacy_note":{"tags":["keep-owner"]}}
	bundle.state.wallets["projection.wallet"]={"id":"projection.wallet","owner":rich_owner.duplicate(true),"balance":0,"reserved":{}}
	bundle.state.inventories["projection.store"]={"id":"projection.store","owner":rich_owner.duplicate(true),"capacity":20,"revision":0,"lots":[]}
	Inventory.add(bundle.state,"projection.store","item.grain",2,rich_owner)
	var lot: Dictionary=bundle.state.inventories["projection.store"].lots[0]
	lot.legacy_note={"nested":[{"value":"keep-lot"}]}
	if not check(Schema.validate(bundle.state,bundle.catalog,bundle.registry).ok,"带额外owner/lot字段的完整旧三元组合法，非裁剪假世界"): return
	var plain_owner: Dictionary={"kind":"organization","id":"org.grain"}
	check(Inventory.count(bundle.state.inventories["projection.store"],"item.grain",[plain_owner])==0 and Inventory.count(bundle.state.inventories["projection.store"],"item.grain",[rich_owner])==2,"删owner额外键会改变旧精确匹配，不能偷偷清洗")
	var projected: Dictionary=compare(bundle,"旧Schema额外字段边界")
	if projected.is_empty(): return
	var before: Dictionary=bundle.duplicate(true)
	var wallets: Dictionary=indexed(projected.wallets); var inventories: Dictionary=indexed(projected.inventories)
	wallets["projection.wallet"].owner.legacy_note.tags.append("changed")
	wallets["projection.wallet"].reserved["changed"]=1
	inventories["projection.store"].owner.legacy_note.tags.append("changed")
	inventories["projection.store"].lots[0].owner.legacy_note.tags.append("changed")
	inventories["projection.store"].lots[0].legacy_note.nested[0].value="changed"
	projected.actors[0].wallet_id="changed"; projected.source.revision=-1
	check(bundle==before,"深层owner/lot、reserved、actor与source返回值修改不反写任何输入")
	check(AssetProjection.project(bundle.state,bundle.catalog,bundle.registry)!=projected,"修改旧输出不污染下一次投影")

## 整数边界仅作为Schema接受的独立完整输入，不伪称游戏创造了上界钱粮。
func numeric_boundaries() -> void:
	var bundle: Dictionary=examples[0].bundle.duplicate(true)
	bundle.state.wallets["wallet.player"].balance=MAXIMUM
	Inventory.add(bundle.state,"store:org.office","item.grain",MAXIMUM,{"kind":"organization","id":"org.office"})
	if not check(Schema.validate(bundle.state,bundle.catalog,bundle.registry).ok,"原32位最大金额/数量和接收库容量是合法纯输入边界"): return
	var projected: Dictionary=compare(bundle,"64位承载原32位有效域")
	if projected.is_empty(): return
	check(indexed(projected.wallets)["wallet.player"].balance==MAXIMUM and indexed(projected.inventories)["store:org.office"].lots[0].quantity==MAXIMUM,"上界不经float或截断")
	for field in ["balance","quantity","capacity","revision"]:
		for value in [true,0.0,"1",-1,2147483648]:
			var malformed: Dictionary=examples[0].bundle.duplicate(true)
			if field=="balance": malformed.state.wallets["wallet.player"][field]=value
			elif field=="quantity": malformed.state.inventories["inventory.shop"].lots[0][field]=value
			else: malformed.state.inventories["inventory.shop"][field]=value
			var expected: Dictionary=Schema.validate(malformed.state,malformed.catalog,malformed.registry)
			var before: Dictionary=malformed.duplicate(true)
			check(not expected.ok and AssetProjection.project(malformed.state,malformed.catalog,malformed.registry)==expected and malformed==before,"原Schema整数拒绝原样返回且不补齐："+field+"/"+str(value))

## 不自行抢先解释坏三元组：原首错、缺世界、坏注册与dangling预留全部透传。
func failure_passthrough() -> void:
	var original: Dictionary=examples[0].bundle
	var variants: Array=[]
	var changed: Dictionary=original.duplicate(true); changed.registry={}; variants.append(changed)
	changed=original.duplicate(true); changed.catalog.ok=false; variants.append(changed)
	changed=original.duplicate(true); changed.state={}; variants.append(changed)
	changed=original.duplicate(true); changed.state.wallets["wallet.player"].reserved["missing-action"]=0; variants.append(changed)
	changed=original.duplicate(true); changed.state.inventories["inventory.shop"].lots[0].reserved_by="missing-action"; variants.append(changed)
	changed=reporting.duplicate(true); changed.state.wallets["wallet.office"].reserved[changed.state.actors[PLAYER].action_id]=MAXIMUM; variants.append(changed)
	for bundle in variants:
		var before: Dictionary=bundle.duplicate(true); var expected: Dictionary=Schema.validate(bundle.state,bundle.catalog,bundle.registry)
		check(not expected.ok and AssetProjection.project(bundle.state,bundle.catalog,bundle.registry)==expected and bundle==before,"完整原错误返回不包装："+str(expected))

## 本门先证真实所有权反例，再核投影；只执行本资产片，不启动UI、原生、保存编解码或迁移。
func run() -> void:
	if not check(ProjectSettings.get_setting("application/config/legacy_asset_projection_isolated",false),"仅在专门正式tiny工程运行"): finish(); return
	if not reachable_sources(): finish(); return
	if not check(FileAccess.file_exists(LEAF),"缺少正式只读资产投影叶"): finish(); return
	AssetProjection=load(LEAF)
	if not check(AssetProjection!=null and AssetProjection.has_method("project"),"真实投影入口存在"): finish(); return
	for example in examples: compare(example.bundle,example.label)
	accepted_extensions(); numeric_boundaries(); failure_passthrough()
	var same=fresh()
	if same!=null:
		var repeated: Dictionary=same.save_context()
		check(AssetProjection.project(repeated.state,repeated.catalog,repeated.registry).source==AssetProjection.project(examples[0].bundle.state,examples[0].bundle.catalog,examples[0].bundle.registry).source,"相同新局可有相同旧来源戳，不能把它当唯一world_id")
	finish()

## 汇报实际局部断言，不把资产切片称为完整NativeProjection或世界后端。
func finish() -> void:
	print("LEGACY_ASSET_PROJECTION checks=",checks," failures=",failures," scope=legacy_assets_readonly native=NOT_RUN")
	quit(0 if failures==0 else 1)
