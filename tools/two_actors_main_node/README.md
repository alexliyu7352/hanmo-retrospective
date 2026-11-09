# 两人 Main 实际接线节点

沿 `tools/formal_appearance_main_node` 的固定编辑器/source stage两进程方法运行。Main与本驱动必须对应当前两人开局源码；当前1.8目录锁为 `85073906b204952b66c9cf81a7963b5f39d6b75ad6a47de85f3112320f3914d7`。新原生库必须另核实际SHA，不能使用旧组合节点的库冒充。

`python3 tools/two_actors_main_node/run_node.py --help` 查看参数。沿用正式Main节点的 `--project`、`--output`、`--editor`、`--expected-editor-sha256`、`--native-library`、`--expected-native-sha256`、`--source-commit`、`--catalog`、`--pack-set-digest` 参数。

当前验收只运行沈蘅一个代表的producer/consumer。沈蘅路线先实际选择程禾进入192组合窗口、选择非preset并取消，核同一空Kernel及零银行/原生安装，再返回正常标题选沈蘅创建和存读。

通用 `--actor` 支持以下两项；程禾单独完整存读路线不在本次执行范围：

- `society::actor.apprentice`：显式人物选择和取消后重开，进入原192组合控件并选择非preset外貌。
- `society::actor.study_swordswoman`：显式人物选择和取消后重开，直接沿原人物固定外貌启程，无组合窗口或appearance确认。

两条路线均检查原空世界/零原生backend候选/零银行直到相应确认，检查单一新世界的实际人物ID、原出生能力及完整外貌，再走正常菜单保存、不同PID继续和完整世界JSON逐字比较。复用原 `package_entry.gd` 的真实可见按钮、菜单、存槽和退出方法；`main_entry.gd` 另继承正式组合driver的只读观察函数。wrapper同时记录三份driver源码身份、stage非缓存文件和原生库身份。

所有世界写入只能来自Main正常控件。编译检查不等于这两条路线运行通过；GUI截图/鼠标和导出包均不属于该source stage门。
