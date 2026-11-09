## 开局与增援的真实行动互斥门；复用正式tiny输入和公共命令，不改运行状态或兵员来源。
extends "res://tests/test_siege_supply_sources.gd"
const B="base::actor.military.b.commander"

## 正式受任、编组与到城后停在围城前，保留每名实际成员的行动释放证据。
func assembled() -> Dictionary:
	var k=Kernel.new()
	if not check(k.new_game("base::scenario.alpha_life",content_path).ok,"正式作者Kernel启动占用场景"): return {}
	for label in ["a","b","c"]:
		var actor: String="base::actor.military."+label+".commander"
		if not send(k,"office_accept",{"office_id":"base::office.military."+label,"context":Interaction.for_role(k.snapshot(),k.content().data,"base::role.military."+label)},actor).ok: return {}
	if not advance(k): return {}
	var campaigns: Dictionary={}
	for label in ["a","b","c"]:
		var actor: String="base::actor.military."+label+".commander"
		var template: String="base::order.military."+label+(".spear" if label=="c" else "")
		var accepted: Dictionary=send(k,"campaign_accept",{"template_id":template,"context":Interaction.for_role(k.snapshot(),k.content().data,"base::role.military."+label)},actor)
		if not accepted.ok: return {}
		campaigns[label]=accepted.details.campaign_id
		var units: Array=["base::unit.military."+label]
		if label=="a": units.append("base::unit.military.a.second")
		if not send(k,"army_form",{"order_id":campaigns[label],"unit_ids":units,"food_quantity":160 if label!="b" else 0,"money":20},actor).ok: return {}
	if not advance(k): return {}
	for label in ["a","b","c"]:
		if not send(k,"campaign_depart",{"campaign_id":campaigns[label]},"base::actor.military."+label+".commander").ok: return {}
	if not advance(k): return {}
	var state: Dictionary=k.snapshot()
	for label in campaigns:
		var campaign: Dictionary=state.campaigns[campaigns[label]];var army: Dictionary=state.armies[campaign.army_id]
		check(campaign.status=="operating" and state.facts[campaign.arrival_fact_id].kind=="campaign_arrival","真实到城凭据 "+label)
		check(army.member_actor_ids==state.facts[army.formation_source].terms.member_actor_ids,"当前具名成员来自真实编组 "+label)
		for person in army.member_actor_ids: check(state.actors[person].action_id.is_empty(),"到城稳定边界无残留行动 "+person)
	return {"k":k,"campaigns":campaigns}

## 预览与原请求两次失败都不得分配ID、消费序号或改动完整世界候选。
func refused(k, kind: String, params: Dictionary, actor: String, code: String, label: String) -> void:
	var before: Dictionary=k.snapshot();var request: Dictionary=command(k,kind,params,actor)
	var preview: Dictionary=k.preview(kind,params,actor)
	check(not preview.ok and preview.get("code")==code and k.snapshot()==before,label+" 只读预览首错 "+str(preview))
	var first: Dictionary=k.submit(request)
	check(not first.ok and first.get("code")==code,label+" 原提交拒绝 "+str(first))
	check(k.snapshot()==before,label+" 全世界/RNG/资产/历史/ID/请求序号不变")
	check(k.submit(request)==first and k.snapshot()==before,label+" 同一失败请求重试不分ID")

## 真正接纳围城仅记一次成功；原请求重放不再生成围城、AI或结果ID。
func accepted_start(f: Dictionary) -> String:
	var k=f.k;var request: Dictionary=command(k,"siege_start",{"campaign_id":f.campaigns.a})
	var preview: Dictionary=k.preview("siege_start",request.params)
	if not check(preview.ok,"合法空闲开局预览 "+str(preview)): return ""
	var result: Dictionary=k.submit(request)
	if not check(result.ok,"合法空闲开局提交 "+str(result)): return ""
	var state: Dictionary=k.snapshot();var id: String=result.details.siege_id
	check(state.sieges[id].phase=="active" and state.sieges[id].armies.size()==2,"开局只占主攻与当前选中守軍")
	check(k.submit(request)==result and k.snapshot()==state,"开局原请求只创建一次围城")
	return id

