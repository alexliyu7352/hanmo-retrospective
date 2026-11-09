# 军令预览跨边界窄门

同一次既有作者军旅场景经过正式任官、受令、编组、行旅及接敌，形成 battle:310、tick 24。没有手写军队、军令事实或墙防系数。精确输入的 C++ ResidentReplaySession.initialize 已通过完整世界准入。

同一输入的12份当前完整草案覆盖两部、朝向0/3、现有作者鼓舞及非法目标格。正式 page/draft 所有字段跨GD/C++相同；JSON对象键序不作UI协议，数组次序完整保留。独立真实 Round.preview 检查阻止两个旧页面同错通过：原页有40条失败，修后GD规则与零写检查通过；原生全部查询前后checkpoint、全state、identity不变。

真实Main使用固定4.7.2和新SO，经原_start_resident及有限队列显示同一世界。改向排query并禁用确认，正常Main导航撤销旧读取代次，旧回调排空后不发布，当前草案重新完整读取。command为0、时钟/RNG/ID/请求序号及全state保持。报告内41项检查在报告写入前冻结，stdout 42项包含最后一次成功报告写入。此门核同世界旧读代次取消，没有声称读档换世界epoch；GUI像素未验，未复跑本人档案115。

运行代码：
- `prepare.py` 复用既有军事作者生成器及正式compose/stage，复制已冻结包中的实际美术资源，资源摘要明确；不伪称恢复旧tiny。
- `game/tests/test_military_preview_joint.gd` 无`--input`时生产一次真实世界；指定`--input`只查询精确输入，`--main`另跑上述正式Main边界。`--output`需绝对路径。
- `native/world/tests/resident/military_preview_joint_test.cpp INPUT NATIVE_OUTPUT` 只读链接中央已终态静态库，不修改CMake/cache。
- `compare.py INPUT GD NATIVE REPORT` 逐字段比较且要求两端成功、零写。旧GD输出相等也因规则失败而被拒。

`receipt.json` 给出源commit、consumer/SO/输入/报告逐SHA和明确范围。大原件由中央去重归档；本分支的original证据commit保持可追溯。共同作者只有rally；其他战法和真实墙防局部门由A/B承担，不能把12例称为全战法覆盖。
