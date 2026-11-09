# 原店单真实交货接触 1.0

场景 shop_contact::scenario.first_shop。固定原首店 1.2 的三名客户、原采购履约、租铺、许可、订单与日租/实际结果合同；原 society 外部引用和所有原动态实体字符串不改。

本包显式选择 original_shop_order_contacts_v1 及真实生命历史。原本人接受客户店单后，实际原 provider SAY 开工才保存接触；原客户私款预约、店库实交、店金收入与失败回滚继续唯一原 writer。成功后的成本只读该次真实开工及原交付源。

没有预置商铺、三客户成功、Task、开工、死亡或完成记录。原三个 NPC subject 与多演员 API 保持，不为本包新增 NPC 交货生产者；本人实际开工需真实外层结果。

源码作者；尚未完整 World 或 GUI 验收。
