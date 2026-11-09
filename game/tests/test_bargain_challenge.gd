## 议价纯叶局部门只核作者公开条件、三轮分数与上下文候选；不冒充市场、课程或交易已集成。
extends SceneTree
const Bargain=preload("res://core/domains/cultivation/bargain_challenge.gd")
const Etiquette=preload("res://core/domains/cultivation/etiquette_challenge.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0
var definition: Dictionary
var texts: Dictionary
const BINDING={"actor_id":"actor.student","source_id":"quote:17"}
const SNAPSHOT={"skill_level":2,"auxiliary_attribute":60}
const CONTEXT={"budget":650,"quantity":10,"relation_discount_bp":300,"quote":{"quote_id":"quote:17","quote_sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef","market_revision":7,"valid_tick":4}}

## 固定作者文件经真实严格JSON解析，不构造引擎资源或启动窗口。
func _init() -> void:
	definition=Strict.parse_text(FileAccess.get_file_as_string("res://../tests/fixtures/bargain_challenge/definition.json")).value
	texts=Strict.parse_text(FileAccess.get_file_as_string("res://../tests/fixtures/bargain_challenge/texts.json")).value
	call_deferred("run_checks")

## 局部断言必须保留标签及真实退出码，脚本错误不能被零失败计数掩盖。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 创建固定相同上下文和seed，不代表调用方已获得正式报价资格。
func create(mode: String="manual", context: Dictionary=CONTEXT) -> Dictionary:
	return Bargain.create(definition,texts,BINDING,SNAPSHOT,context,"987654",mode).state

## 逐轮选择使用显式round ID，便于验证重放和轮次越权拒绝。
func play(options: Array) -> Dictionary:
	var state: Dictionary=create()
	for index in range(options.size()):
		var result: Dictionary=Bargain.choose(definition,texts,state,definition.rounds[index].id,options[index])
		if not check(result.ok,"当前轮合法选择 "+str(result)): return {}
		state=result.state
	return state

## 各类未知字段、脚本、条件类型及非P1方法都必须在作者校验层拒绝。
func authoring() -> void:
	check(Bargain.validate_definition(definition,texts).get("maximum_raw_score")==100,"显式题面理论最高100")
	var bad: Dictionary=definition.duplicate(true); bad.script="return 100"
	check(not Bargain.validate_definition(bad,texts).ok,"拒绝题面脚本")
	bad=definition.duplicate(true); bad.conditions[0].matched=1
	check(not Bargain.validate_definition(bad,texts).ok,"匹配条件不能用整数冒充布尔")
	bad=definition.duplicate(true); bad.conditions[1].id=bad.conditions[0].id
	check(not Bargain.validate_definition(bad,texts).ok,"公开条件ID唯一")
	bad=definition.duplicate(true); bad.rounds[0].options[0].supply_condition_id="cash"
	check(not Bargain.validate_definition(bad,texts).ok,"供需引用不能借已知条款类别加20")
	bad=definition.duplicate(true); bad.rounds[0].options[0].terms_condition_id="missing"
	check(not Bargain.validate_definition(bad,texts).ok,"未公开条件不能评分")
	bad=definition.duplicate(true); bad.rounds[0].options[0].points=100
	check(not Bargain.validate_definition(bad,texts).ok,"作者不能越过正式20/10自报分数")
	bad=definition.duplicate(true); bad.rounds[0].options[2].method="credit"
	check(not Bargain.validate_definition(bad,texts).ok,"拒绝赊欠或新增信用方法")
	bad=definition.duplicate(true); bad.rounds[0].options[1].kind="quantity"
	check(not Bargain.validate_definition(bad,texts).ok,"每轮数量/批量/付款各一项")
	bad=definition.duplicate(true); bad.rounds[0].options[1].id="small"
	check(not Bargain.validate_definition(bad,texts).ok,"同轮选项ID唯一")
	bad=definition.duplicate(true); bad.rounds[1].id=bad.rounds[0].id
	check(not Bargain.validate_definition(bad,texts).ok,"轮ID不能重复")
	bad=definition.duplicate(true); bad.rounds.pop_back()
	check(not Bargain.validate_definition(bad,texts).ok,"必须固定三轮")
	bad=definition.duplicate(true); bad.difficulty=1.0
	check(not Bargain.validate_definition(bad,texts).ok,"难度拒绝浮点")
	bad=definition.duplicate(true); bad.id+="\n"
	check(not Bargain.validate_definition(bad,texts).ok,"全ID严格拒绝尾换行")
	var missing: Dictionary=texts.duplicate(); missing.erase(definition.conditions[0].text_key)
	check(not Bargain.validate_definition(definition,missing).ok,"公开条件必须真实有显示文字")
	bad=definition.duplicate(true)
	for condition in bad.conditions: condition.matched=false
	check(Bargain.validate_definition(bad,texts).get("maximum_raw_score")==20,"最高分仅作作者诊断，不强迫可达100")

## 只按当前显式条件计分，关系和本次小游戏优惠合并一次，课程正常倍率不损失。
func scoring() -> void:
	var mixed: Dictionary=play(["small","batch","cash"])
	check(mixed.result.raw_score==80 and mixed.choices[0].points==30 and mixed.choices[1].points==20 and mixed.choices[2].points==10,"基础20加30/20/10严格来自公开条件")
	check(mixed.result.discount_bp==700 and mixed.result.relation_discount_bp==300 and mixed.result.challenge_discount_bp==400,"总优惠一次合并关系300和成绩400")
	var best: Dictionary=play(["small","small","small"])
	check(best.result.raw_score==100 and best.result.score==100 and best.result.discount_bp==800,"20+90封顶100，关系+500封顶800")
	check(best.result.performance_bp==12000 and Bargain.scaled_experience(definition,texts,best,13).xp==15,"经验候选独立封顶120%且整数向下取整")
	var automatic: Dictionary=create("automatic")
	check(automatic.result.score==59 and automatic.result.performance_bp==10000 and automatic.result.discount_bp==595,"自动成绩59，正常经验不损失，折扣按原式")
	check(automatic.result.auto_score==Etiquette.automatic_score(SNAPSHOT,1),"两小游戏只共用正式自动公式，没有套礼议基础25")
	var sparse: Dictionary=definition.duplicate(true)
	for condition in sparse.conditions: condition.matched=false
	var low: Dictionary=Bargain.create(sparse,texts,BINDING,SNAPSHOT,CONTEXT,"987654","manual").state
	for round_row in sparse.rounds: low=Bargain.choose(sparse,texts,low,round_row.id,"small").state
	check(low.result.raw_score==20 and low.result.score==49 and low.result.performance_bp==10000,"低分仍是auto-10下限且正常经验不降")
	var active: Dictionary=create()
	active=Bargain.choose(definition,texts,active,definition.rounds[0].id,"small").state
	var exited: Dictionary=Bargain.exit_challenge(definition,texts,active).state
	check(exited.result.score==automatic.result.score and exited.result.discount_bp==automatic.result.discount_bp and exited.choices.size()==1,"中途退出按同实例自动成绩保留原选择")
	check(Bargain.exit_challenge(definition,texts,best).state==best,"终态退出不重新叠加或降低原候选")
	check(Bargain.choose(definition,texts,best,definition.rounds[0].id,"small").get("code")=="BargainAlreadyCompleted","终态不能重开答题")

## 存读、完整上下文核对和候选副本验证绑定边界；真正消耗quote_id仍由经济owner负责。
func identity_and_context() -> void:
	var active: Dictionary=create(); var before: Dictionary=active.duplicate(true)
	var view: Dictionary=Bargain.view(definition,texts,active)
	check(view.ok and view.round==1 and view.budget==650 and view.quantity==10 and view.conditions.size()==4 and active==before,"只读显示当前题面与真实冻结预算数量")
	check(Bargain.choose(definition,texts,active,definition.rounds[1].id,"small").get("code")=="StaleBargainRound" and active==before,"不能跳轮或误用旧回复")
	check(not Bargain.result_for_context(definition,texts,active,BINDING,CONTEXT).ok,"未完成无可消费结果候选")
	active=Bargain.choose(definition,texts,active,definition.rounds[0].id,"small").state
	var restored: Dictionary=Strict.parse_text(JSON.stringify(active))
	check(restored.ok and restored.value==active and Bargain.validate_state(definition,texts,restored.value).ok,"课中/议价实例StrictJSON读回同态")
	var one: Dictionary=Bargain.choose(definition,texts,active,definition.rounds[1].id,"batch")
	var two: Dictionary=Bargain.choose(definition,texts,restored.value,definition.rounds[1].id,"batch")
	check(one==two and one.state.seed=="987654","恢复后同选择同结果且seed不变")
	var best: Dictionary=play(["small","small","small"])
	var candidate: Dictionary=Bargain.result_for_context(definition,texts,best,BINDING,CONTEXT)
	check(candidate.ok and candidate.result.discount_bp==800,"可信owner提供原完整上下文才返回总折扣候选")
	var changed: Dictionary=CONTEXT.duplicate(true); changed.quote.market_revision+=1
	check(Bargain.result_for_context(definition,texts,best,BINDING,changed).get("code")=="BargainContextChanged","当前市场版本变化与原候选不符")
	changed=CONTEXT.duplicate(true); changed.quote.quote_sha256="a".repeat(64)
	check(not Bargain.result_for_context(definition,texts,best,BINDING,changed).ok,"完整报价摘要不能被别的商品钱包方向替换")
	changed=CONTEXT.duplicate(true); changed.quantity=11
	check(not Bargain.result_for_context(definition,texts,best,BINDING,changed).ok,"原数量改变需重新正式报价")
	check(not Bargain.result_for_context(definition,texts,best,{"actor_id":"actor.other","source_id":"quote:17"},CONTEXT).ok,"候选不交给另一个人物")
	candidate.result.discount_bp=0
	check(best.result.discount_bp==800,"候选返回深副本不改变实例")
	changed=CONTEXT.duplicate(true); changed.quote.quote_id="quote:other"
	check(Bargain.create(definition,texts,BINDING,SNAPSHOT,changed,"987654","manual").get("code")=="BargainQuoteBindingMismatch","quote_id必须与来源同一")
	changed=CONTEXT.duplicate(true); changed.relation_discount_bp=801
	check(not Bargain.create(definition,texts,BINDING,SNAPSHOT,changed,"987654","manual").ok,"不接受超上限关系优惠")
	changed=CONTEXT.duplicate(true); changed.quote.quote_sha256="g".repeat(64)
	check(not Bargain.create(definition,texts,BINDING,SNAPSHOT,changed,"987654","manual").ok,"报价摘要仅64位小写hex")
	var changed_text: Dictionary=texts.duplicate(); changed_text[definition.conditions[0].text_key]+="变更"
	check(Bargain.validate_state(definition,changed_text,best).get("code")=="BargainSourceChanged","已公开条件文本变化使原源身份失效")
	var forged: Dictionary=best.duplicate(true); forged.result.discount_bp=801
	check(not Bargain.validate_state(definition,texts,forged).ok,"拒绝伪造终态折扣")
	forged=best.duplicate(true); forged.choices[0].points=31
	check(not Bargain.validate_state(definition,texts,forged).ok,"拒绝伪造选项分数")
	forged=best.duplicate(true); forged.result.score=100.0
	check(not Bargain.validate_state(definition,texts,forged).ok,"结果浮点不能冒充整数")
	forged=best.duplicate(true); forged.seed="0987654"
	check(not Bargain.validate_state(definition,texts,forged).ok,"seed拒绝非规范文本")
	changed=CONTEXT.duplicate(true); changed.quote=null
	check(Bargain.create(definition,texts,{"actor_id":"actor.student","source_id":"action:18"},SNAPSHOT,changed,"987654","automatic").ok,"课程题面允许无交易quote，不伪造已安装交易")

## 只执行新纯叶的局部正反例，军阵、世界经济及交易体验均未在此运行。
func run_checks() -> void:
	authoring(); scoring(); identity_and_context()
	print("BARGAIN_CHALLENGE_CHECKS=%d FAILURES=%d"%[checks,failures])
	quit(0 if failures==0 else 1)
