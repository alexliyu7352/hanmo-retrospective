## 地方经济基本页只展示原生已授权的实际生产、人口、工资和供养；查询不推进世界。
extends RefCounted
const CohortWageNeedSummary=preload("res://ui/cohort_wage_need_summary.gd")
const FiscalNeedSummary=preload("res://ui/fiscal_need_summary.gd")
const HouseholdProvisionPanel=preload("res://ui/household_provision_panel.gd")
const SupplyPolicyPanel=preload("res://ui/supply_policy_panel.gd")
const META="economic_world_draft"
const POPULATION_STATUS={"civilian":"民户","training":"正在训练","trained_reserve":"后备","serving":"服役","wounded":"养伤","captive":"被俘","displaced":"流离"}

## 草稿绑定同一世界人物和地点，切换身份时清除机构选择与旧游标。
static func _draft(host) -> Dictionary:
	var source: Dictionary={"identity":host.kernel.source_identity(),"actor_id":host.kernel.command_context().get("actor_id","")}
	var place: String=host.kernel.presentation_location().get("place_id","")
	var draft: Dictionary=host.get_meta(META,{})
	if draft.get("source")!=source or draft.get("place_id")!=place:
		draft={"source":source,"place_id":place,"authority":{},"cursor":{},"fiscal_cursor":{},"occupation_fiscal_cursor":{},"fiscal_taxpayer_cursor":{},"production_workers":{},"history_unit":"","history_cursor":{}}
	host.set_meta(META,draft.duplicate(true))
	return draft

## 机构选择由既有任职入口给出真实Office；新查询仍由原生复核epoch与grant。
static func select_authority(host, authority: Dictionary) -> void:
	var draft: Dictionary=_draft(host)
	draft["authority"]=authority.duplicate(true)
	draft["cursor"]={}; draft["fiscal_cursor"]={}; draft["occupation_fiscal_cursor"]={}; draft["fiscal_taxpayer_cursor"]={}; draft["history_unit"]=""; draft["history_cursor"]={}
	host.set_meta(META,draft.duplicate(true))

## 复用Main的异步查询链，旧profile必须由调用方能力门挡住；失败保留明确错误。
static func prepare(host) -> bool:
	var draft: Dictionary=_draft(host)
	if not str(draft.get("history_unit","")).is_empty():
		var history: Dictionary=_history_query(host,draft)
		if history.get("code")=="NativeAsyncRequired": return false
		if history.get("ok",true)==false: host._resident_page_error=history; return false
		if history.get("identity")!=draft["source"]["identity"] or history.get("observer_actor_id")!=draft["source"]["actor_id"] or history.get("unit_id")!=draft["history_unit"]:
			host._resident_page_error={"ok":false,"code":"StaleProductionHistoryView"}; return false
		return true
	var view: Dictionary=host.kernel.local_economy(draft["place_id"],draft["cursor"],draft["authority"])
	if view.get("code")=="NativeAsyncRequired": return false
	if view.get("ok",true)==false: host._resident_page_error=view; return false
	if view.get("identity")!=draft["source"]["identity"] or view.get("observer_actor_id")!=draft["source"]["actor_id"]:
		host._resident_page_error={"ok":false,"code":"StaleLocalEconomyView"}; return false
	if view.get("fiscal_available",false):
		var fiscal: Dictionary=host.kernel.fiscal_settlement(draft["place_id"],draft.get("fiscal_cursor",{}),draft["authority"])
		if fiscal.get("code")=="NativeAsyncRequired": return false
		if fiscal.get("ok",true)==false: host._resident_page_error=fiscal; return false
		if fiscal.get("identity")!=draft["source"]["identity"] or fiscal.get("observer_actor_id")!=draft["source"]["actor_id"]:
			host._resident_page_error={"ok":false,"code":"StaleFiscalSettlementView"}; return false
	if view.get("occupation_fiscal_available",false):
		var occupied: Dictionary=host.kernel.occupation_fiscal_settlement(draft["place_id"],draft.get("occupation_fiscal_cursor",{}),draft["authority"])
		if occupied.get("code")=="NativeAsyncRequired": return false
		if occupied.get("ok",true)==false: host._resident_page_error=occupied; return false
		if occupied.get("identity")!=draft["source"]["identity"] or occupied.get("observer_actor_id")!=draft["source"]["actor_id"] or occupied.get("place_id")!=draft["place_id"]:
			host._resident_page_error={"ok":false,"code":"StaleOccupationFiscalView"}; return false
	if view.get("fiscal_taxpayer_continuity_available",false):
		var paused: Dictionary=host.kernel.fiscal_taxpayer_continuity(draft["place_id"],draft.get("fiscal_taxpayer_cursor",{}),draft["authority"])
		if paused.get("code")=="NativeAsyncRequired": return false
		if paused.get("ok",true)==false: host._resident_page_error=paused; return false
		if paused.get("identity")!=draft["source"]["identity"] or paused.get("observer_actor_id")!=draft["source"]["actor_id"] or paused.get("place_id")!=draft["place_id"]:
			host._resident_page_error={"ok":false,"code":"StaleFiscalTaxpayerView"}; return false
	if view.get("economic_supply_policies_available",false) and not SupplyPolicyPanel.prepare(host,draft): return false
	if view.get("household_provision_management_available",false) and not HouseholdProvisionPanel.prepare(host): return false
	return true

