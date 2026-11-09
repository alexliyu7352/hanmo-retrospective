## 旅行代表脚本的纯输入微门；不装扩展、不造世界、不代真实Main代表验证。
extends SceneTree
const Probe=preload("res://test_native_travel_profile.gd")
var checks: int=0
var failures: int=0
## 所有反例只替换明确输入字段，失败按退出码上抛。
func expect(value: bool,label: String) -> void:
	checks+=1
	if not value:failures+=1;printerr(label)
## 从固定Engine取真实字段，最小manifest仅用于验证输入合同，不当正式stage。
func _init() -> void:
	var expected: Dictionary={"profile":"native_travel_schedule","source_commit":"a".repeat(40),"source_sha256":"b".repeat(64),"core_source_sha256":"c".repeat(64),"dependency_lock_sha256":"d".repeat(64),"api_sha256":"e".repeat(64)}
	var stage: Dictionary={"format":"HM-NATIVE-TRAVEL-STAGE/1","profile":"native-travel","gd_source":{"commit":"f".repeat(40)},"native":{"travel_build_expected":expected,"bridge_audit_expected":{}},"engine":{},"content":{"entry":{"catalog_path":"res://data/generated/alpha_life.hmcatalog.json","scenario_id":"scenario.alpha_life","content_id":"alpha"}},"storage":{"profile":"native-travel-test","custom_user_dir_name":"hanmo-native-travel-test"},"configuration":{}}
	stage.native.bridge_audit_expected=expected.duplicate();stage.native.bridge_audit_expected.erase("profile")
	var settings: Dictionary={"travel":true,"growth":false,"source_commit":stage.gd_source.commit,"expected_build":expected,"profile":stage.storage.profile,"custom_user_dir_name":stage.storage.custom_user_dir_name,"use_custom_user_dir":true}
	var version: Dictionary=Engine.get_version_info()
	expect(Probe.stage_contract(stage,settings,version).ok,"真实固定引擎与有效最小合同")
	for mode in ["growth_manifest","missing_source","wrong_native_profile","extra_native_key","missing_native_key","typed_native_key","growth_on","travel_off","travel_number","source_mismatch","build_mismatch","player_storage","storage_mismatch","engine_build","short_hash","engine_float","audit_source","player_directory","storage_bool"]:
		var bad: Dictionary=stage.duplicate(true);var config: Dictionary=settings.duplicate(true);var engine: Dictionary=version.duplicate(true)
		match mode:
			"growth_manifest":bad.format="HM-NATIVE-GROWTH-STAGE/1"
			"missing_source":bad.gd_source.erase("commit")
			"wrong_native_profile":bad.native.travel_build_expected.profile="offline_actor_commitments"
			"extra_native_key":bad.native.travel_build_expected.extra="forbidden"
			"missing_native_key":bad.native.travel_build_expected.erase("api_sha256")
			"typed_native_key":bad.native.travel_build_expected.source_commit=1
			"growth_on":config.growth=true
			"travel_off":config.travel=false
			"travel_number":config.travel=1
			"source_mismatch":config.source_commit="0".repeat(40)
			"build_mismatch":config.expected_build.source_commit="0".repeat(40)
			"player_storage":bad.storage.profile="player-alpha";config.profile="player-alpha"
			"storage_mismatch":config.custom_user_dir_name="other"
			"engine_build":engine.build="ed1daf0bf"
			"short_hash":engine.hash="ed1daf0bf"
			"engine_float":engine.major=4.0
			"audit_source":bad.native.bridge_audit_expected.source_commit="0".repeat(40)
			"player_directory":bad.storage.custom_user_dir_name="hanmo-f05";config.custom_user_dir_name="hanmo-f05"
			"storage_bool":config.use_custom_user_dir=1
		expect(not Probe.stage_contract(bad,config,engine).ok,"明确拒绝 "+mode)
	var numbers: Dictionary=JSON.parse_string('{"pointer_bits":64,"real_t_bits":32}')
	expect(typeof(numbers.pointer_bits)==TYPE_FLOAT,"实际Godot JSON将manifest整数解为float")
	expect(Probe.same_build_field("pointer_bits",64,numbers.pointer_bits),"manifest整数64与真实int64 ABI相等")
	expect(Probe.same_build_field("real_t_bits",32,numbers.real_t_bits),"manifest整数32与真实int32 ABI相等")
	for values in [["pointer_bits",64.0,64.0],["pointer_bits",64,63.5],["pointer_bits",64,"64"],["pointer_bits",64,true],["pointer_bits",32,64.0],["source_commit",64,64.0],["pointer_bits",64,INF],["pointer_bits",64,null],["pointer_bits",64,NAN]]:
		expect(not Probe.same_build_field(values[0],values[1],values[2]),"坏ABI型或值明确拒绝 "+str(values))
	print("NATIVE_TRAVEL_PROFILE_CONTRACT_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