## 正向归营在先须拒绝，反向围城在先仍沿用原SiegePending；不能形成同时占用。
func return_pair() -> void:
	var f: Dictionary=assembled()
	if f.is_empty(): return
	var k=f.k;var army: String=k.snapshot().campaigns[f.campaigns.a].army_id
	if not send(k,"army_return",{"army_id":army}).ok: return
	refused(k,"siege_start",{"campaign_id":f.campaigns.a},A,"ActorBusy","主攻真实归营未完成")
	var normal: Dictionary=assembled()
	if normal.is_empty() or accepted_start(normal).is_empty(): return
	refused(normal.k,"siege_start",{"campaign_id":normal.campaigns.a},A,"SiegeAlreadyActive","已存在围城保持旧拒绝")
	refused(normal.k,"army_return",{"army_id":normal.k.snapshot().campaigns[normal.campaigns.a].army_id},A,"SiegePending","先围城再归营保持旧互斥")

## 开局同时检查被选守军；本人忙在其它成员忙之前，未参与第三军则不影响开局。
func start_participants() -> void:
	var f: Dictionary=assembled()
	if f.is_empty(): return
	var k=f.k;var army_b: String=k.snapshot().campaigns[f.campaigns.b].army_id
	if not send(k,"army_return",{"army_id":army_b},B).ok: return
	refused(k,"siege_start",{"campaign_id":f.campaigns.a},A,"ParticipantBusy","被选守军真实归营未完成")
	var both: Dictionary=assembled()
	if both.is_empty(): return
	var busy=both.k
	if not send(busy,"army_return",{"army_id":busy.snapshot().campaigns[both.campaigns.b].army_id},B).ok or not send(busy,"wait").ok: return
	refused(busy,"siege_start",{"campaign_id":both.campaigns.a},A,"ActorBusy","本人和守軍都忙先报本人")
	var unrelated: Dictionary=assembled()
	if unrelated.is_empty(): return
	var other: String=unrelated.k.snapshot().campaigns[unrelated.campaigns.c].army_id
	if not send(unrelated.k,"army_return",{"army_id":other},C).ok: return
	accepted_start(unrelated)

## 旧军令/权限/位置错误必须先于新忙碌守卫；所有输入仍通过真实命令建立。
func original_errors() -> void:
	var f: Dictionary=assembled()
	if f.is_empty(): return
	var k=f.k
	if not send(k,"wait").ok: return
	refused(k,"siege_start",{"campaign_id":"missing"},A,"SiegeOperatingOrderRequired","坏军令先于忙碌")
	refused(k,"siege_start",{"campaign_id":f.campaigns.b},A,"SiegeOperatingOrderRequired","他人军令先于忙碌")
	if not send(k,"wait",{},C).ok: return
	refused(k,"siege_start",{"campaign_id":f.campaigns.c},C,"SiegeOffensiveOrderRequired","合法非攻城军令先于忙碌")

## 八钱只供原在途命令通过报价，离城后原位置首错仍先于忙碌；不修改任何价格或运行钱包。
func location_priority() -> void:
	var f: Dictionary=assembled()
	if f.is_empty(): return
	var k=f.k
	check(k.snapshot().wallets[k.snapshot().actors[A].wallet_id].balance==8,"精确使用独立作者的8钱钱包")
	if not send(k,"travel",{"destination":"base::village.east"}).ok: return
	refused(k,"siege_start",{"campaign_id":f.campaigns.a},A,"SiegeArmyNotAtTarget","真正离城在途先报场所")

## 新援军的归营或等待不能与加入重叠；原未知围城和军令首错仍先于新占用检查。
func join_busy() -> void:
	for action in ["wait","army_return"]:
		var f: Dictionary=assembled()
		if f.is_empty(): return
		var id: String=accepted_start(f)
		if id.is_empty(): return
		var k=f.k;var army: String=k.snapshot().campaigns[f.campaigns.c].army_id
		if not send(k,action,{"army_id":army} if action=="army_return" else {},C).ok: return
		refused(k,"siege_join",{"siege_id":"missing","campaign_id":f.campaigns.c},C,"UnknownSiege","加入未知围城先于忙碌")
		refused(k,"siege_join",{"siege_id":id,"campaign_id":f.campaigns.a},C,"SiegeOperatingOrderRequired","加入他人军令先于忙碌")
		refused(k,"siege_join",{"siege_id":id,"campaign_id":f.campaigns.c},C,"ActorBusy","新增援軍的真实 "+action)

