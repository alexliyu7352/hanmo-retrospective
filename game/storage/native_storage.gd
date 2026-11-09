## 原生双代存储；先按物理封装选代，再独立解码，绝不因语义失败悄悄回退。
extends RefCounted
const Codec = preload("res://core/save_codec.gd")
const ResidentCodec = preload("res://storage/resident_slot_codec.gd")
var directory: String
var catalog: Dictionary = {}
var registry: Variant=null
var _resident_profile: bool=false

## 选择隔离存储目录并复制受信目录；此适配器不支持多个原生写者同时提交。
func _init(path: String = "user://f02-1/saves", content: Dictionary = {}, accepted_registry: Variant=null, resident_profile: bool=false) -> void:
	catalog = content.duplicate(true)
	registry=accepted_registry.duplicate(true) if accepted_registry is Dictionary else accepted_registry
	directory = ProjectSettings.globalize_path(path) if path.begins_with("user://") else path
	_resident_profile=resident_profile

## 常驻包装使用独立扩展；不会把旧HM-SAVE银行当坏档或覆盖。
func _extension() -> String:
	return ".hmcheckpoint" if _resident_profile else ".hmsave"

## 槽名必须完整匹配受信命名空间；路径、尾换行和相似别名均不能成为文件入口。
static func valid_slot(slot: String) -> bool:
	var expression = RegEx.new()
	expression.compile("^(auto-[1-3]|manual-([1-9]|10))(?![\\s\\S])")
	return expression.search(slot) != null

## 只读银行原text与真实文件时间；mtime仅供展示，打不开或资源超限不能伪装成物理损坏。
func _read_bank(slot: String, suffix: String) -> Dictionary:
	var path: String=directory.path_join(slot+"."+suffix+_extension())
	if not FileAccess.file_exists(path): return {"ok":false,"code":"NotFound","modified_unix":0}
	var modified_unix: int=FileAccess.get_modified_time(path)
	var file=FileAccess.open(path,FileAccess.READ)
	if file==null: return {"ok":false,"code":"StorageReadFailed","physical_corruption":false,"suffix":suffix,"modified_unix":modified_unix}
	if file.get_length()>Codec.MAX_BYTES:
		file.close()
		return {"ok":false,"code":"SaveSizeLimit","physical_corruption":false,"suffix":suffix,"modified_unix":modified_unix}
	var text: String=file.get_as_text()
	var read_error: Error=file.get_error()
	file.close()
	if not read_error in [OK,ERR_FILE_EOF]: return {"ok":false,"code":"StorageReadFailed","physical_corruption":false,"suffix":suffix,"modified_unix":modified_unix}
	var inspected: Dictionary=ResidentCodec.inspect_metadata(text) if _resident_profile else Codec.inspect_metadata(text)
	if not inspected.ok:
		inspected.suffix=suffix
		inspected.modified_unix=modified_unix
		return inspected
	if inspected.save_id!=slot:
		return {"ok":false,"code":"SlotIdentityMismatch","physical_corruption":false,"suffix":suffix,"save_id":inspected.save_id,"modified_unix":modified_unix}
	return {"ok":true,"text":text,"metadata":inspected.metadata,"generation":inspected.generation,"snapshot_generation":inspected.generation,"suffix":suffix,"modified_unix":modified_unix}

## A/B只按已验证物理generation选代；任一未知/非物理错误都停止，只有明确损坏可回读另一代。
func inspect_slot(slot: String) -> Dictionary:
	if not valid_slot(slot): return {"ok":false,"code":"InvalidSlot","modified_unix":0}
	var latest: Dictionary={}
	var corrupt: Array=[]
	for suffix in ["A","B"]:
		var inspected: Dictionary=_read_bank(slot,suffix)
		if not inspected.ok:
			if inspected.get("code")=="NotFound": continue
			if inspected.get("physical_corruption",false)!=true: return inspected
			corrupt.append({"suffix":suffix,"code":inspected.code})
			continue
		if not latest.is_empty() and inspected.generation==latest.generation and inspected.text!=latest.text:
			return {"ok":false,"code":"GenerationConflict","physical_corruption":false,"generation":inspected.generation,"suffixes":[latest.suffix,suffix],"modified_unix":0}
		if latest.is_empty() or inspected.generation>latest.generation: latest=inspected
	if latest.is_empty():
		if corrupt.is_empty(): return {"ok":false,"code":"NotFound","generation":0,"modified_unix":0}
		return {"ok":false,"code":"CorruptSlot","physical_corruption":true,"corrupt_generations":corrupt,"modified_unix":0}
	latest.recovered=not corrupt.is_empty()
	if not corrupt.is_empty(): latest.corrupt_generations=corrupt
	return latest

## 兼容读取只解码已选中的最高物理代；语义失败原码返回，不把缺包/不兼容转成CorruptSlot。
func get_slot(slot: String) -> Dictionary:
	if _resident_profile: return {"ok":false,"code":"NativeValidationRequired"}
	var inspected: Dictionary=inspect_slot(slot)
	if not inspected.ok: return inspected
	var decoded: Dictionary=Codec.decode(inspected.text,catalog,registry)
	if not decoded.ok: return decoded
	return inspected

