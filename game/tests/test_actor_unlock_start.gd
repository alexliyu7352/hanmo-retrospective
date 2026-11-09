## 正式作者三人快照加本域显式人物卡片段；只测选择控件，不称已安装新包或建局。
extends "./test_configurable_actor_roster.gd"
const Profile=preload("res://core/actor_unlock_profile.gd")
const Start=preload("res://ui/actor_unlock_start.gd")
const SCENARIO="civicecon::scenario.qingchuan_political_economy"
const TARGET="society::actor.city_steward"
func run() -> void:
	var args=OS.get_cmdline_user_args()
	if args.size()!=3: printerr("需要正式作者目录、同源schema与人物卡作者片段"); quit(2); return
	var parsed: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(args[0])); var registered: Dictionary=Snapshot.register_schema(FileAccess.get_file_as_string(args[1])); var fragment: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(args[2]))
	if not check(parsed.get("ok",false) and registered.get("ok",false) and fragment.get("ok",false),"actual author inputs parsed"): quit(1); return
	var catalog: Dictionary={"ok":true,"data":parsed.value.catalog,"schema_sha256":registered.schema_sha256,"pack_set_digest":parsed.value.pack_set_digest}
	catalog.data.actor_cards={}
	for card in fragment.value.actor_cards: catalog.data.actor_cards[card.id]=card.duplicate(true)
	catalog.data.texts.merge(fragment.value.texts,true); catalog.data.scenarios[SCENARIO].actor_card_unlocks=fragment.value.actor_card_unlocks.duplicate(true)
	var original: Dictionary=catalog.duplicate(true)
	var path: String="user://tests/actor-start-"+str(OS.get_process_id())+"-"+str(Time.get_ticks_usec())+".json"
	var picker=Picker.new();root.add_child(picker);picker.confirmed.connect(selected)
	var result: Dictionary=picker.configure(catalog,SCENARIO,EmptyArt.new(),path)
	if not check(result.get("ok",false),"real author roster accepts collection policy "+str(result)):picker.free();quit(1);return
	var target: Button=button(picker,"select",TARGET)
	check(target!=null and target.disabled and not button(picker,"select","society::actor.apprentice").disabled,"locked target remains visible while original starter is available")
	target.pressed.emit();check(picker.selected().is_empty() and button(picker,"confirm").disabled,"manually emitted locked signal cannot select")
	var stale: Callable=target.pressed.get_connections()[0].callable
	check(picker.find_children("ActorCollectionStatus","Label",true,false).size()==3,"each actor shows explicit collection qualification")
	var receipt: Dictionary={"card_id":"actorlife::card.pc03","target_actor_id":TARGET,"earner_actor_id":"society::actor.apprentice","world_id":"01000000000000000000000000000000","scenario_id":SCENARIO,"acquired_tick":8,"tasks":[{"task_id":"first","template_id":"procurement","settlement_id":"settled-first","reported_tick":5},{"task_id":"second","template_id":"procurement","settlement_id":"settled-second","reported_tick":8}]}
	var profile=Profile.new(path);var loaded: Dictionary=profile.load_profile();var saved: Dictionary=profile.observe({"actor_id":receipt.earner_actor_id,"source_identity":{"world_id":receipt.world_id,"execution_epoch":0,"revision":0,"tick":8},"cards":[{"card_id":receipt.card_id,"target_actor_id":TARGET,"completed_count":2,"acquired":receipt}]},"standard")
	if not check(loaded.ok and saved.ok and profile.save_profile().ok,"new test-only profile persists explicit typed receipt"):
		picker.free(); quit(1); return
	result=picker.configure(catalog,SCENARIO,EmptyArt.new(),path);stale.call()
	check(result.ok and picker.selected().is_empty() and not button(picker,"select",TARGET).disabled,"new configure reads receipt and invalidates old locked callback")
	button(picker,"select",TARGET).pressed.emit();button(picker,"confirm").pressed.emit()
	check(selections.size()==1 and selections[0].actor_id==TARGET and selections[0].size()==4,"selection freezes original identity without adding a new world or credentials")
	var bad=FileAccess.open(path,FileAccess.WRITE);bad.store_string("broken test profile");bad.close()
	result=picker.configure(catalog,SCENARIO,EmptyArt.new(),path)
	check(result.ok and button(picker,"select",TARGET).disabled and FileAccess.get_file_as_string(path)=="broken test profile","bad profile keeps target locked and preserves original file")
	check(catalog==original,"all original actor identities assets and author input unchanged")
	var legacy: Dictionary=catalog.duplicate(true);legacy.data.scenarios[SCENARIO].erase("actor_card_unlocks")
	result=picker.configure(legacy,SCENARIO,EmptyArt.new(),path)
	check(result.ok and not button(picker,"select",TARGET).disabled,"old roster remains independent of corrupt collection file")
	picker.free(); print("ACTOR_UNLOCK_START ",checks," checks ",failures.size()," failures; WORLD_MAIN_NOT_RUN");quit(1 if not failures.is_empty() else 0)