## 对象名称只解析DTO允许的身份，没有作者名称则用普通称谓而非内部ID。
static func _name(host, table: String, id: String, fallback: String) -> String:
	var record: Dictionary=host._data.get(table,{}).get(id,{})
	return host._name(record) if not record.is_empty() else fallback

## 实际产出和容量损失分别叙述；预计到期不被描述为已收到的钱粮。
static func _quantities(host, rows: Array) -> String:
	var parts=PackedStringArray()
	for row in rows:
		parts.append("%s%d份"%[_name(host,"items",row.get("item_id",""),"物资"),row.get("quantity",0)])
	return "、".join(parts) if not parts.is_empty() else "无"

## 原生有限读已筛选当前原因与原收据；页面不按今日系数回算旧产出。
static func _disruption(host, value: Dictionary, settled: bool) -> String:
	if value.is_empty(): return ""
	var text: String="该期减产" if settled else "当前减产安排"
	text+="：保留原潜在产量的%.2f%%"%(value.get("remaining_yield_bp",10000)/100.0)
	for cause in value.get("causes",[]):
		var reason: String=str(host._data.get("texts",{}).get(cause.get("reason_key",""),"已登记的地方影响"))
		text+="\n%s · 半日%d至%d，剩余产量系数%.2f%%"%[reason,cause.get("from_tick",0),cause.get("through_tick",0),cause.get("remaining_yield_bp",10000)/100.0]
	if value.get("causes",[]).size()>1: text+="\n本安排按各原因的最低剩余系数结算。"
	if settled:
		for output in value.get("outputs",[]):
			text+="\n%s：原潜在%d份，减产未形成%d份，减产后%d份；实际入库%d份。"%[_name(host,"items",output.get("item_id",""),"物资"),output.get("original_potential",0),output.get("unproduced",0),output.get("adjusted_potential",0),output.get("stored",0)]
	else: text+="\n只影响时窗内到期的收成；原投入照常消耗，届满后不补回减产。"
	return text