## 合法新援军只消费一次真实到城凭据；加入不发兵、不转库存，也不分配第二次成功结果。
func join_idle() -> void:
	var f: Dictionary=assembled()
	if f.is_empty(): return
	var id: String=accepted_start(f)
	if id.is_empty(): return
	var k=f.k;var before: Dictionary=k.snapshot();var campaign: Dictionary=before.campaigns[f.campaigns.c]
	var request: Dictionary=command(k,"siege_join",{"siege_id":id,"campaign_id":campaign.id},C)
	var quote: Dictionary=k.preview("siege_join",request.params,C)
	if not check(quote.ok and k.snapshot()==before,"空闲同组织援军只读预览成功 "+str(quote)): return
	var result: Dictionary=k.submit(request)
	if not check(result.ok,"空闲同组织真实援军加入 "+str(result)): return
	var state: Dictionary=k.snapshot();var siege: Dictionary=state.sieges[id]
	check(siege.armies.size()==3 and siege.armies[campaign.army_id]==0 and siege.seen_receipt_ids.count(campaign.arrival_fact_id)==1,"仅本次新援军与真实到城来源加入一次")
	check(state.inventories==before.inventories and state.wallets==before.wallets and state.military_units==before.military_units,"加入不发兵或转货款")
	check(k.submit(request)==result and k.snapshot()==state,"同加入请求重放不二次消费来源或分ID")
	refused(k,"siege_join",request.params,C,"SiegeReinforcementInvalid","已加入者沿用原拒绝")

## 原抵达时刻和已交军议不能被busy抢首错；只以真实wait和siege_plan构造边界。
func join_original_errors() -> void:
	var f: Dictionary=assembled()
	if f.is_empty() or not advance(f.k): return
	var id: String=accepted_start(f)
	if id.is_empty() or not send(f.k,"wait",{},C).ok: return
	refused(f.k,"siege_join",{"siege_id":id,"campaign_id":f.campaigns.c},C,"SiegeArrivalReceiptRequired","早于围城的真实到城凭据先拒绝")
	var planned: Dictionary=assembled()
	if planned.is_empty(): return
	id=accepted_start(planned)
	if id.is_empty(): return
	var orders: Dictionary={"base::unit.military.a":{"action":"blockade","node_id":"supply"},"base::unit.military.a.second":{"action":"blockade","node_id":"supply"}}
	if not send(planned.k,"siege_plan",{"siege_id":id,"orders":orders}).ok or not send(planned.k,"wait",{},C).ok: return
	refused(planned.k,"siege_join",{"siege_id":id,"campaign_id":planned.campaigns.c},C,"SiegePlanAlreadySubmitted","已交军议先于忙碌")

## 旧默认作者中的第三组织仍无权加入，不能为了忙碌错误改写既有组织授权。
func foreign_join_error() -> void:
	var f: Dictionary=assembled()
	if f.is_empty(): return
	var id: String=accepted_start(f)
	if id.is_empty() or not send(f.k,"wait",{},C).ok: return
	refused(f.k,"siege_join",{"siege_id":id,"campaign_id":f.campaigns.c},C,"SiegeReinforcementSideUnauthorized","第三组织先于忙碌")

## 每个输入都核实独立pin和安装索引的精确锁，不依赖索引枚举顺序或重标版本。
func use_pin(name: String) -> bool:
	var pin: Dictionary=JSON.parse_string(FileAccess.get_file_as_string("res://data/"+name+"-fixture.json"))
	var index: Dictionary=JSON.parse_string(FileAccess.get_file_as_string("res://data/generated/installed-content.json"));var matched: bool=false
	for row in index.sets:
		if row.pack_set_digest==pin.pack_set_digest and row.directory==pin.directory: matched=true
	if not check(matched,"独立pin精确命中 "+name): return false
	content_path="res://data/generated/"+pin.directory+"/catalog.hmcatalog.json"
	print("SIEGE_OCCUPATION_INPUT=",name," ",pin.pack_set_digest)
	return true

## 复用已冻结粮源工程及其精确锁，不运行粮源旧门或重新stage。
func run() -> void:
	print("SIEGE_OCCUPATION_STORAGE={file_backend:none,Codec:false,user_data_dir:",OS.get_user_data_dir(),"}")
	if not check(ProjectSettings.get_setting("application/config/siege_supply_isolated",false),"仅独立正式作者工程"): finish();return
	if not use_pin("siege-supply"): finish();return
	return_pair();start_participants();original_errors();foreign_join_error()
	if not use_pin("siege-occupation"): finish();return
	location_priority();join_busy();join_idle();join_original_errors();finish()

## 本门只覆盖围城接纳时的行动互斥与原错误顺序，不声明终局、损失或完整存读验收。
func finish() -> void:
	print("SIEGE_START_OCCUPATION_CHECKS=",checks," FAILURES=",failures);quit(0 if failures==0 else 1)
