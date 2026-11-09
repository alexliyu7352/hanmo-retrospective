## 用隔离快照验证规则回归和恶意输入边界，不直接改写运行中的世界。
extends SceneTree
const Strict=preload("res://core/strict_json.gd")
const Kernel=preload("res://core/world_kernel.gd")
const Economy=preload("res://core/economy_rules.gd")
const Codec=preload("res://core/save_codec.gd")
const Catalog=preload("res://core/content_catalog.gd")
var checks: int=0
var failures: int=0

## 在引擎初始化后运行，确保目录与资源可以正常装载。
func _init() -> void:
	call_deferred("_run")

## 逐项记录实际行为，任何期望失败都令进程返回失败。
func check(result: bool, label: String) -> void:
	checks+=1
	if not result:
		failures+=1
		printerr("FAIL: ",label)

## 通过场景共用的真实内容目录建立独立世界。
func fresh():
	var k=Kernel.new(); check(k.new_game().ok,"new game")
	return k

## 携带当前人物序号提交正常命令，允许指定NPC以核验双向占用。
func send(k, kind: String, params: Dictionary={}, actor: String="") -> Dictionary:
	var s: Dictionary=k.snapshot()
	if actor.is_empty(): actor=s.player_actor_id
	return k.submit({"kind":kind,"params":params,"actor_id":actor,"request_seq":s.request_sequences[actor]+1,"expected_revision":s.revision})

## 为测试构造真实岗位上下文；固定样本ID不进入生产规则分支。
func ctx(place: String, person: String, role: String) -> Dictionary:
	return {"place_id":place,"provider_actor_id":person,"role_id":role}

## 仅用公开游戏命令受领、采购并到达可回报状态。
func grain_ready():
	var k=fresh()
	send(k,"move_local",{"place_id":"west.office"}); send(k,"inspect",{"prop_id":"notice.grain"})
	check(send(k,"accept_offer",{"offer_id":"offer.grain","context":ctx("west.office","actor.chen","role.office")}).ok,"accept before boundary fixture")
	k.advance_one_tick()
	var task: Dictionary=k.snapshot().tasks.values()[0]
	send(k,"move_local",{"place_id":"west.street"}); send(k,"move_local",{"place_id":"west.grain"})
	check(send(k,"trade",{"shop_id":"shop.grain","item_id":"item.grain","side":"buy","quantity":10,"payer_wallet_id":task.budget_wallet_id,"context":ctx("west.grain","actor.zheng","role.merchant")}).ok,"buy before boundary fixture")
	k.advance_one_tick(); send(k,"move_local",{"place_id":"west.street"}); send(k,"move_local",{"place_id":"west.office"})
	return k

## 检验整数报价在最大折扣、买卖往返和拆单条件下保持一致。
func test_prices() -> void:
	check(Economy.floor_div(-1,100)==-1 and Economy.floor_div(-100,100)==-1,"negative floor division")
	check(Economy.total_price(40,5000,100,100,0,10,"buy",0)==473,"local reference price")
	check(Economy.total_price(25,5000,100,100,0,10,"buy",0)==293,"remote reference price")
	for stock in [10,25,50,100,200]:
		for qty in [1,5,10]:
			var ask: int=Economy.total_price(40,5000,100,stock,0,qty,"buy",800)
			var bid: int=Economy.total_price(40,5000,100,stock-qty,0,qty,"sell",800)
			check(bid<=ask,"same-boundary maximum-discount round trip")
			var split: int=Economy.total_price(40,5000,100,stock,0,1,"buy",0)+Economy.total_price(40,5000,100,stock-1,0,qty-1,"buy",0)
			check(split==Economy.total_price(40,5000,100,stock,0,qty,"buy",0),"grouping does not change same-tick marginal cost")

## 检验期限、迟到报酬、失败清算及公共物资只交接一次。
func test_deadlines() -> void:
	for late in [false,true]:
		var k=grain_ready(); var s: Dictionary=k.snapshot(); var task: Dictionary=s.tasks.values()[0]
		check(wait_until(k,task.deadline_tick if late else task.deadline_tick-1),"正常等待到期前边界，所有领域共用同一时钟")
		check(send(k,"report_offer",{"offer_id":"offer.grain","context":ctx("west.office","actor.chen","role.office")}).ok,"last-boundary report begins")
		check(k.advance_one_tick().ok,"last-boundary report completes")
		s=k.snapshot(); task=s.tasks.values()[0]
		check(task.status=="success" and task.late==late,"completed tick controls lateness")
		check(s.wallets["wallet.player"].balance==(1017 if late else 1057),"late halves fee but not duplicated funds")
	var k=grain_ready(); var s: Dictionary=k.snapshot()
	check(wait_until(k,s.tasks.values()[0].deadline_tick+2),"正常等待越过宽限期，不单独改写存档日期")
	var before: Dictionary=k.snapshot()
	check(not send(k,"report_offer",{"offer_id":"offer.grain","context":ctx("west.office","actor.chen","role.office")}).ok and k.snapshot()==before,"beyond grace report rejected without side effects")
	check(send(k,"cancel_task",{"offer_id":"offer.grain","context":ctx("west.office","actor.chen","role.office")}).ok,"expired task has explicit lawful clearance")
	s=k.snapshot()
	check(s.tasks.values()[0].status=="failed" and s.wallets["wallet.player"].balance==800,"failure clearance never pays surplus or reward")
	check(s.wallets["wallet.office"].balance==4527 and s.inventories["bag:actor.player"].lots.is_empty(),"remaining funds and public grain returned")

