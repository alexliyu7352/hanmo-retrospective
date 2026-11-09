// 原D1.1收讫引用的同候选采用；财务量、货物与付款仍由S5/S3原owner持有。
#pragma once
#include "treaty_aid_occurrence.hpp"
#include "resident/domains/local_economy/material_aid/material_aid.hpp"
namespace hanmo::resident {
struct TreatyAidReceiptAdopted {
 SourceRef occurrence_binding_ref,actual_receipt_result_ref;
};
// 只核原S5已采用receipt与S6既有字段一一覆盖，不将发出/在途当收讫。
// 全世界还须调用各原owner的完整validator及实际源反向覆盖。
Status validate_treaty_aid_receipt_adoption(const MaterialAidReadContext&,
 const TreatyAidPerformanceReadContext&);
// 唯一实际上游event的全部发出/收讫先由S5原writer原子消费，之后采用其每个
// 真receipt。实际source可含跨期次腿，同一次消费全纳；纯issue合法返回空。
// 原shipment_ref不从货腿列表选取；它若已由原Shipment明确采用则继续严格核。
// 此口仅供未发布World候选；任一步失败必须连实际上游变更与本次两域写入一起撤。
Outcome<std::vector<TreatyAidReceiptAdopted>> consume_and_adopt_treaty_aid_actual_event_in_candidate(
 CommonStore&,TreatyMaterialAidState&,OrderedTable<TreatyAidOccurrenceBinding>&,
 const MaterialAidWriteContext&,const TreatyAidPerformanceReadContext&,
 const SourceRef& actual_event);
}
