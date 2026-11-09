## 有界导入导出现有HM-SAVE；只处理原生文件，不改变世界规则或自行装配注册表。
extends RefCounted
const Codec=preload("res://core/save_codec.gd")

## 通过临时文件写入已验证快照；复读通过后替换目标，受信注册由装配调用方提供。
static func write_world(state: Dictionary, path: String, catalog: Dictionary, registry: Variant=null) -> Dictionary:
	if path.is_empty() or path.get_extension().to_lower()!="hmsave":
		return {"ok":false,"code":"SaveExtensionRequired"}
	var text: String=Codec.encode(state,0,"export",catalog,registry)
	if text.is_empty(): return {"ok":false,"code":"InvalidExportState"}
	var temporary: String=path+".tmp"
	var file=FileAccess.open(temporary,FileAccess.WRITE)
	if file==null: return {"ok":false,"code":"StorageWriteFailed"}
	file.store_string(text); file.flush()
	var error: Error=file.get_error()
	file.close()
	if error!=OK or FileAccess.get_file_as_string(temporary)!=text:
		return {"ok":false,"code":"ReadbackFailed"}
	var valid: Dictionary=read_world(temporary,catalog,registry)
	if not valid.ok: return valid
	if DirAccess.rename_absolute(temporary,path)!=OK:
		return {"ok":false,"code":"StorageRenameFailed"}
	return {"ok":true,"path":path,"sha256":text.sha256_text()}

## 有界读取所选文件；大小、封装和内容不匹配时拒绝，由调用方保留当前世界。
static func read_world(path: String, catalog: Dictionary, registry: Variant=null) -> Dictionary:
	var raw: Dictionary=read_text(path)
	return Codec.decode(raw.text,catalog,registry) if raw.ok else raw

## 导入只先有界读取原文；目录和世界选择留给元信息优先装配，绝不先开默认局。
static func read_text(path: String) -> Dictionary:
	var file=FileAccess.open(path,FileAccess.READ)
	if file==null: return {"ok":false,"code":"SaveFileNotFound"}
	if file.get_length()>Codec.MAX_BYTES:
		file.close()
		return {"ok":false,"code":"SaveSizeLimit"}
	var text: String=file.get_as_text();file.close()
	return {"ok":true,"text":text}