## 一条业务结果对应一份实际收据，已经离职也仍显示其未清欠额。
static func describe(host, row: Dictionary) -> String:
	match row.get("kind",""):
		"production":
			var text="生产 · 已安排%d人，每期最多%d批；下次结算半日%d"%[row.get("allocated_workers",0),row.get("capacity",0),row.get("next_due_tick",0)]
			if row.get("execution_owner") is Dictionary:
				text+="\n经营由%s依明确接管决定执行。"%_name(host,"organizations",row["execution_owner"].get("id",""),"已批准接任机构")
			if not str(row.get("configuration_block_code","")).is_empty(): text+="\n生产配置责任尚未全部获准，暂不能调整。"
			if row.get("production_disruption") is Dictionary: text+="\n"+_disruption(host,row["production_disruption"],false)
			if row.get("last_result") is Dictionary:
				var result: Dictionary=row["last_result"]
				text+="\n最近半日%d：有效劳力%d，完成%d批；实际产出%s"%[result.get("due_tick",0),result.get("effective_workers",0),result.get("batches",0),_quantities(host,result.get("outputs",[]))]
				if not result.get("inputs",[]).is_empty(): text+="；实际投入"+_quantities(host,result["inputs"])
				if not result.get("unrealized_output",[]).is_empty(): text+="\n仓储不足，未形成库存："+_quantities(host,result["unrealized_output"])
				if result.get("production_disruption") is Dictionary: text+="\n"+_disruption(host,result["production_disruption"],true)
				if result.get("production_context") is Dictionary:
					var settled: Dictionary=result["production_context"]
					text+="\n该次结算：土地设施上限%d批，地区系数%.0f%%，季节系数%.0f%%"%[settled.get("capacity",0),settled.get("region_factor_bp",10000)/100.0,settled.get("season_factor_bp",10000)/100.0]
			else: text+="\n尚无可查的已结算产出。" if row.get("execution_owner") is Dictionary else "\n尚无已结算产出。"
			if row.get("context") is Dictionary:
				var context: Dictionary=row["context"]
				text+="\n下次半日%d适用：土地设施上限%d批，地区系数%.0f%%，季节系数%.0f%%"%[row.get("next_due_tick",0),context.get("capacity",0),context.get("region_factor_bp",10000)/100.0,context.get("season_factor_bp",10000)/100.0]
			if row.get("pending_configuration") is Dictionary:
				var pending: Dictionary=row["pending_configuration"]
				text+="\n已安排在本周期完成后调整，首次按新安排结算：半日%d。"%pending.get("first_due_tick",0)
			return text
		"population":
			return "%s · %d人，可用劳力%d人"%[POPULATION_STATUS.get(row.get("status",""),"居民"),row.get("headcount",0),row.get("labor_capacity",0)]
		"arrear":
			var text: String="%s · 半日%d到期，应付%d钱，实付%d钱，尚欠%d钱%s"%[_name(host,"organizations",row.get("principal_id",""),"付款方"),row.get("due_tick",0),row.get("original_due",0),row.get("paid_amount",0),row.get("outstanding",0),"（后续责任已停止，旧账仍保留）" if not row.get("obligation_active",true) else ""]
			var wage_summary: String=CohortWageNeedSummary.describe(row)
			return text+("\n"+wage_summary if not wage_summary.is_empty() else "")
		"ration":
			var result: Dictionary=row.get("result",{})
			return "半日%d供养 · 应需%d份%s，实际消耗%d份，短缺%d份"%[result.get("due_tick",0),result.get("required",0),_name(host,"items",result.get("item_id",""),"口粮"),result.get("consumed",0),result.get("shortage",0)]
	return ""

## 回调边界用Variant接收已释放引用，先判活再读控件；强类型Node会在guard前报错。
## 关闭页面、旧控件或身份改变后不接受旧回调，分页只更新同源只读草稿。
static func _page(host, button: Variant, cursor: Dictionary, source: Dictionary, authority: Dictionary, scene_epoch: int, field: String="cursor") -> void:
	if not is_instance_valid(host) or not is_instance_valid(button) or not button is Button or button.is_queued_for_deletion() or not button.is_inside_tree() or not button.is_visible_in_tree() or button.disabled: return
	if not host._dynamic_open or host._advancing or not host._storage_intent.is_empty() or scene_epoch!=host._scene_counter: return
	if source.get("identity")!=host.kernel.source_identity() or source.get("actor_id")!=host.kernel.command_context().get("actor_id"): return
	var draft: Dictionary=host.get_meta(META,{})
	if draft.get("source")!=source or draft.get("authority")!=authority: return
	if field not in ["cursor","fiscal_cursor","occupation_fiscal_cursor","fiscal_taxpayer_cursor","history_cursor"]: return
	draft[field]=cursor.duplicate(true); host.set_meta(META,draft.duplicate(true)); host._render()

## 只从原生返回的当前可用Office选择查账身份；旧按钮不能改换已关闭或换代页面。
static func _choose_authority(host, button: Variant, authority: Dictionary, source: Dictionary, scene_epoch: int) -> void:
	if not is_instance_valid(host) or not is_instance_valid(button) or not button is Button or button.is_queued_for_deletion() or not button.is_inside_tree() or not button.is_visible_in_tree() or button.disabled: return
	if not host._dynamic_open or host._advancing or not host._storage_intent.is_empty() or scene_epoch!=host._scene_counter: return
	if source.get("identity")!=host.kernel.source_identity() or source.get("actor_id")!=host.kernel.command_context().get("actor_id"): return
	select_authority(host,authority); host._render()