## 先验证候选及旧最高代，再写临时另一银行并回读提升；旧代语义失败同样阻止覆盖。
func put(slot: String, text: String, expected_generation: int) -> Dictionary:
	if _resident_profile: return {"ok":false,"code":"NativeValidationRequired"}
	if not valid_slot(slot):
		return {"ok":false,"code":"InvalidSlot"}
	var decoded: Dictionary = Codec.decode(text,catalog,registry)
	if not decoded.ok:
		return decoded
	if decoded.save_id != slot or decoded.generation != expected_generation+1:
		return {"ok":false,"code":"InvalidGeneration"}
	var previous: Dictionary = get_slot(slot)
	if not previous.ok and previous.code != "NotFound":
		return previous
	var actual: int = previous.generation if previous.ok else 0
	if actual != expected_generation:
		return {"ok":false,"code":"GenerationConflict","generation":actual}
	return _write_bank(slot,text,previous)

## 同worker语义成功只绑定精确新旧payload；本层仍只负责物理代数，不自行恢复世界。
func put_resident(slot: String, text: String, expected_generation: int, validation: Dictionary) -> Dictionary:
	if not _resident_profile: return {"ok":false,"code":"NativeProfileRequired"}
	if not valid_slot(slot): return {"ok":false,"code":"InvalidSlot"}
	if typeof(validation.get("ok"))!=TYPE_BOOL: return {"ok":false,"code":"InvalidNativeValidation"}
	if not validation.ok: return validation.duplicate(true)
	for key in ["candidate_sha256","previous_sha256","previous_text_sha256"]:
		if typeof(validation.get(key))!=TYPE_STRING: return {"ok":false,"code":"InvalidNativeValidation"}
	for key in ["candidate_continuation_sha256","previous_continuation_sha256"]:
		if validation.has(key) and typeof(validation[key])!=TYPE_STRING: return {"ok":false,"code":"InvalidNativeValidation"}
	var candidate: Dictionary=ResidentCodec.inspect_metadata(text)
	if not candidate.ok: return candidate
	if candidate.save_id!=slot or candidate.generation!=expected_generation+1: return {"ok":false,"code":"InvalidGeneration"}
	if validation.candidate_sha256!=candidate.metadata.payload_sha256 or validation.get("candidate_continuation_sha256","")!=candidate.metadata.get("continuation_sha256",""): return {"ok":false,"code":"NativeValidationMismatch"}
	var previous: Dictionary=_resident_previous(slot,expected_generation,validation)
	if not previous.ok and previous.code!="NotFound": return previous
	return _write_bank(slot,text,previous,validation)

## 改写前及tmp验证后都复读选中代；任何未知/不兼容银行仍沿inspect原错误停止。
func _resident_previous(slot: String, expected_generation: int, validation: Dictionary) -> Dictionary:
	var previous: Dictionary=inspect_slot(slot)
	if not previous.ok and previous.code!="NotFound": return previous
	var generation: int=previous.generation if previous.ok else 0
	if generation!=expected_generation: return {"ok":false,"code":"GenerationConflict","generation":generation}
	var text_sha: String=previous.text.sha256_text() if previous.ok else ""
	var payload_sha: String=previous.metadata.payload_sha256 if previous.ok else ""
	var continuation_sha: String=previous.metadata.get("continuation_sha256","") if previous.ok else ""
	if validation.previous_text_sha256!=text_sha or validation.previous_sha256!=payload_sha or validation.get("previous_continuation_sha256","")!=continuation_sha:
		return {"ok":false,"code":"NativeValidationMismatch"}
	return previous

## 两profile共用唯一tmp/flush/readback/rename尾；常驻还在提升前复核已语义验证的旧代字节。
func _write_bank(slot: String, text: String, previous: Dictionary, validation: Dictionary={}) -> Dictionary:
	if DirAccess.make_dir_recursive_absolute(directory) != OK:
		return {"ok":false,"code":"StorageDirectory"}
	var suffix: String = "A" if previous.ok and previous.suffix == "B" else "B"
	var path: String = directory.path_join(slot+"."+suffix+_extension())
	var temporary: String = path+".tmp"
	var file = FileAccess.open(temporary,FileAccess.WRITE)
	if file == null:
		return {"ok":false,"code":"StorageWriteFailed"}
	file.store_string(text)
	file.flush()
	var write_error: Error = file.get_error()
	file.close()
	var read_text: String=FileAccess.get_file_as_string(temporary)
	var verified: Dictionary=ResidentCodec.inspect_metadata(read_text) if _resident_profile else Codec.decode(read_text,catalog,registry)
	if write_error != OK or read_text != text or not verified.ok:
		return {"ok":false,"code":"ReadbackFailed"}
	if _resident_profile:
		var checked: Dictionary=_resident_previous(slot,previous.generation if previous.ok else 0,validation)
		if not checked.ok and checked.code!="NotFound": return checked
	if DirAccess.rename_absolute(temporary,path) != OK:
		return {"ok":false,"code":"StorageRenameFailed"}
	var readback: Dictionary = inspect_slot(slot) if _resident_profile else get_slot(slot)
	if not readback.ok or readback.text != text:
		return {"ok":false,"code":"ReadbackFailed"}
	return readback

## 列表透传物理选中代与文件时间；无确定银行的时间为0，缺包/不兼容不改变选代。
func list_slots() -> Dictionary:
	var slots: Array = []
	for prefix in ["auto","manual"]:
		var count: int = 3 if prefix == "auto" else 10
		for index in range(1,count+1):
			var slot: String = prefix+"-"+str(index)
			var result: Dictionary = inspect_slot(slot)
			if result.get("code","") == "NotFound":
				continue
			var entry: Dictionary={"slot":slot,"ok":result.ok,"generation":result.get("generation",0),"code":result.get("code",""),"modified_unix":result.get("modified_unix",0)}
			if result.ok:
				entry.metadata=result.metadata
				entry.recovered=result.recovered
			elif result.has("physical_corruption"):
				entry.physical_corruption=result.physical_corruption
			slots.append(entry)
	return {"ok":true,"slots":slots}
