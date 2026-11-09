## 真实人产、攻城终局与公库易帜领域集成；覆盖拒绝、守恒、旧军团与实际替代营地，不访问玩家档案。
extends SceneTree
const F=preload("res://tests/garrison_ownership_fixture.gd")
const Military=preload("res://tests/military_combat_fixture.gd")
const Assets=preload("res://core/military_assets.gd")
const Factory=preload("res://core/world_factory.gd")
const Campaign=preload("res://core/campaign_rules.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0

## 等引擎初始化完成后运行领域测试，退出码由真实断言决定。
func _init() -> void:
	call_deferred("run")

## 每项错误保留具体情境，统一发行门禁汇总格式。
func check(ok: bool, label: String) -> void:
	checks+=1
	if not ok: failures+=1; printerr("FAIL: ",label)

## 双方都通过正式任官、受令和装备编组，随后通过正式 Siege 产生可以移交的胜利。
func prepared() -> Dictionary:
	var f: Dictionary=F.fresh(); check(f.initialized.ok,"三座作者营地由工厂初始化")
	var a: Dictionary=Military.mobilize(f); check(a.ok,"真实攻军编组："+str(a))
	var b: Dictionary=Military.mobilize(f,true); check(b.ok,"真实守军编组："+str(b))
	if not a.ok or not b.ok: return {}
	f.army_a=a.details.army_id; f.army_b=b.details.army_id
	var won: Dictionary=F.win(f); check(won.ok,"正式破门与守点获得占领终局："+str(won))
	check(Assets.validate_state(f.state,f.data).ok,"转产权之前真实战争供粮账合法")
	return f

## 公库原址转移守恒、永久链来源与失职军队归营必须共同通过，不能只改 owner 让存档表面合法。
func run() -> void:
	var f: Dictionary=prepared()
	if f.is_empty(): print("CHECKS=",checks," FAILURES=",failures); quit(1); return
	var camp: Dictionary=f.data.garrisons[f.enemy_camp]
	var before: Dictionary=f.state.duplicate(true)
	check(Assets.garrison_owner(f.state,f.data,f.enemy_camp).organization_id==camp.organization_id and f.state==before,"未占领与旧包查询只读且保留默认归属")
	check(Assets.public_owner(f.state,f.data,"inventory_id",camp.inventory_id).owner==f.state.inventories[camp.inventory_id].owner,"原公库产权可供根层存档验证")
	var receipt: String=F.receipt(f)
	check(not Assets.validate_state(f.state,f.data).ok,"未应用占领回执不能被存为稳定档")
	for field in ["army_id","campaign_id","siege_id","stronghold_id","garrison_id"]:
		var bad: Dictionary=f.state.duplicate(true); bad.facts[receipt][field]="invented"; var snapshot: Dictionary=bad.duplicate(true)
		check(not Assets.apply_occupation(bad,f.data,receipt).ok and bad==snapshot,"拒绝伪造永久来源且原子不变："+field)
	for change in [{"phase":"active"},{"result":{"kind":"defended","winner_side":1,"tick":f.state.tick}},{"result":{"kind":"occupied","winner_side":0,"tick":f.state.tick+1}}]:
		var bad: Dictionary=f.state.duplicate(true)
		for field in change: bad.sieges[f.siege_id][field]=change[field]
		check(not Assets.apply_occupation(bad,f.data,receipt).ok,"非真实已到期胜利不转公产")
	var bad: Dictionary=f.state.duplicate(true); bad.actors[f.actor].location=Factory.settlement_location(f.data,f.data.garrisons[f.camp].place_id)
	check(Assets.apply_occupation(bad,f.data,receipt).code=="OccupationArmyMustBePresent","获胜旧消息不代替实际入城")
	bad=f.state.duplicate(true); bad.facts[receipt].public_inventory_ids=[]
	check(Assets.apply_occupation(bad,f.data,receipt).code=="OccupationPublicManifestMismatch","不能漏掉本营真实公库")
	bad=f.state.duplicate(true); bad.wallets[camp.wallet_id].reserved["action.pending"]=0; var snapshot: Dictionary=bad.duplicate(true)
	check(Assets.apply_occupation(bad,f.data,receipt).code=="OccupationPublicAssetsReserved" and bad==snapshot,"公共预留含零金额也须真实取消后移交")
	bad=f.state.duplicate(true); bad.inventories[camp.inventory_id].lots[0].reserved_by="action.pending"
	check(Assets.apply_occupation(bad,f.data,receipt).code=="OccupationPublicAssetsReserved","公共实物保留不可被易帜吞掉")
	bad=f.state.duplicate(true); bad.military_units[f.enemy_unit].garrison_id=f.enemy_camp; bad.military_units[f.enemy_unit].army_id=""
	check(Assets.apply_occupation(bad,f.data,receipt).code=="GarrisonDefendersUnresolved","未结清在营敌兵不会被删除或改旗收编")
	bad=f.state.duplicate(true); bad.military_units[f.unit].fit=[]; bad.military_units[f.second_unit].fit=[]
	check(Assets.apply_occupation(bad,f.data,receipt).code=="OccupationGuardRequired","没有有效实际守军不能空名占领")
	var private_owner: Dictionary={"kind":"actor","id":f.actor}
	var private_lot: Dictionary={"id":Factory.next_id(f.state,"lot"),"item_id":camp.food_item_id,"quantity":9,"owner":private_owner,"reserved_by":""}
	f.state.inventories[camp.inventory_id].lots.append(private_lot.duplicate(true))
	var other: Dictionary=private_lot.duplicate(true); other.id=Factory.next_id(f.state,"lot"); other.owner={"kind":"organization","id":f.state.armies[f.army_a].organization_id}
	f.state.inventories[camp.inventory_id].lots.append(other)
	before=f.state.duplicate(true)
	var applied: Dictionary=Assets.apply_occupation(f.state,f.data,receipt); check(applied.ok,"同城胜军正式原子接收公产："+str(applied))
	check(f.state.strongholds["stronghold.test"].occupation_id==receipt and Assets.garrison_owner(f.state,f.data,f.enemy_camp).organization_id==f.state.armies[f.army_a].organization_id,"据点归属与公库使用同一永久链")
	check(f.state.wallets[camp.wallet_id].balance==before.wallets[camp.wallet_id].balance and f.state.inventories[camp.inventory_id].lots.size()==before.inventories[camp.inventory_id].lots.size(),"易帜不增减余额或批次数")
	for index in range(before.inventories[camp.inventory_id].lots.size()):
		var old: Dictionary=before.inventories[camp.inventory_id].lots[index]; var actual: Dictionary=f.state.inventories[camp.inventory_id].lots[index]
		check(actual.id==old.id and actual.item_id==old.item_id and actual.quantity==old.quantity,"每批物品实物与来源 ID 保持")
		if old.owner.id!=camp.organization_id: check(actual==old,"私人及第三方组织货物完整保留")
	check(applied.disabled_office_ids==[f.enemy_office] and not f.state.offices[f.enemy_office].enabled and f.state.offices[f.enemy_office].appointment_id==before.offices[f.enemy_office].appointment_id,"失营旧公职停止权限并保留任命史")
	check(f.state.armies==before.armies and f.state.military_units==before.military_units,"占领不改旧军团旗号、军官、位置或五类名册")
	check(Assets.stationed_units(f.state,f.data,f.enemy_camp).is_empty(),"空营不会把作者旧敌兵当成已收编兵员")
	check(not Assets.ready_office(f.state,f.data,f.enemy,f.enemy_office).ok,"旧官无权征发新旗公库")
	var stable: Dictionary=Assets.validate_state(f.state,f.data); check(stable.ok,"占领后可存档且形成完整来源："+str(stable))
	before=f.state.duplicate(true); var again: Dictionary=Assets.apply_occupation(f.state,f.data,receipt)
	check(again.ok and again.already_completed and before==f.state,"同一回执重放不再转财物或修改人事")
	for path in ["chain","owner","army","unit","manifest"]:
		bad=f.state.duplicate(true)
		match path:
			"chain": bad.facts[receipt].previous_occupation_id=receipt
			"owner": bad.wallets[camp.wallet_id].owner=private_owner
			"army": bad.armies[f.army_b].organization_id=f.state.armies[f.army_a].organization_id
			"unit": bad.military_units[f.enemy_unit].organization_id=f.state.armies[f.army_a].organization_id
			"manifest": bad.facts[receipt].asset_manifest.wallets={}
		check(not Assets.validate_state(bad,f.data).ok,"伪造稳定状态被拒绝："+path)
	var saved: Dictionary=Strict.parse_text(JSON.stringify(f.state)).value
	check(Assets.validate_state(saved,f.data).ok and saved==f.state,"JSON 重启保留占领及归属历史")
	check(Assets.preview(f.state,f.data,f.actor,"army_resupply",{"army_id":f.army_a,"garrison_id":f.enemy_camp,"food_quantity":1,"money":0}).code=="InsufficientMilitarySupply","占领营只有另一粮种不能虚报已补本軍口粮")
	var funded: Dictionary=Military.action(f,f.actor,"army_resupply",{"army_id":f.army_a,"garrison_id":f.enemy_camp,"food_quantity":0,"money":3})
	check(funded.ok,"有现任权限的胜军可在占领营从实际新公库补饷："+str(funded))
	check_supply_ownership_scope(f,receipt)
	recapture(f,receipt)
	prisoner_transfer()
	var army: Dictionary=f.state.armies[f.army_b]
	var old_food: int=Inventory.count(f.state.inventories[army.inventory_id],camp.food_item_id,[{"kind":"organization","id":army.organization_id}])
	var supply: Dictionary=Assets.step(f.state,f.data,f.state.tick+1); check(supply.ok,"失营后旧军团仍从自己实际军粮供给")
	check(Campaign.step(f.state,f.data,f.state.tick+1).ok,"共同半日也同步军令时钟")
	f.state.tick+=1
	check(Inventory.count(f.state.inventories[army.inventory_id],camp.food_item_id,[{"kind":"organization","id":army.organization_id}])<old_food,"失营不会停止或错误按新旗吞旧军粮")
	check(Assets.preview(f.state,f.data,f.enemy,"army_return",{"army_id":army.id}).code=="FriendlyGarrisonRequired","默认原营已失，明确拒绝向敌方交账")
	check(Assets.preview(f.state,f.data,f.enemy,"army_return",{"army_id":army.id,"garrison_id":f.friend_camp}).code=="ArmyMustReturnToCamp","指定友营不能跳过真人实际行程")
	var prior_position: Dictionary=f.state.actors[f.enemy].location.duplicate(true)
	f.state.actors[f.enemy].location=Factory.settlement_location(f.data,f.data.garrisons[f.friend_camp].place_id)
	check(prior_position!=f.state.actors[f.enemy].location,"测试明确提供已抵达另一营的宏观边界")
	var return_quote: Dictionary=Assets.preview(f.state,f.data,f.enemy,"army_return",{"army_id":army.id,"garrison_id":f.friend_camp})
	check(return_quote.ok and return_quote.terms.garrison_id==f.friend_camp and return_quote.terms.source_garrison_id==f.enemy_camp,"失职原帅实际归友营可清算，报价保留原营")
	check(not Assets.preview(f.state,f.data,f.enemy,"army_resupply",{"army_id":army.id,"garrison_id":f.friend_camp,"food_quantity":1,"money":0}).ok,"失职可交回已有公产，但不能凭旧任命征发粮钱")
	var returned: Dictionary=Military.action(f,f.enemy,"army_return",{"army_id":army.id,"garrison_id":f.friend_camp})
	check(returned.ok,"真实半日卸装交回另一友营："+str(returned))
	if returned.ok:
		var fact: Dictionary=f.state.facts[returned.details.receipt_id]
		check(fact.garrison_id==f.friend_camp and fact.source_garrison_id==f.enemy_camp and army.return_garrison_id==f.friend_camp and army.garrison_id==f.enemy_camp,"军团初始来源与实际交接地点各自保存")
		check(f.state.military_units[f.enemy_unit].garrison_id==f.friend_camp and f.state.military_units[f.enemy_unit].source_garrison_id==f.enemy_camp and f.enemy_unit in Assets.stationed_units(f.state,f.data,f.friend_camp),"兵员仅在真实返营后转驻防，原始名单来源不变")
		check(f.state.inventories[army.inventory_id].lots.is_empty() and f.state.wallets[army.wallet_id].balance==0,"军团闭环真实清空公产")
		stable=Assets.validate_state(f.state,f.data); check(stable.ok,"替代友营交接后稳定存档合法："+str(stable))
		bad=f.state.duplicate(true); bad.military_units[f.enemy_unit].garrison_id=f.camp
		check(not Assets.validate_state(bad,f.data).ok,"不能借合法归营收据再任意瞬移兵员")
		var feeding: Dictionary=Assets.step(f.state,f.data,f.state.tick+1); check(feeding.ok,"转调后的同营日账继续覆盖实际来兵")
		f.state.tick+=1
		var found: bool=false
		for id in feeding.get("events",[]):
			var day: Dictionary=f.state.facts[id]
			if day.garrison_id==f.friend_camp: found=day.mouth_ids.has(f.state.military_units[f.enemy_unit].fit[0])
		check(found,"友营口粮不只扫描作者初始驻军")
	print("CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)

## 再次收复仍从正式 Siege 生成新终局；旧凭据可查询但不能重放把城改回前一旗号。
func recapture(source: Dictionary, first_receipt: String) -> void:
	var f: Dictionary=source.duplicate(true)
	var actor: String=f.actor; f.actor=f.enemy; f.enemy=actor
	var army: String=f.army_a; f.army_a=f.army_b; f.army_b=army
	var won: Dictionary=F.win(f); check(won.ok,"真实旧军团经另一场围城收复原城："+str(won))
	if not won.ok: return
	var receipt: String=F.receipt(f); var applied: Dictionary=Assets.apply_occupation(f.state,f.data,receipt)
	check(applied.ok and f.state.facts[receipt].previous_occupation_id==first_receipt,"收复沿前次占领继续永久产权链")
	var stable: Dictionary=Assets.validate_state(f.state,f.data); check(stable.ok,"两次易帜后冻结军团来源与供粮日账仍合法："+str(stable))
	check_supply_ownership_scope(f,receipt)
	var before: Dictionary=f.state.duplicate(true); var again: Dictionary=Assets.apply_occupation(f.state,f.data,first_receipt)
	check(again.ok and again.already_completed and f.state==before,"重放前次占領不撤销后来收复")
	check(not f.state.offices[f.enemy_office].enabled,"收复城池也不自动恢复旧官任命")

## 战后分类边界提供一名真实被俘兵，正式返营、销令、再编组后由守军出城，俘虏留在真实原营。
func prisoner_transfer() -> void:
	var f: Dictionary=F.fresh()
	var a: Dictionary=Military.mobilize(f); var b: Dictionary=Military.mobilize(f,true)
	check(a.ok and b.ok,"俘虏测试双方仍走真实任官编组")
	if not a.ok or not b.ok: return
	f.army_a=a.details.army_id; f.army_b=b.details.army_id
	var person: String=f.state.military_units[f.unit].fit[0]
	var results: Dictionary={f.unit:f.state.military_units[f.unit].duplicate(true),f.enemy_unit:f.state.military_units[f.enemy_unit].duplicate(true)}
	results[f.unit].fit.erase(person); results[f.unit].captured.append(person)
	var battle_id: String=Factory.next_id(f.state,"battle")
	f.state.facts[battle_id]={"kind":"military_battle","phase":"completed","tick":f.state.tick,"army_ids":[f.army_a,f.army_b],"unit_ids":[f.unit,f.enemy_unit],"result":{"kind":"repelled","winner_side":1}}
	var settled: Dictionary=Assets.settle_battle(f.state,f.data,battle_id,results,{person:f.army_b})
	check(settled.ok,"明确战果回写边界移交真实被俘兵，不声称此处跑过六角命中")
	var campaign_id: String=f.state.armies[f.army_b].order_id
	check(Campaign.apply(f.state,f.data,f.enemy,"campaign_cancel",{"campaign_id":campaign_id}).ok,"守军先依军令退出本次出征")
	var returned: Dictionary=Military.action(f,f.enemy,"army_return",{"army_id":f.army_b})
	check(returned.ok,"真实归营清算将既有实俘留在营中")
	if not returned.ok: return
	check(Campaign.army_returned(f.state,f.data,campaign_id,returned.details.receipt_id).ok,"实际归营回执接入销令")
	var role: String=f.data.offices[f.enemy_office].appointing_role_id
	var context: Dictionary=Interaction.for_role(f.state,f.data,role)
	check(Campaign.apply(f.state,f.data,f.enemy,"campaign_report",{"campaign_id":campaign_id,"context":context}).ok,"原军令当面报告闭环")
	var accepted: Dictionary=Campaign.apply(f.state,f.data,f.enemy,"campaign_accept",{"template_id":f.enemy_office,"context":context})
	check(accepted.ok,"现任官重新当面受令")
	if not accepted.ok: return
	var formed: Dictionary=Military.action(f,f.enemy,"army_form",{"order_id":accepted.details.campaign_id,"unit_ids":[f.enemy_unit],"retainer_ids":[],"food_quantity":100,"money":0})
	check(formed.ok,"原部队再次实际出营而不带走留营俘虏")
	if not formed.ok: return
	var old_keeper: String=f.army_b; f.army_b=formed.details.army_id
	check(person in f.state.garrisons[f.enemy_camp].prisoner_ids and f.state.military_units[f.unit].captors[person]==old_keeper,"营中实俘保留第一次真实看管来源")
	var won: Dictionary=F.win(f); check(won.ok,"现守军实际战术撤离后让出城池")
	if not won.ok: return
	var receipt: String=F.receipt(f); var applied: Dictionary=Assets.apply_occupation(f.state,f.data,receipt)
	check(applied.ok,"占领军接收同城营中实俘")
	check(f.state.garrisons[f.enemy_camp].prisoner_ids.is_empty() and person in f.state.armies[f.army_a].prisoner_ids and f.state.military_units[f.unit].captors[person]==f.army_a,"实俘只换现场看管人，不复制、不释放、不重生")
	check(f.state.facts[receipt].custody_transfers==[{"person_id":person,"from_army_id":old_keeper,"to_army_id":f.army_a}],"看管改变记入唯一占领移交清单")
	var stable: Dictionary=Assets.validate_state(f.state,f.data); check(stable.ok,"留营实俘移交后五类与看管并集仍守恒："+str(stable))

## 同次账目同时出现初始和新归属；逐次改坏同一对象，证明局部复用不跨世界、日期或目录。
func check_supply_ownership_scope(source: Dictionary, receipt: String) -> void:
	var f: Dictionary=source.duplicate(true)
	check(Assets.validate_state(f.state,f.data).ok,"归属复用前完整旧日账可验证")
	var supply: Dictionary=Assets.step(f.state,f.data,f.state.tick+1)
	check(supply.ok,"易帜后真正结算下一半日供粮")
	if not supply.ok: return
	f.state.tick+=1
	var old_id: String=""; var new_id: String=""; var mouth_id: String=""
	for id in f.state.facts:
		var fact: Dictionary=f.state.facts[id]
		if fact.get("kind")!="military_supply": continue
		if not fact.mouth_ids.is_empty(): mouth_id=id
		if fact.garrison_id!=f.enemy_camp: continue
		if fact.ownership_id.is_empty(): old_id=id
		if fact.ownership_id==receipt: new_id=id
	check(not old_id.is_empty() and not new_id.is_empty() and not mouth_id.is_empty(),"同一驻军账内保留初始归属、新占领归属和真实吃粮名单")
	if old_id.is_empty() or new_id.is_empty() or mouth_id.is_empty(): return
	var state_before: Dictionary=f.state.duplicate(true); var data_before: Dictionary=f.data.duplicate(true)
	check(Assets.validate_state(f.state,f.data).ok and f.state==state_before and f.data==data_before,"同次按驻军和来源双键读取，完整世界与目录均不变")
	var known_members: Dictionary={}
	for actor_id in f.state.actors: known_members[actor_id]=true
	for camp in f.data.garrisons.values():
		for unit in camp.units:
			for index in range(unit.initial_soldiers): known_members["soldier:"+unit.id+":"+str(index+1)]=true
	var expected_codes: Dictionary={"unknown_ownership":"OccupationHistoryMissing","wrong_organization":"MilitarySupplyOrganizationMismatch","missing_ownership":"MilitarySupplyOwnershipMissing","duplicate_supply":"DuplicateMilitarySupply","duplicate_mouth":"InvalidMilitarySupplyFact","unknown_mouth":"UnknownMilitarySupplyMouth","missing_receipt":"OccupationReceiptRequired","cycle":"OccupationChainCycle","changed_catalog":"MilitarySupplyAccountingMismatch"}
	for mode in expected_codes:
		# 先用同一个对象正常查询，再原地破坏；跨次缓存会遮住历史变化。
		var bad: Dictionary=f.duplicate(true)
		check(Assets.validate_state(bad.state,bad.data).ok,"跨次污染前先接受同一完整对象："+mode)
		check(Assets._validate_receipts(bad.state,bad.data,known_members).ok,"直接热查历史叶，避免外层检查掩盖跨次复用："+mode)
		match mode:
			"unknown_ownership": bad.state.facts[new_id].ownership_id="military:999999"
			"wrong_organization": bad.state.facts[old_id].organization_id=bad.state.facts[new_id].organization_id+".invented"
			"missing_ownership": bad.state.facts[old_id].erase("ownership_id")
			"duplicate_supply": bad.state.facts[Factory.next_id(bad.state,"military")]=bad.state.facts[old_id].duplicate(true)
			"duplicate_mouth": bad.state.facts[mouth_id].mouth_ids.append(bad.state.facts[mouth_id].mouth_ids[0])
			"unknown_mouth": bad.state.facts[mouth_id].mouth_ids[0]="soldier:missing:1"
			"missing_receipt": bad.state.facts.erase(receipt)
			"cycle": bad.state.facts[receipt].previous_occupation_id=receipt
			"changed_catalog": bad.data.garrisons[bad.enemy_camp].ration_per_person+=1
		var before_state: Dictionary=bad.state.duplicate(true); var before_data: Dictionary=bad.data.duplicate(true)
		var rejected: Dictionary=Assets.validate_state(bad.state,bad.data)
		check(not rejected.ok and rejected.get("code")==expected_codes[mode] and bad.state==before_state and bad.data==before_data,"旧历史损坏保留首错且输入完整不变："+mode+" "+str(rejected))
		var direct: Dictionary=Assets._validate_receipts(bad.state,bad.data,known_members)
		check(direct==rejected and bad.state==before_state and bad.data==before_data,"历史叶本身重新读取坏来源并保留完整错误："+mode+" "+str(direct))
		check(Assets.validate_state(f.state,f.data).ok,"坏账拒绝不会污染下一世界的同名驻军："+mode)
