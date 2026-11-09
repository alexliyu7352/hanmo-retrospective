## 护具共享入口的真实新锁短门；初始实物、装备命令、存读和实际卖出同一lot，不重复个人战几何矩阵。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Armor=preload("res://core/domains/personal/personal_armor.gd")
const Codec=preload("res://core/save_codec.gd")
const Interaction=preload("res://core/interaction_rules.gd")
var checks: int=0
var failures: int=0

## 从专属正式准备工程启动，不读用户存档或图形界面。
func _init() -> void:
	call_deferred("run")

## 每个失败影响退出；先核前置成功再访问返回字段。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 真实版本化指令共用当前玩家身份和唯一请求序号。
func command(k, kind: String, params: Dictionary) -> Dictionary:
	var state: Dictionary=k.snapshot();var actor: String=state.player_actor_id;var original_rng: Dictionary=state.rng.duplicate(true)
	return {"kind":kind,"params":params,"actor_id":actor,"expected_revision":state.revision,"request_seq":state.request_sequences[actor]+1}

## 新局从作者明确初货得到装备，卖出后选择可留而实际防御必须消失。
func run() -> void:
	var index: Dictionary=JSON.parse_string(FileAccess.get_file_as_string("res://data/generated/installed-content.json"))
	var path: String="res://data/generated/"+index.sets[0].directory+"/catalog.hmcatalog.json"
	var k=Kernel.new();var started: Dictionary=k.new_game("base::scenario.procurement",path)
	if not check(started.ok,"正式无武器有护具作者开局 "+str(started)): quit(1);return
	var state: Dictionary=k.snapshot();var actor: String=state.player_actor_id;var original_rng: Dictionary=state.rng.duplicate(true)
	check(state.actors[actor].combat.weapon_id=="" and state.actors[actor].combat.armor_id=="base::item.coat","工厂只投影显式armor初态")
	check(Armor.effective(state,k.content().data,actor).armor_defense==6,"实际本人未预留lot才给防御")
	var denied: Dictionary=k.submit(command(k,"equip_armor",{"armor_id":"base::item.public_coat"}))
	check(denied.code=="ArmorNotOwned" and k.snapshot()==state,"公共库存不能借穿且失败整态不变")
	var removed: Dictionary=k.submit(command(k,"equip_armor",{"armor_id":""}))
	check(removed.ok and Armor.effective(k.snapshot(),k.content().data,actor).armor_defense==0,"真实命令可显式卸下")
	var request: Dictionary=command(k,"equip_armor",{"armor_id":"base::item.coat"})
	var equipped: Dictionary=k.submit(request)
	if not check(equipped.ok,"本人装备走注册入口 "+str(equipped)): quit(1);return
	state=k.snapshot()
	check(k.submit(request)==equipped and k.snapshot()==state,"同请求重放不改变第二次状态")
	check(state.tick==0 and state.rng==original_rng,"装备不推日期或抽种")
	var saved: Dictionary=k.save_context();var decoded: Dictionary=Codec.decode(Codec.encode(saved.state,1,"armor-kernel",saved.catalog,saved.registry),saved.catalog,saved.registry)
	check(decoded.ok and decoded.state==state,"actor可选armor字段完整v2校验")
	var context: Dictionary=Interaction.for_role(state,k.content().data,"base::role.merchant")
	var trade: Dictionary=k.submit(command(k,"trade",{"shop_id":"base::shop.grain","item_id":"base::item.coat","quantity":1,"side":"sell","payer_wallet_id":state.actors[actor].wallet_id,"context":context}))
	if not check(trade.ok,"原trade真实转出最后一件装备 "+str(trade)): quit(1);return
	state=k.snapshot()
	check(state.actors[actor].combat.armor_id=="base::item.coat" and Armor.effective(state,k.content().data,actor).armor_defense==0,"最后实物卖出后立即零防御，不靠清理选择伪造产权")
	saved=k.save_context();decoded=Codec.decode(Codec.encode(saved.state,2,"armor-sold",saved.catalog,saved.registry),saved.catalog,saved.registry)
	check(decoded.ok,"陈旧选择合法存读但不能授予护具效果")
	print("ARMOR_KERNEL_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
