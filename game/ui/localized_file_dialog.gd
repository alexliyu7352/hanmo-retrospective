## 为Godot原生文件框提供局部中文翻译；文件访问、过滤、确认与取消仍由原控件负责。
extends FileDialog
const DOMAIN: StringName=&"hanmo.file_dialog"
const MESSAGES: Dictionary={
	"Open a File":"导入存档", "Open File(s)":"选择文件", "Open a Directory":"选择目录",
	"Open a File or Directory":"选择文件或目录", "Save a File":"导出存档",
	"Open":"打开", "Save":"保存", "Cancel":"取消", "OK":"确定", "Select Current Folder":"选择当前目录",
	"Path:":"路径：", "Favorites:":"收藏：", "Recent:":"最近访问：", "Directories & Files:":"目录与文件：",
	"Directories:":"目录：", "File:":"文件：", "Filter:":"筛选：", "Search":"搜索", "Name:":"名称：",
	"All Files":"所有文件", "All Files (*.*)":"所有文件 (*.*)", "All Recognized":"所有支持的文件", "Create Folder":"新建文件夹",
	"Please Confirm...":"请确认", "Alert!":"提示", "File \"%s\" already exists.\nDo you want to overwrite it?":"文件“%s”已存在。\n是否覆盖？",
	"Could not create folder.":"无法创建文件夹。", "Invalid extension, or empty filename.":"文件扩展名无效，或尚未填写文件名。",
	"Go to previous folder.":"返回上一个目录。", "Go to next folder.":"前往下一个目录。", "Go to parent folder.":"前往上级目录。",
	"Refresh files.":"刷新文件。", "(Un)favorite current folder.":"收藏或取消收藏当前目录。", "Create a new folder.":"新建文件夹。",
	"Toggle the visibility of hidden files.":"显示或隐藏隐藏文件。", "View items as a grid of thumbnails.":"以缩略图查看。",
	"View items as a list.":"以列表查看。", "Toggle the visibility of the filter for file names.":"显示或隐藏文件名筛选。",
	"Sort files":"文件排序", "Sort by Name (Ascending)":"名称升序", "Sort by Name (Descending)":"名称降序",
	"Sort by Type (Ascending)":"类型升序", "Sort by Type (Descending)":"类型降序",
	"Sort by Modified Time (Newest First)":"修改时间：最新在前", "Sort by Modified Time (Oldest First)":"修改时间：最早在前",
	"Drive":"磁盘", "Network":"网络", "Filename Filter:":"文件名筛选：", "New Folder...":"新建文件夹…",
	"Refresh":"刷新", "Select This Folder":"选择此目录", "Show Package Contents":"显示软件包内容",
	"You don't have permission to access contents of this folder.":"没有权限访问此目录的内容。",
	"Copy Path":"复制路径", "Copy":"复制", "Cut":"剪切", "Paste":"粘贴", "Select All":"全选", "Clear":"清空",
	"Undo":"撤销", "Redo":"重做", "Delete":"删除", "Show in File Manager":"在文件管理器中显示",
	"Delete the selected file?\nDepending on your filesystem configuration, the files will either be moved to the system trash or deleted permanently.":"是否删除所选文件？\n根据文件系统配置，文件将移至回收站或被永久删除。"
}

## 注册一次独立翻译域并由子控件继承；不修改操作系统或游戏其余部分的语言设置。
func _init() -> void:
	if not TranslationServer.has_domain(DOMAIN):
		var domain: TranslationDomain=TranslationServer.get_or_add_domain(DOMAIN)
		var translation=Translation.new(); translation.locale="zh_CN"
		for message in MESSAGES: translation.add_message(message,MESSAGES[message])
		domain.add_translation(translation)
		domain.set_locale_override("zh_CN")
	set_translation_domain(DOMAIN)
