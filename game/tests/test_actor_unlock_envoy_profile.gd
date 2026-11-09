## v2跨局资格的坏来源与旧档边界；不创建World或替代真实任务来源。
extends SceneTree
const Profile=preload("res://core/actor_unlock_profile.gd")
const Start=preload("res://ui/actor_unlock_start.gd")
var checks: int=0
var failures: int=0
func check(ok: bool,label: String) -> void:
	checks+=1
	if not ok: failures+=1; printerr("FAIL "+label)
func _init() -> void: call_deferred("run")
func run() -> void:
	var path: String="user://tests/actor-envoy-"+str(OS.get_process_id())+"-"+str(Time.get_ticks_usec())+".json"
	var profile=Profile.new(path); profile.load_profile(); profile.save_profile()
	var original: String=FileAccess.get_file_as_string(path)
	var loaded=Profile.new(path)
	check(loaded.load_profile().get("ok",false) and JSON.parse_string(original).version==1 and not loaded.needs_save(),"old empty v1 remains v1")
	var source: Dictionary={"card_id":"pc09","target_actor_id":"messenger","earner_actor_id":"player","world_id":"01000000000000000000000000000000","scenario_id":"scene","acquired_tick":20,"envoy":{"task_id":"task:1","contract_id":"contract:2","acceptance_fact_id":"accept:3","mission_authority_fact_id":"mission:4","proposal_id":"proposal:5","delivery_fact_id":"deliver:6","presentation_fact_id":"present:7","completion_fact_id":"complete:8","report_fact_id":"report:9","accepted_tick":1,"delivered_tick":10,"completed_tick":20}}
	var view: Dictionary={"source_version":2,"actor_id":"player","source_identity":{"world_id":source.world_id,"execution_epoch":1,"revision":30,"tick":20},"cards":[{"card_id":"pc09","target_actor_id":"messenger","condition_kind":"completed_envoy_round_trip_v1","required_count":1,"completed_count":1,"acquired":source}]}
	var bad: Dictionary=view.duplicate(true); bad.erase("source_version")
	check(not loaded.observe(bad,"standard").get("ok",true) and loaded.unlocked_actor_ids().is_empty() and FileAccess.get_file_as_string(path)==original,"v1 observation cannot accept v2 source")
	bad=view.duplicate(true); bad.cards[0].acquired.envoy.report_fact_id=bad.cards[0].acquired.envoy.acceptance_fact_id
	check(not loaded.observe(bad,"standard").get("ok",true) and not loaded.needs_save(),"reused fact cannot stand for a return report")
	check(loaded.observe(view,"free").get("ok",false) and loaded.source_for("messenger").is_empty() and loaded.source_for("messenger",true)==source and loaded.save_profile().get("ok",false),"v2 free provenance persists separately")
	var cold=Profile.new(path)
	check(cold.load_profile().get("ok",false) and cold.source_for("messenger",true)==source and JSON.parse_string(FileAccess.get_file_as_string(path)).version==2,"v2 round trip source survives file reload")
	bad=view.duplicate(true); bad.cards[0].acquired.envoy.completed_tick=21
	check(not cold.observe(bad,"standard").get("ok",true) and cold.source_for("messenger").is_empty() and not cold.needs_save(),"mismatched or future completion cannot grant standard provenance")
	var scenario: Dictionary={"dynamic_world":{"founder_policy":"selected_existing_actor_roster_v1"},"actor_card_unlocks":{"profile":"completed_task_actor_cards_v2","initially_available_actor_ids":["player"]},"player_actor":"player"}
	check(Start.creation_mode(scenario)=="standard" and Start.request_qualification(scenario,"messenger",path).get("unlocked_actor_ids",[])==["messenger"],"v2 confirmation consumes same valid cross-life record")
	scenario.actor_card_unlocks.profile="unknown"
	check(Start.creation_mode(scenario).is_empty(),"unknown profile cannot create a life")
	print("ACTOR_UNLOCK_ENVOY_PROFILE ",checks," checks ",failures," failures; 0 World; file=",path)
	quit(1 if failures else 0)