## 当前页只保存待提交人数；已有计划作为初值，旧期实际人数仍在结果段显示。
static func _worker_value(draft: Dictionary, row: Dictionary, labor: Dictionary) -> int:
	var saved: Dictionary=draft.get("production_workers",{}).get(row.get("unit_id",""),{})
	var allocation: String=labor.get("allocation_id","")
	if saved.has(allocation): return int(saved[allocation])
	if row.get("pending_configuration") is Dictionary:
		for target in row["pending_configuration"].get("targets",[]):
			if target.get("allocation_id")==allocation: return int(target.get("workers",0))
	return int(labor.get("workers",0))

## 人数编辑不调用世界命令；过期控件也不能改变新的草稿。
static func _remember_workers(value: float, host, spin: Variant, unit: String, allocation: String, source: Dictionary, scene_epoch: int) -> void:
	if not is_instance_valid(host) or not is_instance_valid(spin) or not spin is SpinBox or not spin.is_inside_tree() or not spin.is_visible_in_tree() or spin.is_queued_for_deletion(): return
	if not host._dynamic_open or host._advancing or scene_epoch!=host._scene_counter: return
	if source.get("identity")!=host.kernel.source_identity() or source.get("actor_id")!=host.kernel.command_context().get("actor_id"): return
	var draft: Dictionary=host.get_meta(META,{})
	if draft.get("source")!=source: return
	var choices: Dictionary=draft.get("production_workers",{})
	var unit_choices: Dictionary=choices.get(unit,{})
	unit_choices[allocation]=int(value); choices[unit]=unit_choices; draft["production_workers"]=choices
	host.set_meta(META,draft.duplicate(true))

## 只提交具名业务命令；权限、预留和周期边界由原生报价及候选再次裁决。
static func _configure(host, button: Variant, row: Dictionary, mode: String, source: Dictionary, scene_epoch: int) -> void:
	if not is_instance_valid(host) or not is_instance_valid(button) or not button is Button or button.is_queued_for_deletion() or not button.is_inside_tree() or not button.is_visible_in_tree() or button.disabled: return
	if not host._dynamic_open or host._advancing or not host._storage_intent.is_empty() or scene_epoch!=host._scene_counter: return
	if source.get("identity")!=host.kernel.source_identity() or source.get("actor_id")!=host.kernel.command_context().get("actor_id"): return
	if not row.get("may_configure",false): return
	var draft: Dictionary=host.get_meta(META,{})
	if draft.get("source")!=source: return
	var targets: Array=[]
	if mode!="cancel_pending":
		for labor in row.get("labor",[]):
			targets.append({"allocation_id":labor.get("allocation_id",""),"workers":0 if mode=="stop" else _worker_value(draft,row,labor)})
	var params: Dictionary={"actor_id":source["actor_id"],"unit_id":row.get("unit_id",""),"authority":row.get("configuration_authority"),"kind":"cancel_pending" if mode=="cancel_pending" else "configure","targets":targets}
	host._perform("configure_local_production",params)

## 生产操作紧邻真实产出，停工明确等待当前周期完成，不暗示退还已消耗物资。
static func _production_controls(host, body: Control, row: Dictionary, draft: Dictionary) -> void:
	if not row.get("may_configure",false): return
	var box=VBoxContainer.new(); box.name="ProductionConfiguration"; body.add_child(box)
	var ordinal=0
	for labor in row.get("labor",[]):
		ordinal+=1
		var line=HBoxContainer.new(); box.add_child(line)
		host._text(line,"第%d组劳力 · 下周期人数"%ordinal,17)
		var spin=SpinBox.new(); spin.name="ProductionWorkers"; spin.min_value=0; spin.max_value=min(int(labor.get("maximum_workers",0)),2147483647); spin.step=1; spin.value=_worker_value(draft,row,labor); spin.editable=not host._advancing; line.add_child(spin)
		spin.value_changed.connect(_remember_workers.bind(host,spin,row.get("unit_id",""),labor.get("allocation_id",""),draft["source"],host._scene_counter))
	var actions=HBoxContainer.new(); box.add_child(actions)
	for action in [["安排下一周期","configure"],["本期完成后停工","stop"]]:
		var button: Button=host._button(actions,action[0],"production_"+action[1],row.get("unit_id",""),func(): pass,host._advancing)
		button.name="ProductionConfigure" if action[1]=="configure" else "ProductionStop"
		button.pressed.connect(_configure.bind(host,button,row.duplicate(true),action[1],draft["source"],host._scene_counter))
	if row.get("pending_configuration") is Dictionary:
		var cancel: Button=host._button(actions,"撤回未生效安排","production_cancel",row.get("unit_id",""),func(): pass,host._advancing); cancel.name="ProductionCancel"
		cancel.pressed.connect(_configure.bind(host,cancel,row.duplicate(true),"cancel_pending",draft["source"],host._scene_counter))

