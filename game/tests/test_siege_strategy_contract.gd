## 真实Kernel的围城战略输入合同短门；复用正式作者与离城准备，不推导整编迁移规则。
extends "res://tests/test_siege_safe_departure.gd"

## 未支持字段的拒绝、重试与冷恢复必须整态不变；省略这些字段仍沿原合法条款执行。
func check_rejected(k, prepared: Dictionary, command: Dictionary, field: String) -> void:
	var before: Dictionary=k.snapshot()
	var tested=Kernel.new()
	if not check(tested.new_game("scenario.alpha_life",content_path).ok and tested.restore(before).ok,"拒绝支路恢复真实世界："+field): return
	var params: Dictionary={"siege_id":prepared.siege_id,"command":command}
	var quote: Dictionary=tested.preview("siege_strategy",params)
	check(not quote.ok and quote.get("code")=="UnexpectedSiegeStrategicField" and quote.get("path")==field,"预览明确拒绝未支持字段："+field)
	check(tested.snapshot()==before,"拒绝预览保留世界与RNG："+field)
	var request: Dictionary={"kind":"siege_strategy","params":params,"actor_id":before.player_actor_id,"request_seq":before.request_sequences[before.player_actor_id]+1,"expected_revision":before.revision}
	var rejected: Dictionary=tested.submit(request)
	check(not rejected.ok and rejected.get("code")=="UnexpectedSiegeStrategicField" and rejected.get("path")==field,"实际提交明确拒绝未支持字段："+field+" "+JSON.stringify(rejected))
	check(tested.snapshot()==before,"拒绝完整保留资产、历史、占用、序号与RNG："+field)
	check(tested.submit(request)==rejected and tested.snapshot()==before,"同失败请求重放不移交公库或消耗序号："+field)
	var restored=Kernel.new()
	var opened: Dictionary=restored.new_game("scenario.alpha_life",content_path)
	var decoded: Dictionary=Codec.decode(Codec.encode(before,0,"siege-strategy-rejected",k.content()),k.content())
	if not check(opened.ok and decoded.ok and restored.restore(decoded.state).ok,"拒绝前世界完整Codec冷恢复："+field): return
	check(restored.submit(request)==rejected and restored.snapshot()==before,"冷恢复后同失败请求仍拒绝且整态不变："+field)

## 原权限、条款及准备条件先于额外字段错误，不让新守卫改变既有首错。
func check_error_priority(k, prepared: Dictionary) -> void:
	var before: Dictionary=k.snapshot(); var id: String=prepared.siege_id
	var params: Dictionary={"siege_id":id,"command":{"kind":"negotiate","terms":"unknown","recipient_army_id":"forged"}}
	check(k.preview("siege_strategy",params).get("code")=="UnknownSiegeTerms","未知条款保持原错误优先级")
	params.command={"kind":"withdraw","army_id":"army.unknown","terms":"integrate"}
	check(k.preview("siege_strategy",params).get("code")=="SiegeArmyCommandRequired","无指挥权保持原错误优先级")
	params.command={"kind":"unknown","recipient_army_id":"forged"}
	check(k.preview("siege_strategy",params).get("code")=="UnknownSiegeStrategicCommand","未知战略命令保持原错误优先级")
	var obs: Dictionary=SiegeWorld.observe(before,k.content().data,before.sieges[id])
	var rejected: Dictionary=obs.duplicate(true); rejected.negotiation.willingness=0
	var result: Dictionary=Siege.strategic(before.sieges[id],k.content().data,rejected,before.player_actor_id,{"kind":"negotiate","terms":"integrate","consent":true})
	check(result.get("code")=="SiegeTermsRejected","额外同意声明不能遮盖观察边界中的实际拒绝")
	check(k.snapshot()==before,"首错检查不改真实世界")

