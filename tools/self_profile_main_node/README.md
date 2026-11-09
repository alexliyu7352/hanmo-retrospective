# 本人档案 Main 只读导航门

只从正常标题继续已有沈蘅银行的独立副本，检查本人档案、原普通技艺和武学页、返回与迟到回调。没有新局、半日推进、学习、装备或产品保存操作。

复用 `formal_appearance_main_node/main_entry.gd` 的固定引擎与 Main 启动核验，以及 `resident_delivery_node/package_entry.gd` 的真实可见控件、存槽确认和正常退出辅助函数。新驱动覆写的只有继续后的只读导航流程。

`run_node.py --help` 列出全部绑定参数。执行需要同时具备新 self_profile 查询的 native 库和已接新页的 Main stage；旧库不能代替，只检查驱动编译不证明功能通过。

`--profile-open-kind self_profile_open` 对应实际 `SelfProfileMenuOpen` 菜单入口。原普通技艺和武学页返回场所后，再从正常系统菜单打开本人档案，没有新增回退栈。`--saved-world` 指向原两人节点 `produce/saved-world.json`；`--original-bank` 指向原银行文件；其余源码、内容与库参数均须填本次新stage确值。

wrapper 把固定引擎以同SHA硬链接或副本放入输出目录，并设置独立 `_sc_`、XDG 和临时目录，不写共享编辑器缓存。关键六份产品源码须与声明的完整Git提交逐字一致；实际扩展的Linux debug库路径也必须就是所核验的SO。

原银行 `manual-1.B.hmcheckpoint` 的 SHA256 必须是 `e52025590173c4258fde867bf72068d78a53db2957abdec5bcff5c08ec06b28e`。wrapper 复制它到新 XDG 状态目录，前后核验原件与副本；原银行不作写入目标。恢复后的完整世界与原两人节点 `produce/saved-world.json` 逐字一致。

检查当前实际人物、五项属性、十项经验和 native 等级，前后完整世界 JSON、全部 RNG、tick、请求序号、上下文、来源身份和冷 checkpoint 字节必须不变。只有正常控件导航，零世界命令。证据写入全新目录，失败不覆盖旧回执。GUI、鼠标、截图和导出包均未在此门验收。