## 追加在原地方近况滚动区，继续使用原返回场所按钮和现有场景生命周期。
static func build(host, stage: Control, configuration_available: bool=false) -> void:
	var draft: Dictionary=_draft(host)
	if not str(draft.get("history_unit","")).is_empty():
		_build_history(host,stage,draft)
		return
	var view: Dictionary=host.kernel.local_economy(draft["place_id"],draft["cursor"],draft["authority"])
	if view.get("identity")!=draft["source"]["identity"] or view.get("observer_actor_id")!=draft["source"]["actor_id"]: return
	var panel: Control=stage.find_child("DynamicWorldPanel",true,false)
	if panel==null: return
	var scroll: ScrollContainer=panel.get_node("DynamicWorldScroll")
	var body: Control=scroll.get_child(0)
	var section=VBoxContainer.new(); section.name="EconomicWorldSection"; section.add_theme_constant_override("separation",12); body.add_child(section)
	section.add_child(HSeparator.new()); host._text(section,"生产与收支",22)
	if not view.get("authorities",[]).is_empty():
		var identities=HBoxContainer.new(); identities.name="EconomicAuthoritySelection"; section.add_child(identities)
		var personal: Button=host._button(identities,"本人收支","economic_personal","",func(): pass,host._advancing)
		personal.pressed.connect(_choose_authority.bind(host,personal,{},draft["source"],host._scene_counter))
		for authority in view["authorities"]:
			var organization_name: String=_name(host,"organizations",authority.get("organization_id",""),"任职机构")
			var button: Button=host._button(identities,organization_name+" · 公务明细","economic_authority",authority.get("office_id",""),func(): pass,host._advancing)
			button.pressed.connect(_choose_authority.bind(host,button,authority.duplicate(true),draft["source"],host._scene_counter))
	for row in view.get("rows",[]):
		var text: String=describe(host,row)
		if text.is_empty(): continue
		var label: Label=host._text(section,text,18); label.name="EconomicWorldRow"
		if row.get("kind")=="production" and row.get("history_available",false):
			var history: Button=host._button(section,"查看历期投入与产出","production_history",row.get("unit_id",""),func(): pass,host._advancing)
			history.pressed.connect(_open_history.bind(host,history,row.get("unit_id",""),draft["source"],draft["authority"],host._scene_counter))
		if (configuration_available or view.get("configuration_available",false)) and row.get("kind")=="production": _production_controls(host,section,row,draft)
	if view.get("rows",[]).is_empty(): host._text(section,"此处暂无你可查阅的经济明细。公开供给见地方消息。",18)
	var navigation=HBoxContainer.new(); section.add_child(navigation)
	if not draft["cursor"].is_empty():
		var first: Button=host._button(navigation,"从头查看收支","economic_world_first","",func(): pass,host._advancing)
		first.pressed.connect(_page.bind(host,first,{},draft["source"],draft["authority"],host._scene_counter))
	if view.get("next_cursor") is Dictionary:
		var next: Button=host._button(navigation,"更多经济明细","economic_world_next","",func(): pass,host._advancing)
		next.pressed.connect(_page.bind(host,next,view["next_cursor"].duplicate(true),draft["source"],draft["authority"],host._scene_counter))

	if view.get("fiscal_available",false): _fiscal_section(host,section,draft)
	if view.get("occupation_fiscal_available",false): _occupation_fiscal_section(host,section,draft)
	if view.get("economic_supply_policies_available",false): SupplyPolicyPanel.build(host,section,draft)
	if view.get("fiscal_taxpayer_continuity_available",false): _taxpayer_continuity_section(host,section,draft)
	if view.get("household_provision_management_available",false): HouseholdProvisionPanel.build(host,section)

