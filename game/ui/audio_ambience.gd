## 可选配乐和交互音；声音设置与世界存档状态分离。
extends Node
var enabled: bool = true
var music := AudioStreamPlayer.new()
var effect := AudioStreamPlayer.new()
var _settings: String = "user://presentation.cfg"
## 读取本地偏好并加载音频；无窗口测试不要求实际声音设备。
func _ready() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--profile=") and arg!="--profile=default": _settings="user://presentation-development.cfg"
	var cfg := ConfigFile.new()
	if cfg.load(_settings)==OK: enabled=bool(cfg.get_value("audio","enabled",true))
	music.stream=load("res://assets/audio/ambience.wav")
	if music.stream is AudioStreamWAV:
		music.stream.loop_mode=AudioStreamWAV.LOOP_FORWARD
		music.stream.loop_end=int(music.stream.get_length()*music.stream.mix_rate)
	music.volume_db=-15; effect.volume_db=-20
	effect.stream=load("res://assets/audio/paper.wav")
	add_child(music);add_child(effect)
	apply()
## 只改变声音输出，不影响时钟、随机数流或待执行行动。
func apply() -> void:
	if enabled and DisplayServer.get_name()!="headless":
		if not music.playing:music.play()
	else:music.stop();effect.stop()
## 把用户音效选择保存在独立的本地配置文件。
func toggle() -> void:
	enabled=not enabled;apply()
	var cfg:=ConfigFile.new();cfg.set_value("audio","enabled",enabled);cfg.save(_settings)

## 播放一次轻量反馈，不另行发起行动或改动世界状态。
func click() -> void:
	if enabled and DisplayServer.get_name()!="headless":effect.play()