## 每个合法命令使用同一正式世界的独立冷恢复支路，检验拒绝后仍可用原序号提交原条款。
func check_legal(k, prepared: Dictionary, command: Dictionary) -> void:
	var before: Dictionary=k.snapshot(); var restored=Kernel.new()
	if not check(restored.new_game("scenario.alpha_life",content_path).ok and restored.restore(before).ok,"合法战略支路恢复真实世界"): return
	var params: Dictionary={"siege_id":prepared.siege_id,"command":command}
	check(restored.preview("siege_strategy",params).ok,"既有合法战略预览保留："+JSON.stringify(command))
	var request: Dictionary={"kind":"siege_strategy","params":params,"actor_id":before.player_actor_id,"request_seq":before.request_sequences[before.player_actor_id]+1,"expected_revision":before.revision}
	var unsupported: Dictionary=request.duplicate(true); unsupported.params.command.unsupported=true
	var rejected: Dictionary=restored.submit(unsupported)
	check(rejected.get("code")=="UnexpectedSiegeStrategicField" and restored.snapshot()==before,"先拒绝额外声明且原序号仍可使用")
	var result: Dictionary=restored.submit(request)
	if not check(result.ok,"既有合法战略提交保留："+JSON.stringify(command)+" "+JSON.stringify(result)): return
	var after: Dictionary=restored.snapshot()
	check(restored.submit(request)==result and restored.snapshot()==after,"合法战略同请求重放不重复资产交接")
	var decoded: Dictionary=Codec.decode(Codec.encode(after,0,"siege-strategy-legal",restored.content()),restored.content())
	check(decoded.ok and restored.restore(decoded.state).ok and restored.snapshot()==after,"既有合法战略结果完整保存恢复")
	if command.get("terms")=="integrate":
		# 只冻结现有交城语义作为相邻回归，不把兵员仍属原军宣称为整编完成。
		check(after.strongholds["stronghold.military.b"].owner_organization_id=="org.military.a","既有integrate公共占领行为未变")
		check(after.military_units==before.military_units and after.campaigns[prepared.campaigns.b].status=="operating","本片未隐式新增兵员迁移或旧军令关闭规则")
	check(k.snapshot()==before,"独立合法支路不污染待拒绝的原世界")

## 正式作者短链覆盖两支守军，不造军团、战果或占领状态；只检验战略输入合同。
func run() -> void:
	if not ProjectSettings.get_setting("application/config/siege_departure_isolated",false): quit(1); return
	content_path=ProjectSettings.globalize_path("res://").trim_suffix("/").get_base_dir().path_join("build/safe-departure.hmcatalog.json")
	var k=Kernel.new(); var opened: Dictionary=k.new_game("scenario.alpha_life",content_path)
	if not check(opened.ok,"正式作者准备后的真实开局 "+JSON.stringify(opened)): finish(); return
	var prepared: Dictionary=prepare(k)
	if prepared.is_empty(): finish(); return
	var before: Dictionary=k.snapshot()
	var army_id: String=before.campaigns[prepared.campaigns.a].army_id
	var declarations: Dictionary={"recipient_army_id":army_id,"unit_ids":["unit.military.b"],"consent":true,"defender_actor_id":prepared.defender,"army_id":army_id}
	for terms in ["integrate","safe_departure","truce"]:
		for field in declarations:
			var command: Dictionary={"kind":"negotiate","terms":terms}
			command[field]=declarations[field]
			check_rejected(k,prepared,command,field)
	var withdrawal: Dictionary={"terms":"truce","army_ids":[army_id],"consent":true}
	for field in withdrawal:
		var command: Dictionary={"kind":"withdraw","army_id":army_id}
		command[field]=withdrawal[field]
		check_rejected(k,prepared,command,field)
	check_error_priority(k,prepared)
	for terms in ["integrate","safe_departure","truce"]: check_legal(k,prepared,{"kind":"negotiate","terms":terms})
	check_legal(k,prepared,{"kind":"withdraw","army_id":army_id})
	print("SIEGE_STRATEGY_CONTRACT checks=",checks," failures=",failures)
	quit(0 if failures==0 else 1)