## 税账只叙述已完成评税及实缴，原欠期独立保留，不把倍率或公库余额当收入。
static func _fiscal_section(host, body: Control, draft: Dictionary) -> void:
	var view: Dictionary=host.kernel.fiscal_settlement(draft["place_id"],draft.get("fiscal_cursor",{}),draft["authority"])
	if view.get("identity")!=draft["source"]["identity"] or view.get("observer_actor_id")!=draft["source"]["actor_id"]: return
	var section=VBoxContainer.new(); section.name="FiscalSettlementSection"; body.add_child(section)
	section.add_child(HSeparator.new()); host._text(section,"税赋应付与实缴",21)
	for row in view.get("rows",[]):
		var mode: String={"lenient":"宽免","normal":"常赋","surcharge":"加征"}.get(row.get("mode",""),"原期税政")
		var grain: String=_name(host,"items",row.get("grain_item_id",""),"粮食")
		var text="半日%d · %s\n应缴%d钱，实缴%d钱，尚欠%d钱；%s应缴%d份，实缴%d份，尚欠%d份。"%[row.get("due_tick",0),mode,row.get("cash_due",0),row.get("cash_paid",0),row.get("cash_outstanding",0),grain,row.get("grain_due",0),row.get("grain_paid",0),row.get("grain_outstanding",0)]
		text+="\n该税源本期实际粮产%d份，民用留粮%d份；旧欠期沿原应付结算。"%[row.get("produced_grain",0),row.get("civilian_grain_reserve",0)]
		var need_summary: String=FiscalNeedSummary.describe(row)
		if not need_summary.is_empty(): text+="\n"+need_summary
		var label: Label=host._text(section,text,18); label.name="FiscalSettlementRow"
	if view.get("rows",[]).is_empty(): host._text(section,"此处暂无你可查阅的已评税记录。",18)
	var navigation=HBoxContainer.new(); section.add_child(navigation)
	if not draft.get("fiscal_cursor",{}).is_empty():
		var first: Button=host._button(navigation,"从头查看税账","fiscal_first","",func(): pass,host._advancing)
		first.pressed.connect(_page.bind(host,first,{},draft["source"],draft["authority"],host._scene_counter,"fiscal_cursor"))
	if view.get("next_cursor") is Dictionary:
		var next: Button=host._button(navigation,"更多已评税记录","fiscal_next","",func(): pass,host._advancing)
		next.pressed.connect(_page.bind(host,next,view["next_cursor"].duplicate(true),draft["source"],draft["authority"],host._scene_counter,"fiscal_cursor"))

## 新账单独呈现原受领机构、实际缴款和分数权益；暂停期不会显示伪造零税额。
static func occupation_fiscal_description(host, row: Dictionary) -> String:
	var recipient: String=_name(host,"organizations",row.get("organization_id",""),"原受领机构")
	var grain: String=_name(host,"items",row.get("grain_item_id",""),"粮食")
	var text: String="半日%d · 原受领%s\n本期新税基收入系数%.2f%%；旧欠额保持原数。\n应缴%d钱，实缴%d钱，尚欠%d钱；%s应缴%d份，实缴%d份，尚欠%d份。"%[row.get("due_tick",0),recipient,row.get("income_basis_points",10000)/100.0,row.get("cash_due",0),row.get("cash_paid",0),row.get("cash_outstanding",0),grain,row.get("grain_due",0),row.get("grain_paid",0),row.get("grain_outstanding",0)]
	var reasons={"original_beneficiary_control_lost":"原受领机构已失去管辖，本笔旧款暂停收缴。","original_taxpayer_assets_captured":"原纳税组织资产已换属，本笔旧款暂停收缴。","responsibility_under_takeover":"本笔原责任已进入接管处置，旧款暂停收缴。"}
	var reason: String=str(row.get("collection_pause_reason",""))
	if not reason.is_empty(): text+="\n"+str(reasons.get(reason,"原责任暂不可执行，旧欠额仍保留。"))
	var cash: Dictionary=row.get("cash_bucket",{}); var food: Dictionary=row.get("grain_bucket",{})
	text+="\n原受益公库分别保留分数权益：钱税%d/1亿 ＋ %d/1万亿；粮税%d/1亿 ＋ %d/1万亿。不是可支用钱粮。"%[cash.get("coarse",0),cash.get("fine",0),food.get("coarse",0),food.get("fine",0)]
	var need_summary: String=FiscalNeedSummary.describe(row)
	if not need_summary.is_empty(): text+="\n"+need_summary
	return text

