## 预备人口军籍页只消费当事人的有限DTO；训练、编组、口粮和退籍均由正式命令负责。
extends RefCounted
const STATES={"training":"受训中","trained_reserve":"受训预备","serving":"现役","wounded":"伤员","captive":"被俘","displaced":"离队"}

## 每条来源保留完整String键数据供当前控件和验收读取，界面不自行合成人口或授权。
static func render(host, parent: Control, view: Dictionary) -> void:
	host._text(parent,"预备人口与军籍",23)
	host._text(parent,"训练同意与服役同意分别登记。登记、退籍各需一个半日；出勤仍须按原军令编组并备足钱粮。",17)
	for row in view.get("rows",[]):
		var box=VBoxContainer.new(); box.name="PopulationMilitaryRow"; box.set_meta("population_military_row",row.duplicate(true)); box.add_theme_constant_override("separation",10); parent.add_child(box)
		host._text(box,row.get("unit_name",row["unit_id"])+" · "+row.get("garrison_name",row["garrison_id"]),21)
		var reserve_state: String=STATES.get(row.get("reserve_status",""),"尚未受训")
		host._text(box,"%s %d人 · 其中劳力%d人，可再分配%d人"%[reserve_state,row.get("reserve_count",0),row.get("reserve_labor_count",0),row.get("allocatable_labor_count",0)],18).name="PopulationReserveCount"
		host._text(box,"现役%d · 伤员%d · 被俘%d · 离队%d · 战殁%d"%[row.get("serving_count",0),row.get("wounded_count",0),row.get("captive_count",0),row.get("displaced_count",0),row.get("dead_count",0)],18).name="PopulationServiceCounts"
		host._text(box,"登记初始品质（暂定）：训练%d，士气%d，疲劳%d。"%[row.get("initial_training",0),row.get("initial_morale",0),row.get("initial_fatigue",0)],16)
		_wages(host,box,row.get("wages",[]))
		_food(host,box,row)
		if row.get("enroll_action") is Dictionary:
			var action: Dictionary=row["enroll_action"]
			host._text(box,"已另获服役许可，本次可登记%d人、%d名劳力。"%[action["params"]["count"],action["params"]["labor_count"]],17)
			_action(host,box,"登记志愿服役",action,row["source_id"])
		elif row.get("reserve_status")=="training": host._text(box,"先在「地方近况」安排自愿训练，实际完成后才能登记。",17)
		else: host._text(box,"当前尚不能登记；请核对受训人数、在场军职、服役许可及正在办理的事务。",17)
		for action in row.get("release_actions",[]):
			_action(host,box,"退籍返乡 · %d名健康人员"%action["fit_count"],action,action["enrollment_id"])
		host._text(box,"归营后仍在军籍。只有健康人员可另办退籍，回到原受训群体；恢复可分配劳力，农田分配须另行安排。",16)
		box.add_child(HSeparator.new())
	if view.get("rows",[]).is_empty(): host._text(parent,"本页仅向同时持有接收军职与组织授权的当事人开放。请先在官职页办理真实任命。",18)
	if view.get("has_more",false): host._text(parent,"尚有其他军籍来源，本页仅列当前有限清单。",16)

## 工资沿既有周期与欠额显示，收款人始终是原家户，不把军团携款当工资支付。
static func _wages(host, parent: Control, rows: Array) -> void:
	host._text(parent,"家户工资 · 公账按期支付至原家户",18)
	for row in rows:
		var label: String=STATES.get(row.get("status",""),row.get("status",""))
		var rate: String=str(row["numerator"]) if row["denominator"]==1 else "%d/%d"%[row["numerator"],row["denominator"]]
		var caption: Label=host._text(parent,"%s：每%d个半日，每人%s钱；尚欠%d钱。"%[label,row["period_ticks"],rate,row.get("outstanding",0)],16)
		caption.set_meta("population_wage",row.duplicate(true)); caption.tooltip_text="付款账户：%s\n收款账户：%s\n义务：%s"%[row["payer_wallet_id"],row["payee_wallet_id"],row["obligation_id"]]

## 口粮来源取原生已证责任方，俘虏不会被界面写成仍由原驻军重复供养。
static func _food(host, parent: Control, row: Dictionary) -> void:
	for food in row.get("military_food_sources",[]):
		var camp: Dictionary=host._data.get("garrisons",{}).get(food.get("garrison_id",""),{})
		var owner: String=host._name(camp) if not camp.is_empty() else "实际军团或羁押方"
		var caption: Label=host._text(parent,"军粮责任：%s，%d人；只从实际军粮库供给。"%[owner,food["member_count"]],16)
		caption.set_meta("population_food_source",food.duplicate(true)); caption.tooltip_text="实际粮库："+food["inventory_id"]
	for food in row.get("macro_food_sources",[]):
		var title: String="受训预备" if food.get("cohort_id")==row.get("reserve_cohort_id") else "离队"
		var caption: Label=host._text(parent,title+"群体：沿原地方口粮义务供养。",16)
		caption.set_meta("population_food_source",food.duplicate(true)); caption.tooltip_text="实际粮库："+food["inventory_id"]

## 预览依既有异步队列返回；成功后也必须在点击时核对原世界、版本、人物与活控件。
static func _action(host, parent: Control, label: String, action: Dictionary, target: String) -> void:
	if not action.get("command") is String or not action.get("params") is Dictionary: return
	var request: Dictionary=action["params"].duplicate(true)
	var source: Dictionary=host.kernel.source_identity().duplicate(true)
	var actor: String=host.kernel.command_context().get("actor_id","")
	var quote: Dictionary=host.kernel.preview(action["command"],request)
	var button: Button=host._button(parent,label,action["command"],target,func(): pass,not quote.get("ok",false) or host._advancing)
	button.name="PopulationMilitaryAction"; button.set_meta("population_military_action",action.duplicate(true)); button.set_meta("population_military_quote",quote.duplicate(true))
	button.pressed.connect(_submit.bind(host,button,action["command"],request,source,actor,host._scene_counter))
	var reason: String="需%d个半日"%quote.get("duration",1) if quote.get("ok",false) else "正在核对军籍…" if quote.get("code")=="NativeAsyncRequired" else host._reason(quote.get("code","Unknown"))
	host._text(parent,reason,16)

## 旧页面或旧身份不能把一次同意复用到新世界；最终提交仍由中央重新报价。
static func _submit(host, button: Button, kind: String, params: Dictionary, source: Dictionary, actor: String, epoch: int) -> void:
	if not is_instance_valid(button) or button.is_queued_for_deletion() or not button.is_inside_tree() or not button.is_visible_in_tree() or button.disabled: return
	if not host._military_open or host._advancing or not host._storage_intent.is_empty() or epoch!=host._scene_counter: return
	if source!=host.kernel.source_identity() or actor!=host.kernel.command_context().get("actor_id",""): return
	host._perform(kind,params.duplicate(true))
