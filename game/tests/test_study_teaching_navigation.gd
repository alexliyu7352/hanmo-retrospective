## 两条真实Main导航函数的有限门；替换读取后端，不初始化或创建世界。
extends SceneTree
class ReadOnlyBackend extends RefCounted:
	func personal_battle_view() -> Dictionary: return {}
	func military_battle_page() -> Dictionary: return {}
	func siege_page() -> Dictionary: return {}
	func growth_cards_enabled() -> bool: return true
class MainProbe extends "res://ui/main.gd":
	var render_calls: int=0
	func _render() -> void: render_calls+=1
	func _stop_journey_auto(_reason: String="") -> void: pass
func _initialize() -> void: call_deferred("run")
func run() -> void:
	var main=MainProbe.new()
	main.kernel=ReadOnlyBackend.new(); main._resident_mode=true; main._started=true
	main._study_teaching_open=true; main._open_cards()
	var cards: bool=not main._study_teaching_open and main._cards_open and main.render_calls==1
	main._study_teaching_open=true; main._open_ordinary_skills("martial")
	var skills: bool=not main._study_teaching_open and not main._cards_open and main._ordinary_skills_open and main._ordinary_skill_id=="martial" and main.render_calls==2
	if not cards: printerr("FAIL actual Main teaching-to-cards navigation retains teaching page")
	if not skills: printerr("FAIL actual Main teaching-to-ordinary-skills navigation retains teaching page")
	main.journey_view.free()
	main._ambience.music.free(); main._ambience.effect.free(); main._ambience.free()
	main.free(); await process_frame
	print("STUDY_TEACHING_NAVIGATION 2 checks ",int(not cards)+int(not skills)," failures; 0 World")
	quit(0 if cards and skills else 1)
