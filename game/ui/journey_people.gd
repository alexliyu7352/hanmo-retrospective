## 行旅人物表现只消费已选同行与会面的有限输入；画像独立查询，选择仍提交原世界命令。
extends RefCounted
const Art=preload("res://ui/art_catalog.gd")
## 显示已达成的同行约定；解散入口受真实地点和忙碌状态约束。
static func roster(main, box: VBoxContainer, sidebar: Dictionary) -> void:
	if not sidebar.party_present: return
	main._text(box,"这一程的同伴",21)
	for member in sidebar.companions:
		main._text(box,member.display_name+" · 行走半日吃用 %d钱"%member.cost_per_tick,16)
		var request: Dictionary={"actor_id":member.actor_id}
		var quote: Dictionary=main.kernel.preview("dismiss_companion",request)
		main._button(box,"与"+member.display_name+"暂别","dismiss_companion",member.actor_id,main._perform.bind("dismiss_companion",request),not quote.ok or main._advancing)
## 待处理会面优先于赶路控制；显示实际旅人而不是远程任务页。
static func encounter(main, box: VBoxContainer, sidebar: Dictionary) -> bool:
	var e: Dictionary=sidebar.encounter
	if e.is_empty(): return false
	var provider: String=e.provider_actor_id
	main._text(box,"路上相逢 · "+e.display_name,22)
	var portrait=Art.character_control(Art.character_input(main.kernel.character_input(provider),"half_body")); portrait.name="EncounterPortrait"
	portrait.custom_minimum_size=Vector2(0,195)
	box.add_child(portrait)
	var view: Dictionary=main.kernel.interaction_view()
	if view.phase!="scene":
		main.main_column=box
		main._conversation(view,sidebar.dialogue_speaker.display_name)
	else:
		main._text(box,"你们在途中停下脚步。行李还在各自手中，未走完的路也仍在那里。",17)
		var quote: Dictionary=main.kernel.preview("encounter_talk",{"encounter_id":e.encounter_id})
		main._button(box,"与对方交谈","encounter_talk",e.encounter_id,main._perform.bind("encounter_talk",{"encounter_id":e.encounter_id}),not quote.ok)
	var conflict: Dictionary=main.kernel.preview("encounter_fight",{"encounter_id":e.encounter_id})
	if conflict.ok:
		main._text(box,"对方持械阻路。若要交锋，同行者也将置身其中；须照看受托保护之人。",17)
		main._button(box,"持械应战","encounter_fight",e.encounter_id,main._perform.bind("encounter_fight",{"encounter_id":e.encounter_id}))
	main._button(box,"道别，继续原行程","encounter_leave",e.encounter_id,main._perform.bind("encounter_leave",{"encounter_id":e.encounter_id}))
	main._text(box,"会面不会重收此前的路费。道别后仍需走完实际余程。",16)
	return true
