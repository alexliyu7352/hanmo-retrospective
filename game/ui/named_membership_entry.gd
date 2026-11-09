## 具名任职直接放在现有人物近况与对白中；控件只展示冻结来源，结果由正式命令决定。
extends RefCounted

## 当场人物继续使用同一个候选容器，不创建组织管理页或远地名册。
static func local_people(main, parent: VBoxContainer) -> void:
	if not main._named_membership_enabled(): return
	var location: Dictionary=main.kernel.presentation_location()
	var view: Dictionary=main.kernel.named_membership_entries(location.get("place_id",""),"")
	if view.get("ok",false): _rows(main,parent,view,{})

## 白话里的任职只针对当前真实说话者，关闭或换人后旧选择不能继续提交。
static func conversation(main, parent: VBoxContainer, interaction: Dictionary) -> void:
	if not main._named_membership_enabled() or interaction.get("phase") not in ["say","choice","end"]: return
	var context: Dictionary=interaction.get("context",{})
	var view: Dictionary=main.kernel.named_membership_entries(context.get("place_id",""),context.get("provider_actor_id",""))
	if view.get("ok",false): _rows(main,parent,view,main._dialogue_identity(interaction))

## 阅读条款是本地展示选择；只有仍在同一场景和来源时才允许展开，不写世界或保存账本。
static func _toggle_terms(main, row_id: String, source: Dictionary, scene_epoch: int) -> void:
	if main._scene_counter!=scene_epoch or main.kernel.source_identity()!=source or main._advancing: return
	var expanded: Dictionary=main.get_meta("membership_terms_expanded",{}).duplicate(true)
	expanded[row_id]=not expanded.get(row_id,false)
	main.set_meta("membership_terms_expanded",expanded); main._render()

## 接受或发邀请前实际展开全部工资/退出条款；只读报价不能替本人自动作答。
static func _rows(main, parent: VBoxContainer, view: Dictionary, dialogue: Dictionary) -> void:
	for row in view.get("rows",[]):
		var box=VBoxContainer.new(); box.name="NamedMembershipEntry"; parent.add_child(box)
		box.set_meta("membership_row",row.duplicate(true)); box.set_meta("membership_source",main.kernel.source_identity())
		main._text(box,row.get("display_name","")+" · "+row.get("text",""),16)
		var terms: String=row.get("terms_text","")
		var expanded: bool=terms.is_empty() or main.get_meta("membership_terms_expanded",{}).get(row.id,false)
		if not terms.is_empty():
			main._button(box,"收起任职条款" if expanded else "核对工资、职责与离开条款","membership_terms",row.id,_toggle_terms.bind(main,row.id,main.kernel.source_identity(),main._scene_counter),main._advancing)
			if expanded:
				var text: Label=main._text(box,terms,15); text.name="NamedMembershipTerms"
				text.set_meta("terms_id",row.get("terms_id",""))
				if row.has("expires_tick"): main._text(box,"邀请在半日%d到期，到期时不能再接受。"%row.expires_tick,15)
		for action in row.get("actions",[]):
			var needs_terms: bool=action.command=="organization_offer_membership" or action.command=="organization_answer_membership" and action.params.get("answer")=="accept"
			if needs_terms and not expanded: continue
			var button: Button=main._regional_button(box,action.text,action.command,row.id,action.params,dialogue,not action.get("available",false))
			button.name="NamedMembershipChoice"; button.set_meta("membership_action",action.duplicate(true)); button.set_meta("membership_row_id",row.id)
			if action.get("code","")=="MembershipPersonBusy" and not dialogue.is_empty(): button.tooltip_text="先结束当前交谈，再答复这份邀请。"
	if view.get("truncated",false): main._text(parent,"眼前事项较多，先处理已显示的邀请，再查看后续。",15)