static func _occupation_fiscal_section(host, body: Control, draft: Dictionary) -> void:
	var view: Dictionary=host.kernel.occupation_fiscal_settlement(draft["place_id"],draft.get("occupation_fiscal_cursor",{}),draft["authority"])
	if view.get("identity")!=draft["source"]["identity"] or view.get("observer_actor_id")!=draft["source"]["actor_id"] or view.get("place_id")!=draft["place_id"]: return
	var section=VBoxContainer.new(); section.name="OccupationFiscalSection"; body.add_child(section)
	section.add_child(HSeparator.new()); host._text(section,"接管后的独立税账",21)
	for row in view.get("rows",[]):
		var label: Label=host._text(section,occupation_fiscal_description(host,row),18); label.name="OccupationFiscalRow"
	for paused in view.get("suspended_periods",[]):
		var text: String="半日%d · 原纳税组织资产已换属，本期暂停评税；没有生成新应缴额，原欠期和下次税期仍保留。"%paused.get("due_tick",0)
		var label: Label=host._text(section,text,18); label.name="OccupationFiscalSuspension"
	if view.get("rows",[]).is_empty() and view.get("suspended_periods",[]).is_empty(): host._text(section,"此处暂无你可查阅的接管后税账。",18)
	var navigation=HBoxContainer.new(); section.add_child(navigation)
	if not draft.get("occupation_fiscal_cursor",{}).is_empty():
		var first: Button=host._button(navigation,"从头查看接管税账","occupation_fiscal_first","",func(): pass,host._advancing)
		first.pressed.connect(_page.bind(host,first,{},draft["source"],draft["authority"],host._scene_counter,"occupation_fiscal_cursor"))
	if view.get("next_cursor") is Dictionary:
		var next: Button=host._button(navigation,"更多接管税账","occupation_fiscal_next","",func(): pass,host._advancing)
		next.pressed.connect(_page.bind(host,next,view["next_cursor"].duplicate(true),draft["source"],draft["authority"],host._scene_counter,"occupation_fiscal_cursor"))

## Only the current lawful unit ID enters this query; account IDs and cause graphs stay native.
static func _history_query(host, draft: Dictionary) -> Dictionary:
	var args: Dictionary={"place_id":draft["place_id"],"unit_id":draft["history_unit"]}
	if not draft.get("history_cursor",{}).is_empty(): args["cursor"]=draft["history_cursor"].duplicate(true)
	if not draft.get("authority",{}).is_empty(): args["authority"]=draft["authority"].duplicate(true)
	return host.kernel.demand_view("local_production_history",args)

static func _open_history(host, button: Variant, unit: String, source: Dictionary, authority: Dictionary, scene_epoch: int) -> void:
	if not is_instance_valid(host) or not is_instance_valid(button) or not button is Button or button.is_queued_for_deletion() or not button.is_inside_tree() or not button.is_visible_in_tree() or button.disabled: return
	if not host._dynamic_open or host._advancing or not host._storage_intent.is_empty() or scene_epoch!=host._scene_counter: return
	if source.get("identity")!=host.kernel.source_identity() or source.get("actor_id")!=host.kernel.command_context().get("actor_id"): return
	var draft: Dictionary=host.get_meta(META,{})
	if draft.get("source")!=source or draft.get("authority")!=authority: return
	draft["history_unit"]=unit; draft["history_cursor"]={}
	host.set_meta(META,draft.duplicate(true)); host._render()

