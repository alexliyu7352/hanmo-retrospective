## 恢复原真实编组bank后按原军令出发，地图只随权威旅行位置刷新；不重演编组或修改军令。
extends "test_world_map_company_formed_main.gd"

## 既存军团经冷准入后仍由本人军务入口办理出发，地图半日按钮沿同一旅行规则。
func _run() -> void:
	var input: String=""
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--bank="):input=arg.trim_prefix("--bank=")
		if arg.begins_with("--output="):output_path=arg.trim_prefix("--output=")
	if not check(FileAccess.file_exists(input),"原生产编组bank存在"):quit(1);return
	var bytes: PackedByteArray=FileAccess.get_file_as_bytes(input)
	evidence.bank_sha256=FileAccess.get_sha256(input)
	var main=Main.new();root.add_child(main);main.size=Vector2(1280,800)
	var prepared: Dictionary=main._resident_prepare_load_candidate(bytes)
	evidence.prepared=prepared.duplicate(true)
	if not check(prepared.get("ok",false) and prepared.get("restored",false),"原Main完整准入真实编组checkpoint："+str(prepared)):await finish(main);return
	if not check(main._resident_adopt_load_candidate().get("ok",false),"正常采用已准入军旅候选"):await finish(main);return
	main._resident_frame_pending=true;main._advancing=true
	if not await settled(main):await finish(main);return
	var initial: Dictionary=main.get_meta(Company.META,{})
	var army_id: String=initial.get("page",{}).get("army",{}).get("army_id","")
	if not check(not army_id.is_empty() and initial.page.army.commanded_by_observer,"冷恢复保留实际本人统领军团"):await finish(main);return
	evidence.restored_company=initial.duplicate(true)
	if not await press(main,"map_details_open","",false) or not await press(main,"map_company_manage","",false):await finish(main);return
	if not await press(main,"camp_close","",false):await finish(main);return
	if not await press(main,"move_local","qingxi::place.north.gate"):await finish(main);return
	main._open_map()
	if not await settled(main):await finish(main);return
	if not await press(main,"map_details_open","",false) or not await press(main,"map_company_manage","",false):await finish(main);return
	if not await press(main,"campaign_depart"):await finish(main);return
	main._open_map()
	if not await settled(main):await finish(main);return
	var before: Dictionary=main.kernel.source_identity()
	var motion: Dictionary=main.kernel.map_view()
	var sidebar: Dictionary=main.kernel.journey_sidebar()
	evidence.departed_source=before;evidence.departed_map=motion.duplicate(true);evidence.departed_sidebar=sidebar.duplicate(true)
	if not check(sidebar.travel.active and not sidebar.travel.paused and not motion.get("travel_overlay",{}).is_empty(),"原军令启动真实行旅及路径投影"):await finish(main);return
	if not await press(main,"advance"):await finish(main);return
	var after: Dictionary=main.kernel.source_identity()
	var moved: Dictionary=main.kernel.map_view()
	var company: Dictionary=main.get_meta(Company.META,{})
	evidence.moved_source=after;evidence.moved_map=moved.duplicate(true);evidence.moved_company=company.duplicate(true);evidence.moved_sidebar=main.kernel.journey_sidebar().duplicate(true)
	check(after.tick==before.tick+1 and moved.get("observer_position")!=motion.get("observer_position"),"原半日使实际世界坐标前进")
	var canvas=main.find_child("WorldMapCanvas",true,false)
	check(company.get("page",{}).get("army",{}).get("army_id")==army_id and company.get("source",{}).get("identity")==after and canvas!=null and canvas.observer_label=="所领军团" and canvas.view.observer_position==moved.observer_position,"同一军团与本人标记只采用新来源实际行军坐标")
	check(FileAccess.get_file_as_bytes(input)==bytes,"输入编组bank未被后继行军改写")
	await finish(main)
