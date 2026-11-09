## 正式军阵目录经唯一Kernel验证忙碌输入、随机、首课系数及v2；不重复纯叶棋盘或训练矩阵。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Codec=preload("res://core/save_codec.gd")
const Interaction=preload("res://core/interaction_rules.gd")
var checks: int=0
var failures: int=0
const UNIT="base::unit.study"
const OFFICE="base::office.study"
const PAY="base::wallet.camp"

## 延迟执行，使SceneTree完成初始化；仅准备器隔离工程可运行此门。
func _init() -> void:
	call_deferred("run")

## 保留首错并把每项断言纳入实际退出码。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 只用正式请求修订和序号推进，不直接注入题面/行动或训练结果。
func command(k, kind: String, params: Dictionary={}) -> Dictionary:
	var state: Dictionary=k.snapshot();var actor: String=state.player_actor_id
	return {"kind":kind,"params":params,"actor_id":actor,"expected_revision":state.revision,"request_seq":state.request_sequences[actor]+1}

## 四步手动题面中途冷恢复，首课后拒绝改分，结课再验证零课退出和取消的闭合。
func run() -> void:
	var index: Dictionary=JSON.parse_string(FileAccess.get_file_as_string("res://data/generated/installed-content.json"))
	var path: String="res://data/generated/"+index.sets[0].directory+"/catalog.hmcatalog.json"
	var k=Kernel.new();var made: Dictionary=k.new_game("base::scenario.procurement",path)
	if not check(made.ok,"正式typed军阵新锁启动 "+str(made)): finish();return
	var state: Dictionary=k.snapshot();var actor: String=state.player_actor_id
	var context: Dictionary=Interaction.for_role(state,k.content().data,"base::role.office")
	if not check(k.submit(command(k,"office_accept",{"office_id":OFFICE,"context":context})).ok,"真实接受军务任命"): finish();return
	if not check(k.submit(command(k,"advance")).ok,"原半日任职完成"): finish();return
	var before: Dictionary=k.snapshot()
	var begun: Dictionary=k.submit(command(k,"military_train",{"office_id":OFFICE,"unit_id":UNIT,"challenge_mode":"manual"}))
	if not check(begun.ok,"真实军阵训练行动 "+str(begun)): finish();return
	state=k.snapshot();var id: String=state.actors[actor].action_id;var rng: Dictionary=state.rng.duplicate(true);var tick: int=state.tick
	check(rng.minigame!=before.rng.minigame and state.wallets[PAY].balance==500 and state.wallets[PAY].reserved[id]==150,"只首次抽种与预留，不预发训练")
	var shown: Dictionary=k.military_drill(actor,id)
	if not check(shown.ok and shown.step==1 and not shown.has("solution"),"公开题面不含作者解证据"): finish();return
	var moves: Array=[{"group_id":"guard","target":[1,0]},{"group_id":"horse","target":[4,1]},{"group_id":"ranged","target":[2,2]},{"group_id":"guard","target":[2,1]}]
	for number in range(4):
		var drafted: Dictionary=k.submit(command(k,"military_drill_draft",{"action_id":id,"expected_step":number+1,"group_id":moves[number].group_id,"target":moves[number].target}))
		if not check(drafted.ok,"实际忙碌草案 "+str(drafted)): finish();return
		shown=k.military_drill(actor,id)
		if number==0:
			var cancelled: Dictionary=k.submit(command(k,"military_drill_cancel_pending",{"action_id":id,"expected_step":1,"expected_pending":shown.pending}))
			if not check(cancelled.ok and k.military_drill(actor,id).pending==null,"撤销未确认输入不改轨迹"): finish();return
			if not check(k.submit(command(k,"military_drill_draft",{"action_id":id,"expected_step":1,"group_id":"guard","target":[1,0]})).ok,"合法重新选择"): finish();return
			shown=k.military_drill(actor,id)
		if number==1:
			var saved: Dictionary=k.save_context();var decoded: Dictionary=Codec.decode(Codec.encode(saved.state,1,"drill-pending",saved.catalog,saved.registry),saved.catalog,saved.registry)
			if not check(decoded.ok and decoded.state==k.snapshot(),"pending草案及种子全Schema v2 "+str(decoded.get("code",""))): finish();return
			var recovered=Kernel.new();var restored: Dictionary=recovered.restore_with_catalog(decoded.state,saved.catalog,saved.registry)
			if not check(restored.ok,"另一Kernel精确恢复同题面"): finish();return
			k=recovered
		var request: Dictionary=command(k,"military_drill_confirm",{"action_id":id,"expected_step":number+1,"expected_pending":shown.pending})
		var confirmed: Dictionary=k.submit(request)
		if not check(confirmed.ok,"真实确认一步 "+str(confirmed)): finish();return
		state=k.snapshot();check(k.submit(request)==confirmed and k.snapshot()==state,"相同请求不会二确认或重抽")
		check(state.rng==rng and state.tick==tick and state.military_units[UNIT].training==10,"选择不推进世界或真实训练")
	var response: Dictionary=k.submit(command(k,"advance"))
	if not check(response.ok,"真实供给后首课消费固定军阵系数 "+str(response)): finish();return
	state=k.snapshot();check(state.facts[id].drill.phase=="frozen" and state.facts[id].drill.performance_bp==12000 and state.wallets[PAY].balance==463,"首课仅固定一次120%且支付37原价")
	var rejected: Dictionary=k.submit(command(k,"military_drill_exit",{"action_id":id}))
	check(not rejected.ok and k.snapshot()==state,"首课后不能越busy门重选系数")
	for number in range(3):
		if not check(k.submit(command(k,"advance")).ok,"后续真实课程 "+str(number)): finish();return
	state=k.snapshot();check(state.military_units[UNIT].training==13 and state.military_units[UNIT].fatigue==16 and state.wallets[PAY].balance==350,"正常累计3再乘120%取整，不乘费用/疲劳")
	var saved: Dictionary=k.save_context();var decoded: Dictionary=Codec.decode(Codec.encode(saved.state,2,"drill-done",saved.catalog,saved.registry),saved.catalog,saved.registry)
	check(decoded.ok and decoded.state==state,"完成军阵历史全Schema v2")
	if not check(k.submit(command(k,"military_train",{"office_id":OFFICE,"unit_id":UNIT,"challenge_mode":"manual"})).ok,"开始另一真实课程"): finish();return
	id=k.snapshot().actors[actor].action_id
	check(k.submit(command(k,"military_drill_exit",{"action_id":id})).ok,"零课主动退出题面不取消课程")
	if not check(k.submit(command(k,"military_train_cancel",{"action_id":id})).ok,"正式取消释放剩余费用与真人/部队"): finish();return
	state=k.snapshot();check(state.facts[id].drill.phase=="closed" and state.actions.is_empty() and state.wallets[PAY].reserved.is_empty() and state.wallets[PAY].balance==350,"零课题面封存不收费/训练")
	finish()

## 单一接缝结论独立输出，不将纯叶检查次数叠加或宣称图形验收。
func finish() -> void:
	print("MILITARY_DRILL_KERNEL_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