static func _build_history(host, stage: Control, draft: Dictionary) -> void:
	var view: Dictionary=_history_query(host,draft)
	if view.get("identity")!=draft["source"]["identity"] or view.get("observer_actor_id")!=draft["source"]["actor_id"] or view.get("unit_id")!=draft["history_unit"]: return
	var panel: Control=stage.find_child("DynamicWorldPanel",true,false)
	if panel==null: return
	var section=VBoxContainer.new(); section.name="ProductionHistorySection"; section.add_theme_constant_override("separation",12)
	panel.get_node("DynamicWorldScroll").get_child(0).add_child(section)
	host._text(section,"生产历期结算",22)
	host._text(section,"以下为你可查阅的原始结算，从最近一期向前排列。",17)
	var back: Button=host._button(section,"返回生产与收支","production_history_back","",func(): pass,host._advancing)
	back.pressed.connect(_open_history.bind(host,back,"",draft["source"],draft["authority"],host._scene_counter))
	for receipt in view.get("rows",[]):
		var text="半日%d到期 · 安排%d人，实际有效%d人，完成%d批"%[receipt.get("due_tick",0),receipt.get("allocated_workers",0),receipt.get("effective_workers",0),receipt.get("batches",0)]
		if receipt.get("tick",0)!=receipt.get("due_tick",0): text+="（半日%d结算）"%receipt.get("tick",0)
		text+="\n实际投入："+_quantities(host,receipt.get("inputs",[]))+"；实际产出："+_quantities(host,receipt.get("outputs",[]))
		if not receipt.get("unrealized_output",[]).is_empty(): text+="\n仓储不足，未形成库存："+_quantities(host,receipt["unrealized_output"])
		if receipt.get("production_disruption") is Dictionary: text+="\n"+_disruption(host,receipt["production_disruption"],true)
		if receipt.get("production_context") is Dictionary:
			var context: Dictionary=receipt["production_context"]
			text+="\n该次结算：土地设施上限%d批，地区系数%.0f%%，季节系数%.0f%%"%[context.get("capacity",0),context.get("region_factor_bp",10000)/100.0,context.get("season_factor_bp",10000)/100.0]
		var label: Label=host._text(section,text,18); label.name="ProductionHistoryReceipt"
	if view.get("rows",[]).is_empty(): host._text(section,"暂无你可查阅的已结算记录。",18)
	var navigation=HBoxContainer.new(); section.add_child(navigation)
	if not draft.get("history_cursor",{}).is_empty():
		var first: Button=host._button(navigation,"最近结算","production_history_first","",func(): pass,host._advancing)
		first.pressed.connect(_page.bind(host,first,{},draft["source"],draft["authority"],host._scene_counter,"history_cursor"))
	if view.get("next_cursor") is Dictionary:
		var older: Button=host._button(navigation,"更早结算","production_history_older","",func(): pass,host._advancing)
		older.pressed.connect(_page.bind(host,older,view["next_cursor"].duplicate(true),draft["source"],draft["authority"],host._scene_counter,"history_cursor"))

## 显式连续性段说明停税原因；旧债仍按原数保留，不把未评税期显示为零额欠款。
static func fiscal_taxpayer_description(row: Dictionary) -> String:
	if row.get("kind")=="fiscal_taxpayer_period_suspended":
		return "半日%d · 原纳税组织资产已换属，本期暂停评税。\n没有新增应缴额，原税期日程继续，恢复后不补征本期。"%row.get("due_tick",0)
	return "半日%d原欠期 · 原纳税组织资产已换属，暂缓收款。\n尚欠%d钱、%d份粮，金额与原受领机构保留，不向新所有人转派旧税。"%[row.get("due_tick",0),row.get("cash_outstanding",0),row.get("grain_outstanding",0)]

## 与旧税账分开分页，机构/场所/世界变化后原父页草稿一并失效。
static func _taxpayer_continuity_section(host, body: Control, draft: Dictionary) -> void:
	var view: Dictionary=host.kernel.fiscal_taxpayer_continuity(draft["place_id"],draft.get("fiscal_taxpayer_cursor",{}),draft["authority"])
	if view.get("identity")!=draft["source"]["identity"] or view.get("observer_actor_id")!=draft["source"]["actor_id"] or view.get("place_id")!=draft["place_id"]: return
	var section=VBoxContainer.new(); section.name="FiscalTaxpayerContinuitySection"; body.add_child(section)
	section.add_child(HSeparator.new()); host._text(section,"税源换属与暂停记录",21)
	for row in view.get("rows",[]):
		var label: Label=host._text(section,fiscal_taxpayer_description(row),18); label.name="FiscalTaxpayerContinuityRow"
	if view.get("rows",[]).is_empty(): host._text(section,"此处暂无你可查阅的税源暂停记录。",18)
	var navigation=HBoxContainer.new(); section.add_child(navigation)
	if not draft.get("fiscal_taxpayer_cursor",{}).is_empty():
		var first: Button=host._button(navigation,"从头查看暂停记录","fiscal_taxpayer_first","",func(): pass,host._advancing)
		first.pressed.connect(_page.bind(host,first,{},draft["source"],draft["authority"],host._scene_counter,"fiscal_taxpayer_cursor"))
	if view.get("next_cursor") is Dictionary:
		var next: Button=host._button(navigation,"更多暂停记录","fiscal_taxpayer_next","",func(): pass,host._advancing)
		next.pressed.connect(_page.bind(host,next,view["next_cursor"].duplicate(true),draft["source"],draft["authority"],host._scene_counter,"fiscal_taxpayer_cursor"))
