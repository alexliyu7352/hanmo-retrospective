## 条件纯叶窄门：真实课程只借原训练夹具；行军/休整仅数学投影，不写世界或宣称生产者已经接线。
extends "res://tests/test_military_training.gd"
const History=preload("res://core/domains/military/condition_history.gd")

## 只运行纯投影与真实训练历史的受影响边界，不触发GUI、全世界回归或新活动生产。
func _run() -> void:
	projection_cases()
	real_lesson_fold_cases()
	malformed_projection_cases()
	print("MILITARY_CONDITION_HISTORY_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)

## 文档只给行军加3与合格休整减10；训练原值不变，未知战斗/缺粮系数不得被默认接受。
func projection_cases() -> void:
	for row in [[0,"march",3],[98,"march",100],[100,"march",100],[13,"qualified_garrison_rest",3],[3,"qualified_garrison_rest",0],[0,"qualified_garrison_rest",0]]:
		var profile: Dictionary={"training":23,"fatigue":row[0]}; var before: Dictionary=profile.duplicate(true)
		var result: Dictionary=History.project_profile(profile,row[1])
		check(result=={"ok":true,"profile":{"training":23,"fatigue":row[2]}} and profile==before,"明定条件增减、上下限与只读："+str(row))
	for kind in ["battle","shortage","rest","wait","defend",null,true,3,{}]:
		check(not History.project_profile({"training":23,"fatigue":10},kind).ok,"未批准活动或错类型不能取默认速率："+str(kind))
	for profile in [{},{"training":10},{"training":10,"fatigue":0.0},{"training":true,"fatigue":0},{"training":101,"fatigue":0},{"training":10,"fatigue":-1},{"training":10,"fatigue":0,"validated":true},null,[]]:
		check(not History.project_profile(profile,"march").ok,"纯投影拒绝不完整或自报有效的profile："+str(profile))

## 测试用活动投影显式引用预期来源；只用于纯折叠输入，不分配世界ID或制造真实执行凭证。
func projected(unit_id: String, tick: int, kind: String, before: Dictionary, source_id: String) -> Dictionary:
	var after: Dictionary=History.project_profile(before,kind).profile
	return {"unit_id":unit_id,"tick":tick,"kind":kind,"source":{"kind":"executed_movement" if kind=="march" else "qualified_garrison_rest","id":source_id},"training_before":before.training,"training_after":after.training,"fatigue_before":before.fatigue,"fatigue_after":after.fatigue}

## 原任职、两课、取消及下一课程均走现owner；纯叶只读原fact.lessons，活动投影不会混进生产历史。
func real_lesson_fold_cases() -> void:
	var f: Dictionary=fresh(10,5); commission(f); var action: Dictionary=start(f)
	if action.is_empty(): return
	advance(f,action); advance(f,action)
	check(Training.cancel(f.state,f.data,f.actor,action.id).ok,"原课程两课后正常取消")
	Step.release(f.state,action)
	var first: Dictionary=f.state.facts[action.id]
	var sources: Array=[{"action_id":action.id,"fact":first}]
	var original: Dictionary=f.state.duplicate(true)
	var profile: Dictionary={"training":f.state.military_units[f.unit].training,"fatigue":f.state.military_units[f.unit].fatigue}
	var result: Dictionary=History.fold(f.unit,{"training":10,"fatigue":5},sources,[],f.state.tick)
	check(result=={"ok":true,"profile":profile} and f.state==original,"共用折叠从原真实课时得到相同profile且整世界不变")
	var march: Dictionary=projected(f.unit,f.state.tick+1,"march",profile,"projection.march")
	var after_march: Dictionary={"training":march.training_after,"fatigue":march.fatigue_after}
	var rest: Dictionary=projected(f.unit,f.state.tick+2,"qualified_garrison_rest",after_march,"projection.rest")
	var activities: Array=[rest,march]; var activities_before: Array=activities.duplicate(true)
	result=History.fold(f.unit,{"training":10,"fatigue":5},sources,activities,f.state.tick+2)
	check(result.ok and result.profile=={"training":profile.training,"fatigue":6},"原两课疲劳13后纯行军16再纯休整6，输入次序不影响时序")
	check(f.state==original and activities==activities_before,"纯合并不排序调用者数组、不抄写第二日记、不改实际疲劳")
	check(Training.validate_state(f.state,f.data).ok and f.state.military_units[f.unit].fatigue==13,"生产校验仍只消费真实训练，活动投影未启用")
	var broken: Array=activities.duplicate(true); broken[0].fatigue_before+=1; broken[0].fatigue_after+=1
	check(History.fold(f.unit,{"training":10,"fatigue":5},sources,broken,f.state.tick+2).code=="MilitaryConditionHistoryMismatch","各自算术合法但前后链断裂的活动不能通过")
	broken=[projected(f.unit,first.lessons[-1].tick,"march",profile,"projection.overlap")]
	check(History.fold(f.unit,{"training":10,"fatigue":5},sources,broken,f.state.tick).code=="MilitaryConditionHistoryTimeMismatch","训练与另一活动不能共占同一半日")
	check(History.fold(f.unit,{"training":10,"fatigue":5},sources,[march],f.state.tick).code=="MilitaryConditionHistoryTimeMismatch","未来投影不能进入较早稳定边界")
	var next_action: Dictionary=start(f)
	if next_action.is_empty(): return
	advance(f,next_action)
	sources.append({"action_id":next_action.id,"fact":f.state.facts[next_action.id]})
	result=History.fold(f.unit,{"training":10,"fatigue":5},sources,[],f.state.tick)
	check(result.ok and result.profile.training==f.state.military_units[f.unit].training and result.profile.fatigue==17 and Training.validate_state(f.state,f.data).ok,"下一真实课程沿原13起点取得17，没有吃到数学投影的6")
	var saved: Dictionary=Strict.parse_text(JSON.stringify(f.state))
	check(saved.ok and Training.validate_state(saved.value,f.data).ok,"新折叠器仍接受原真实训练存档")
	var loaded: Dictionary=f.duplicate(true); loaded.state=saved.value
	advance(f,next_action); advance(loaded,loaded.state.actions[next_action.id])
	check(f.state==loaded.state,"原课程与严格JSON恢复后同命令续课整态相同")
	var changed: Dictionary=f.state.duplicate(true); changed.military_units[f.unit].fatigue+=1
	check(Training.validate_state(changed,f.data).code=="MilitaryTrainingConditionMismatch","活动课程原首错保留，不能无源改变疲劳")

## 投影来源采用有限形状与绑定检查；不把validated布尔、另一部或重复来源当成实际事实证明。
func malformed_projection_cases() -> void:
	var initial: Dictionary={"training":12,"fatigue":20}
	var row: Dictionary=projected("unit.test",1,"march",initial,"projection.one")
	var valid: Dictionary=History.fold("unit.test",initial,[],[row],1)
	check(valid.ok and valid.profile=={"training":12,"fatigue":23},"独立活动纯折叠按同一数学接口核前后值")
	for field in ["unit_id","tick","kind","source","training_before","training_after","fatigue_before","fatigue_after"]:
		for wrong in [null,false,1.5,[],{}]:
			var bad: Dictionary=row.duplicate(true); bad[field]=wrong
			var before: Dictionary=bad.duplicate(true)
			check(not History.fold("unit.test",initial,[],[bad],1).ok and bad==before,"坏活动字段明确拒绝且只读："+field+"/"+str(typeof(wrong)))
	for bad_source in [{},{"validated":true},{"kind":"executed_movement","id":true},{"kind":[],"id":"x"},{"kind":"qualified_garrison_rest","id":"x"},{"kind":"executed_movement","id":""},{"kind":"executed_movement","id":"x","validated":true}]:
		var bad: Dictionary=row.duplicate(true); bad.source=bad_source
		check(not History.fold("unit.test",initial,[],[bad],1).ok,"坏来源或调用者布尔不能代替来源合同："+str(bad_source))
	var duplicate: Dictionary=projected("unit.test",2,"march",valid.profile,"projection.one")
	check(History.fold("unit.test",initial,[],[row,duplicate],2).code=="DuplicateMilitaryConditionSource","同一来源不能在后续tick再次兑现")
	var bad: Dictionary=row.duplicate(true); bad.validated=true
	check(not History.fold("unit.test",initial,[],[bad],1).ok,"活动不允许额外validated伪证明")
	bad=row.duplicate(true); bad.unit_id="unit.other"
	check(not History.fold("unit.test",initial,[],[bad],1).ok,"别部投影不能计入本部历史")
	bad=row.duplicate(true); bad.fatigue_after=24
	check(History.fold("unit.test",initial,[],[bad],1).code=="MilitaryConditionProjectionMismatch","调用者不能更改明定行军加3")
	for source in [{},{"action_id":"action.test","fact":{}},{"action_id":true,"fact":{}},{"validated":true,"action_id":"action.test","fact":{}}]:
		check(not History.fold("unit.test",initial,[source],[],1).ok,"原训练来源引用也必须有精确类型与单位绑定")