## 通过真实等待达到期限测试所需日期；命令失败立即返回，防止测试用改时钟掩盖领域漏结算。
func wait_until(k, target: int) -> bool:
	while k.snapshot().tick<target:
		if not send(k,"wait").ok or not k.advance_one_tick().ok: return false
	return k.snapshot().tick==target

## 检查当前代理人、私人身份、过期报价及采购中断后的恢复。
func test_delegation() -> void:
	var k=fresh(); send(k,"move_local",{"place_id":"west.grain"})
	var params: Dictionary={"shop_id":"shop.grain","item_id":"item.grain","side":"buy","quantity":2,"payer_wallet_id":"wallet.player","context":ctx("west.grain","actor.zheng","role.merchant")}
	var quote: Dictionary=k.preview("trade",params)
	var s: Dictionary=k.snapshot(); s.actors["actor.zheng"].location={"kind":"Settlement","settlement_id":"city.west","place_id":"west.street"}
	check(k.restore(s).ok,"merchant genuinely leaves")
	check(not k.preview("trade",params).ok,"absent merchant cannot trade")
	params.context=ctx("west.grain","actor.apprentice","role.clerk")
	check(k.preview("trade",params).ok,"authorized clerk handles same shop")
	params.quoted_total=quote.total+1; params.market_revision=quote.market_revision
	var before: Dictionary=k.snapshot()
	check(not send(k,"trade",params).ok and k.snapshot()==before,"altered quote not silently paid")
	params.erase("quoted_total"); params.erase("market_revision")
	check(send(k,"trade",params).ok,"clerk sells actual shared stock")
	var pending: Dictionary=k.snapshot()
	var text: String=Codec.encode(pending,1,"manual-2")
	check(not text.is_empty(),"already-paid transaction can save")
	var decoded: Dictionary=Codec.decode(text); var copy=fresh()
	check(decoded.ok and copy.restore(decoded.state).ok,"already-paid transaction restores")
	check(k.advance_one_tick().ok and copy.advance_one_tick().ok and k.snapshot()==copy.snapshot(),"restored purchase completion does not charge twice")
	check(k.snapshot().inventories["inventory.shop"].lots[0].quantity==98,"clerk and owner share original stock")

## 被困NPC不能阻止无关玩家行动与共享世界时钟推进。
func test_parallel_actions() -> void:
	var k=fresh(); var s: Dictionary=k.snapshot()
	s.actors["actor.shen"].location={"kind":"Settlement","settlement_id":"city.west","place_id":"west.street"}
	s.wallets["wallet.shen"].balance=8
	check(k.restore(s).ok,"NPC traveler fixture")
	check(send(k,"travel",{"destination":"village.east"},"actor.shen").ok,"NPC uses same real travel action")
	check(k.advance_one_tick().ok,"first NPC road step paid")
	check(send(k,"wait").ok,"player starts independent half-day")
	check(k.advance_one_tick().ok,"stranded NPC does not freeze unrelated world actions")
	s=k.snapshot()
	check(s.tick==2 and s.actors[s.player_actor_id].action_id.is_empty(),"player waiting completes while NPC remains on road")
	check(s.actors["actor.shen"].location.kind=="OnRoute" and s.wallets["wallet.shen"].balance==0,"no free NPC travel or negative money")

