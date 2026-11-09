## 真实会战收据历史窄门：复用任官编组与原供给，不构造完整野战数值模型或宣称全链发行已验。
extends SceneTree
const F=preload("res://tests/military_combat_fixture.gd")
const Combat=preload("res://core/military_combat.gd")
const Assets=preload("res://core/military_assets.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0

## 引擎开始后再建立独立轻量规则世界，不启动图形或读取玩家存档。
func _init() -> void:
	call_deferred("run")

## 每个失败保留诊断并使最终进程失败，不把部分检查数当作通过。
func check(ok: bool, label: String) -> void:
	checks+=1
	if not ok: failures+=1; printerr("FAIL: ",label)

## 双方通过原工厂、真实任官及编组形成军团；仅宏观抵达由既有夹具边界提供。
func prepared(second: bool=false) -> Dictionary:
	var f: Dictionary=F.fresh(); check(f.initialized.ok,"真实工厂军事初始化")
	var a: Dictionary=F.mobilize(f,false,second); check(a.ok and a.get("success",false),"己方实际任官受令编组："+str(a))
	var b: Dictionary=F.mobilize(f,true); check(b.ok and b.get("success",false),"敌方同源任官受令编组："+str(b))
	f.army_a=a.get("details",{}).get("army_id",""); f.army_b=b.get("details",{}).get("army_id","")
	check(Assets.validate_state(f.state,f.data).ok,"编组后真实人产可保存")
	return f

## 使用同一永久接敌来源建局，全部单位沿正式预先保存的AI执行。
func begin(f: Dictionary, controller: String="", second: bool=false) -> String:
	var ids: Array=[f.unit,f.second_unit,f.enemy_unit] if second else [f.unit,f.enemy_unit]
	var request: Dictionary=F.contact(f,[f.army_a,f.army_b],ids,controller)
	var created: Dictionary=Combat.create(f.state,f.data,request)
	check(created.ok,"以真实位置和接敌事实建会战："+str(created))
	return created.get("details",{}).get("battle_id","")

## 只覆盖本修复的历史拒绝、正常续轮、零时长收尾和半日末收尾，不跑无关军事矩阵。
func run() -> void:
	check_round_receipt_history()
	check_termination_history()
	print("MILITARY_RECEIPT_HISTORY_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)

## 真实首轮的永久来源必须与轮号、半日和接敌身份一致；损坏历史不能靠合法末态或保存AI蒙混恢复。
func check_round_receipt_history() -> void:
	var f: Dictionary=prepared(false); var id: String=begin(f,"",false)
	var advanced: Dictionary=F.advance(f)
	check(advanced.ok and f.state.battles[id].phase=="planning","真实自动首轮生成一份持续作战收据")
	if not advanced.ok or f.state.battles[id].phase!="planning": return
	var battle: Dictionary=f.state.battles[id]
	var receipt_id: String=battle.round_receipt_ids[0]
	var source_id: String=f.state.facts[receipt_id].battle_id
	var source: Dictionary=f.state.facts[source_id]
	var corruptions: Array=[
		["round",source.round+1,"不能更改已消费轮号"],
		["start_tick",source.tick,"普通轮次不能伪装零半日"],
		["contact_source_id","invented","不能替换永久接敌来源"],
		["army_ids",[],"不能删除当轮真实参战军团"],
		["source_id","invented","中间轮次不能引用另一结算来源"],
		["final",true,"持续作战收据不能提前标记终局"],
		["result",{"winner_side":0,"reason":"invented"},"中间轮次不能夹带终局战果"],
		["winner_side",0,"中间轮次不能预先宣告获胜"],
		["kind","invented","收据不能引用另一种世界事实"],
		["phase","started","未完成事实不能冒充已经落账"]]
	for row in corruptions:
		var broken: Dictionary=f.state.duplicate(true); broken.facts[source_id][row[0]]=row[1]
		var before: Dictionary=broken.duplicate(true)
		check(not Combat.validate_state(broken,f.data).ok and broken==before,row[2]+"且拒绝校验只读")
		check(not Combat.preview_step(broken,f.data,broken.tick+1).ok and broken==before,"坏历史在任何新口粮/轮次前拒绝："+row[0])
	var shifted: Dictionary=f.state.duplicate(true)
	shifted.facts[source_id].tick=battle.created_tick; shifted.facts[source_id].start_tick=battle.created_tick
	shifted.facts[receipt_id].tick=battle.created_tick
	check(not Combat.validate_state(shifted,f.data).ok,"同时回拨来源与落账tick也不能删掉真实已过半日")
	for field in ["round","final","army_ids","unit_ids","contact_source_id","source_id","result","winner_side","tick","start_tick","session_id","phase","kind"]:
		for wrong in [null,false,1.5,"",[],{}]:
			if typeof(wrong)==typeof(source[field]) and wrong==source[field]: continue
			var broken: Dictionary=f.state.duplicate(true); broken.facts[source_id][field]=wrong
			var before: Dictionary=broken.duplicate(true)
			var answer: Dictionary=Combat.validate_state(broken,f.data)
			check(answer.get("ok",true)==false and broken==before,"错类型来源必须返回明确拒绝，不抛脚本错误："+field+"/"+str(typeof(wrong)))
	for field in ["tick","kind","battle_id","units"]:
		for wrong in [null,false,1.5,"",[],{}]:
			var broken: Dictionary=f.state.duplicate(true); broken.facts[receipt_id][field]=wrong
			var before: Dictionary=broken.duplicate(true)
			var answer: Dictionary=Combat.validate_state(broken,f.data)
			check(answer.get("ok",true)==false and broken==before,"错类型收据必须返回明确拒绝，不抛脚本错误："+field+"/"+str(typeof(wrong)))
	var stored: Dictionary=Strict.parse_text(JSON.stringify(f.state))
	check(stored.ok and Combat.validate_state(stored.value,f.data).ok,"未改写的真实轮次历史经严格JSON恢复")
	var restored: Dictionary=f.duplicate(true); restored.state=stored.value
	var saved_ai: Dictionary=restored.state.battles[id].ai_plans.duplicate(true)
	check(Combat.validate_state(restored.state,restored.data).ok and restored.state.battles[id].ai_plans==saved_ai,"历史核验保留原AI意图而不重算")
	var left: Dictionary=F.advance(f); var right: Dictionary=F.advance(restored)
	check(left.ok and right.ok and f.state==restored.state,"同源历史恢复后下一轮整态及随机流一致")

## 外部结束可在当前边界或已消费的半日末封账，保留原round与last_tick，不凭空追加攻击或重算AI。
func check_termination_history() -> void:
	for elapsed in [0,1]:
		var f: Dictionary=prepared(); var id: String=begin(f)
		if elapsed==1: check(F.advance(f).ok,"收尾前原会战真实推进一轮")
		var before_tick: int=f.state.tick
		var ended: Dictionary=Combat.terminate(f.state,f.data,id,"truce")
		check(ended.ok and f.state.tick==before_tick and Combat.validate_state(f.state,f.data).ok and Assets.validate_state(f.state,f.data).ok,"当前边界零半日收尾保留合法永久历史")
		var before: Dictionary=f.state.duplicate(true)
		check(Combat.terminate(f.state,f.data,id,"truce").get("already_completed",false) and f.state==before,"重复结束保留完整状态/随机/实例号")
	var f: Dictionary=prepared(); var id: String=begin(f)
	var finish_tick: int=f.state.tick+1
	check(Assets.step(f.state,f.data,finish_tick).ok,"半日末停止前只消费原有口粮")
	var ended: Dictionary=Combat.terminate(f.state,f.data,id,"cancelled",finish_tick)
	f.state.tick=finish_tick
	check(ended.ok and Combat.validate_state(f.state,f.data).ok and Assets.validate_state(f.state,f.data).ok,"半日末外部取消可以封账而不伪造已经攻击一轮")
	f=prepared(); id=begin(f)
	var limit: int=0
	while f.state.battles[id].phase=="planning" and limit<20:
		var stepped: Dictionary=F.advance(f)
		check(stepped.ok,"正常会战按原AI继续真实半日")
		if not stepped.ok: return
		limit+=1
	check(f.state.battles[id].phase=="closed" and Combat.validate_state(f.state,f.data).ok and Assets.validate_state(f.state,f.data).ok,"正常终局最后一轮仍有真实半日与同一人产落账")
