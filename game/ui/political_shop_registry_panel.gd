## 接册仅延续原记录；每项通缉、解除和签证仍由原法律办理页逐项报价。
extends RefCounted

static func prepare(host, place_id: String) -> bool:
	var kernel=host.kernel
	if not kernel.has_method("political_shop_registry_successions_enabled") or not kernel.political_shop_registry_successions_enabled(): return true
	var page: Dictionary=kernel.political_shop_registry_successions(place_id,int(host.get_meta("shop_registry_cursor",0)))
	if _pending(page): return false
	if not page.get("ok",false) or not page.get("enabled",false): return true
	var context: Dictionary=kernel.command_context()
	if _pending(context): return false
	if page.get("source_identity",{})!=kernel.source_identity() or page.get("observer_actor_id","")!=context.get("actor_id","") or page.get("place_id","")!=place_id: return true
	var ready=true
	for row in page.get("rows",[]):
		if not row.get("available",false): continue
		var quote: Dictionary=kernel.preview(row.get("command",""),row.get("params",{}),context.get("actor_id",""))
		if _pending(quote): ready=false
	return ready

static func render(host, parent: Control, page: Dictionary, place_id: String) -> void:
	if _pending(page): host._text(parent,"正在核对原司法册与接管来源…",17);return
	if not page.get("ok",false): host._text(parent,reason(page.get("code","")),17);return
	if not page.get("enabled",false): return
	var identity: Dictionary=host.kernel.source_identity()
	var context: Dictionary=host.kernel.command_context()
	if _pending(context) or page.get("source_identity",{})!=identity or page.get("observer_actor_id","")!=context.get("actor_id","") or page.get("place_id","")!=place_id:
		host._text(parent,"公职或原册来源已变化，请刷新后办理。",17);return
	host._text(parent,"原司法册接收",22)
	host._text(parent,"接册保留原通缉、解除、查核、许可与撤证记录。后续每项处分仍须单独具有办理权。",16)
	for row in page.get("rows",[]):
		host._text(parent,str(row.get("court_id","法院")),19)
		if row.has("retained_source_count"): host._text(parent,"完整保留 %d 项原册来源。"%int(row.retained_source_count),16)
		host._regional_button(parent,row.get("label","接收原完整司法册"),row.get("command",""),row.get("court_id",""),row.get("params",{}),{},not row.get("available",false))
		if not row.get("available",false): host._text(parent,reason(row.get("code","")),16)
	if page.has("next_cursor"): host._button(parent,"后续原册","shop_registry_page","next",_page.bind(host,identity.duplicate(true),context.get("actor_id",""),place_id,int(page.next_cursor)))
	if int(page.get("cursor",0))>0: host._button(parent,"最前原册","shop_registry_page","first",_page.bind(host,identity.duplicate(true),context.get("actor_id",""),place_id,0))

static func _page(host, identity: Dictionary, actor_id: String, place_id: String, cursor: int) -> void:
	if host.kernel.source_identity()!=identity: return
	var context: Dictionary=host.kernel.command_context()
	if _pending(context) or context.get("actor_id","")!=actor_id: return
	var current: Dictionary=host.kernel.political_shop_registry_successions(place_id,int(host.get_meta("shop_registry_cursor",0)))
	if _pending(current) or current.get("source_identity",{})!=identity or current.get("place_id","")!=place_id: return
	host.set_meta("shop_registry_cursor",cursor);host._render()

static func _pending(value: Dictionary) -> bool:
	return value.get("_pending",false) or value.get("code","")=="NativeAsyncRequired"

static func reason(code: String) -> String:
	return {"PoliticalShopRegistryApprovedControlRequired":"须先完成本次实际占领的公共责任裁定。", "PoliticalShopRegistryAuthorityDenied":"本人须在原柜台持有当前接册专项权。", "PoliticalShopRegistryAlreadyAccepted":"本次占领的完整原册已经接收。", "PoliticalShopRegistryAcceptanceChanged":"原接册来源已变化，请重新核对。", "PoliticalShopRegistryContinuityRequired":"辖区再次易手，须有新的明确接册来源。"}.get(code,"暂不能接册："+code)
