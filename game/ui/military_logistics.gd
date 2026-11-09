## 军粮提示只格式化军事资产的同源纯查询；不推算隐藏敌军口粮、不下令补给，也不以预计半日保证战事时长。
extends RefCounted
const Assets=preload("res://core/military_assets.gd")
const Location=preload("res://core/world_location.gd")

## 只为本人实际统领的活动军团添加紧凑提示，夹具或无军团场景不伪造库存。
static func render(host, parent: Control, state: Dictionary, army_id: String="") -> Label:
	if army_id.is_empty(): army_id=Location.army_id(state,state.player_actor_id)
	var army: Dictionary=state.get("armies",{}).get(army_id,{})
	if army.get("commander_actor_id")!=state.player_actor_id: return null
	var quote: Dictionary=Assets.supply_preview(state,host._data,army_id)
	if not quote.ok: return null
	var label: Label=host._text(parent,"",16); label.name="ArmyLogistics"
	update(label,quote)
	return label

## 有限侧栏和军营投影只消费同源报价；不在绘制层重新选择军团或推算粮数。
static func render_quote(host, parent: Control, quote: Dictionary) -> Label:
	if not quote.get("ok",false): return null
	var label: Label=host._text(parent,"",16); label.name="ArmyLogistics"
	update(label,quote)
	return label

## 编组草案与现军共用文字，只有下一半日实际短缺才警示；无有效正式报价时清空而不沿用旧估算。
static func update(label: Label, quote: Dictionary) -> void:
	label.visible=quote.get("ok",false)
	if not label.visible: label.text=""; label.tooltip_text=""; return
	label.text="军粮 %d份 · 每半日需 %d份\n按当前 %d口估算 %d个完整半日"%[quote.available,quote.required,quote.mouth_count,quote.full_ticks]
	if quote.required==0: label.text="军粮 %d份 · 当前名册无需供粮"%quote.available
	if quote.shortage>0: label.text+="\n下个半日缺粮 %d份，将影响士气与疲劳"%quote.shortage
	label.tooltip_text="伤员、活军官与实俘均计入实际口数，借调人员不重复计算。行军、野战与围城共用半日供粮；后续伤亡、俘虏与补给会改变估算。"
	label.add_theme_color_override("font_color",Color("efaa83") if quote.shortage>0 else Color("e9dec5"))