## 恢复时先拒绝损坏状态，避免实例ID冲突及场景崩溃。
func test_restore_boundaries() -> void:
	var k=fresh(); send(k,"move_local",{"place_id":"west.office"}); var s: Dictionary=k.snapshot(); var before: Dictionary=s.duplicate(true)
	s.next_instance_id=1
	check(not k.restore(s).ok and k.snapshot()==before,"restored instance allocator cannot reuse existing identities")
	for field in ["knowledge","roles","wallets","inventories","rng","interactions"]:
		s=before.duplicate(true); s[field]=[]
		check(not k.restore(s).ok and k.snapshot()==before,"invalid table type refused: "+field)
	for removed in ["wallet.office","wallet.shop"]:
		s=before.duplicate(true); s.wallets.erase(removed)
		check(not k.restore(s).ok and k.snapshot()==before,"missing required funding wallet refused: "+removed)
	for removed in ["inventory.shop","store:org.office"]:
		s=before.duplicate(true); s.inventories.erase(removed)
		check(not k.restore(s).ok and k.snapshot()==before,"missing required shop/receiving inventory refused: "+removed)
	check(k.restore(before).ok,"reset after malformed reference fixtures")
	# 存档规范化会重排字典；淘汰仍按版本先后，不能依赖JSON键顺序。
	for _i in range(35):
		send(k,"move_local",{"place_id":"west.street"}); send(k,"move_local",{"place_id":"west.office"})
	var clone=fresh(); var decoded: Dictionary=Codec.decode(Codec.encode(k.snapshot(),1,"manual-3"))
	check(decoded.ok and clone.restore(decoded.state).ok,"result cache restores after pruning")
	check(send(k,"inspect",{"prop_id":"notice.grain"}).ok and send(clone,"inspect",{"prop_id":"notice.grain"}).ok and clone.snapshot()==k.snapshot(),"post-JSON eviction preserves deterministic state")

## 在隔离内容中递归替换实体身份，保持对白语法与规则结构不变。
func remap_ids(value: Variant, mapping: Dictionary) -> Variant:
	if value is String: return mapping.get(value,value)
	if value is Array:
		var copied: Array=[]
		for child in value: copied.append(remap_ids(child,mapping))
		return copied
	if value is Dictionary:
		var copied: Dictionary={}
		for key in value: copied[mapping.get(key,key)]=remap_ids(value[key],mapping)
		return copied
	return value

## 改名全部已声明实体后重跑采购，旧包可省略扩展表，核心仍不能依赖样例ID。
func test_renamed_content() -> void:
	var header: Dictionary=Strict.parse_text(FileAccess.get_file_as_string("res://data/generated/two_stories.hmcatalog.json")).value
	var mapping: Dictionary={}
	for table in Catalog.TABLES:
		for id in header.catalog.get(table,{}): mapping[id]="renamed."+id
	header.catalog=remap_ids(header.catalog,mapping)
	header.content_id="test.renamed"
	header.catalog_sha256=(JSON.stringify(header.catalog,"",true,true)+"\n").sha256_text()
	var path: String="user://renamed-content.hmcatalog.json"
	var file=FileAccess.open(path,FileAccess.WRITE); file.store_string(JSON.stringify(header)); file.close()
	var k=Kernel.new()
	var loaded: Dictionary=k.new_game(mapping["scenario.two_stories"],path)
	if not loaded.ok: printerr("RENAME_LOAD: ",loaded)
	check(loaded.ok,"renamed author content loads without code changes")
	if not k.snapshot().is_empty():
		check(send(k,"move_local",remap_ids({"place_id":"west.office"},mapping)).ok,"renamed place entry")
		check(send(k,"inspect",remap_ids({"prop_id":"notice.grain"},mapping)).ok,"renamed discovery source")
		check(send(k,"accept_offer",remap_ids({"offer_id":"offer.grain","context":ctx("west.office","actor.chen","role.office")},mapping)).ok and k.advance_one_tick().ok,"renamed officer accepts")
		var task: Dictionary=k.snapshot().tasks.values()[0]
		send(k,"move_local",remap_ids({"place_id":"west.street"},mapping)); send(k,"move_local",remap_ids({"place_id":"west.grain"},mapping))
		var request: Dictionary=remap_ids({"shop_id":"shop.grain","item_id":"item.grain","side":"buy","quantity":10,"context":ctx("west.grain","actor.zheng","role.merchant")},mapping)
		request.payer_wallet_id=task.budget_wallet_id
		check(send(k,"trade",request).ok and k.advance_one_tick().ok,"renamed merchant transacts")
		send(k,"move_local",remap_ids({"place_id":"west.street"},mapping)); send(k,"move_local",remap_ids({"place_id":"west.office"},mapping))
		check(send(k,"report_offer",remap_ids({"offer_id":"offer.grain","context":ctx("west.office","actor.chen","role.office")},mapping)).ok and k.advance_one_tick().ok,"renamed task reports")
		check(k.snapshot().wallets[mapping["wallet.player"]].balance==1057,"renamed identities preserve generic settlement result")
		var encoded: String=Codec.encode(k.snapshot(),1,"manual-1",k.content())
		check(Codec.decode(encoded,k.content()).ok and not Codec.decode(encoded).ok,"save explicitly bound to renamed content, not default story")
	DirAccess.remove_absolute(ProjectSettings.globalize_path(path))

## 执行各领域回归，仅统计实际求值的断言。
func _run() -> void:
	test_prices(); test_deadlines(); test_delegation(); test_parallel_actions(); test_restore_boundaries(); test_renamed_content()
	print("RULE_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
