## 军政军需从真实作者目录进入Kernel；当面受任、领粮、去程、交付、返营与读档均走普通命令。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const F=preload("res://tests/companion_fixture.gd")
const Codec=preload("res://core/save_codec.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const Location=preload("res://core/world_location.gd")
var checks: int=0
var failures: int=0
var path: String=""

## 测试目录由正式准备器生成，缺少目录明确失败，不以跳过冒充军事已运行。
func _init() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--military-content="): path=arg.trim_prefix("--military-content=")
	call_deferred("run_checks")

## 每个行为记录实际失败，退出码保持可用于既有发行门禁。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 操作失败立即给出领域诊断，调用者停止依赖步骤，保留此前真实命令输出。
func send(k, kind: String, params: Dictionary={}) -> bool:
	var result: Dictionary=F.send(k,kind,params)
	return check(result.ok,kind+" "+str(result))

## 在正常行旅半日间验证正式Codec与完整世界恢复，不修改人物位置或抢占军团资金。
func restore_same(k) -> bool:
	var before: Dictionary=k.snapshot()
	var decoded: Dictionary=Codec.decode(Codec.encode(before,0,"military-world",k.content()),k.content())
	return check(decoded.ok and k.restore(decoded.state).ok and k.snapshot()==before,"军事轮间存档保留日期、人产和军令阶段")

## 有限半日沿真实路径抵达；本测试不处理非军需的可选个人路遇。
func arrive(k, city: String) -> bool:
	for _half in range(40):
		var state: Dictionary=k.snapshot(); var actor: Dictionary=state.actors[state.player_actor_id]
		if Location.resolve(state,actor.id).get("settlement_id")==city: return true
		var result: Dictionary=k.advance_one_tick()
		if not result.ok: return check(false,"实际行军受阻："+str(result))
	return check(false,"行军在有限真实半日内未能抵达")

## 由普通公职与编组建立唯一军团；同一世界私庄继续生产，公私钱粮不自动混用。
func run_checks() -> void:
	var k=Kernel.new()
	if not check(not path.is_empty() and k.new_game("scenario.alpha_life",path).ok,"真实军事作者目录可建立完整同一世界"):
		finish(); return
	var original: Dictionary=k.snapshot(); var player: String=original.player_actor_id
	var public_money: int=original.wallets["wallet.military.a"].balance
	var context: Dictionary=F.context(k,"role.military.a")
	if not send(k,"office_accept",{"office_id":"office.military.a","context":context}): finish(); return
	check(not k.snapshot().offices["office.military.a"].enabled,"受任手续未完不提前取得指挥权")
	if not check(k.advance_one_tick().ok,"任命与庄务同半日推进"): finish(); return
	check(k.snapshot().offices["office.military.a"].holder_actor_id==player and k.snapshot().estates["estate.riverside"].last_tick==1,"任官同时私人庄园正常经营")
	if not send(k,"campaign_accept",{"template_id":"order.military.a","context":context}): finish(); return
	var state: Dictionary=k.snapshot(); var order: String=state.campaigns.keys()[0]
	var wrong: Dictionary={"order_id":order,"unit_ids":["unit.military.a","unit.military.b"],"food_quantity":120,"money":100}
	check(not k.preview("army_form",wrong).ok and k.snapshot()==state,"本部将领不能编入第二部或远方别人的驻军")
	if not send(k,"army_form",{"order_id":order,"unit_ids":["unit.military.a"],"food_quantity":160,"money":100}): finish(); return
	if not restore_same(k): finish(); return
	if not check(k.advance_one_tick().ok,"实际兵装粮钱完成半日装运"): finish(); return
	state=k.snapshot(); var army_id: String=state.campaigns[order].army_id; var army: Dictionary=state.armies[army_id]
	check(army.unit_ids==["unit.military.a"] and state.military_units["unit.military.a"].fit.size()==8,"军团来自原驻军八个稳定兵员，无凭空增兵")
	check(state.wallets[army.wallet_id].balance==100 and state.wallets["wallet.military.a"].balance==public_money-100 and state.wallets[state.actors[player].wallet_id].balance==original.wallets[original.actors[player].wallet_id].balance,"军费由真实公库转入，私人钱柜不变")
	if not send(k,"campaign_depart",{"campaign_id":order}): finish(); return
	state=k.snapshot(); var action: Dictionary=state.actions[state.actors[player].action_id]
	check(action.kind=="travel" and action.params.mode=="army" and action.params.payer_wallet_id==army.wallet_id,"出征实际建立地形行旅并由本军公柜承担费用")
	if not restore_same(k) or not arrive(k,"village.east"): finish(); return
	check(k.snapshot().campaigns[order].status=="operating","真实抵达敌城入口后军令才进入当地执行阶段")
	var target: Dictionary=k.snapshot().inventories["inventory.military.b"]
	var before_goods: int=Inventory.count(target,"item.military.a.food",[{"kind":"organization","id":"org.military.b"}])
	if not send(k,"campaign_deliver",{"campaign_id":order}): finish(); return
	state=k.snapshot()
	check(Inventory.count(state.inventories[target.id],"item.military.a.food",[{"kind":"organization","id":"org.military.b"}])==before_goods+20,"按明确外交接收授权交付二十份实际公粮")
	check(not F.send(k,"campaign_deliver",{"campaign_id":order}).ok,"交货事实不能重复领出或转移")
	if not send(k,"campaign_return",{"campaign_id":order}) or not arrive(k,"city.west"): finish(); return
	if not send(k,"army_return",{"army_id":army_id}) or not check(k.advance_one_tick().ok,"回营实际清算剩余兵员装备钱粮"): finish(); return
	state=k.snapshot()
	check(state.armies[army_id].status=="closed" and state.campaigns[order].status=="awaiting_report" and state.inventories[army.inventory_id].lots.is_empty(),"军团实物结清后才待回报")
	if not restore_same(k) or not send(k,"campaign_report",{"campaign_id":order,"context":F.context(k,"role.military.a")}): finish(); return
	if not send(k,"office_resign",{"office_id":"office.military.a","context":F.context(k,"role.military.a")}): finish(); return
	state=k.snapshot()
	check(state.campaigns[order].status=="closed" and not state.offices["office.military.a"].enabled and state.estates["estate.riverside"].owner_actor_id==player,"军需回报后正常离职，原私人事业保留")
	restore_same(k)
	finish()

## 汇总实际流程断言，失败不以正常退出或静态配置验收代替。
func finish() -> void:
	print("MILITARY_WORLD_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
