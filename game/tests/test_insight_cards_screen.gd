## 收藏页见闻区域的真实控件与迟到回调；组件验证不代实际World经历。
extends "./test_growth_cards_screen.gd"
func insight_view() -> Dictionary:
	return {"actor_id":"person","cards":[],"offset":0,"limit":32,"next_offset":null,"insight_cards":{"cards":[
		{"card_id":"unknown","name":"尚未发现","hint":"亲身完成军营操练。","discovered":false,"experienced_this_world":false},
		{"card_id":"remembered","name":"军营见闻","description":"行伍生活的记录。","discovered":true,"discovered_free":true,"experienced_this_world":false},
		{"card_id":"experienced","name":"军营见闻","description":"亲身完成操练。","discovered":true,"experienced_this_world":true,"acquired_tick":7}
	],"offset":32,"limit":32,"next_offset":64}}

## 当前经历、旧发现、未知分别展示；所有输入仅走页选择，不提交世界命令。
func run() -> void:
	var host=Host.new();root.add_child(host);host.size=Vector2(1280,800)
	var pages: Array=[]
	var panel: Control=Screen.build(host,host,insight_view(),func(): pass,func(_offset: int): pass,Callable(),func(offset: int): pages.append(offset))
	await process_frame
	var rows=panel.find_children("InsightState","Label",true,false)
	check(rows.size()==3,"insights share original collection surface")
	if rows.size()!=3: host.queue_free();await process_frame;quit(1);return
	check(rows[0].text=="尚未发现","unknown experience stays undiscovered")
	check(rows[1].text.contains("本局尚未经历") and rows[1].text.contains("自由创建记录"),"cross-world discovery is distinct from current experience")
	check(rows[2].text=="本局亲身经历" and panel.find_children("InsightDate","Label",true,false).size()==1,"only current actual experience has current date")
	check(panel.find_children("CardConfigureHeart","Button",true,false).is_empty() and panel.find_children("CardPractice*","Button",true,false).is_empty() and host.calls.is_empty(),"insights grant no skill configuration or practice command")
	var previous: Button=panel.find_child("InsightCardsPrevious",true,false);var next: Button=panel.find_child("InsightCardsNext",true,false)
	var late: Callable=next.pressed.get_connections()[0].callable
	previous.pressed.emit();next.pressed.emit();check(pages==[0,64],"insight pagination uses own bounded offsets")
	host.kernel.revision+=1;next.pressed.emit();check(pages.size()==2,"changed source rejects old insight page")
	host.kernel.revision-=1;panel.find_child("GrowthCardsClose",true,false).pressed.emit();next.pressed.emit();check(pages.size()==2,"closed page rejects insight callback")
	panel.queue_free();await process_frame;late.call();check(pages.size()==2,"freed page safely ignores late callback")
	host.queue_free();await process_frame
	print("INSIGHT_CARDS_SCREEN %d checks %d failures"%[checks,failures]);quit(1 if failures else 0)
