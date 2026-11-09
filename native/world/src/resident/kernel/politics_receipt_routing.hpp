// 共用PoliticsEffects按原命令所属域分派；此口不代替任何领域来源校验。
#pragma once
#include "resident/model/common_model.hpp"
namespace hanmo::resident {
struct PoliticsEffects;
// 只由同World已存在的显式目录和状态成对置真，各本域晚门仍必须执行。
struct PoliticsReceiptRoutes {
 bool succession=false,head_handover=false,successor_round=false,term_handover=false;
 bool office_reception=false,office_resignation=false,governance_agency=false,subordinate_appointment=false;
};
// 原五命令和未知PoliticsEffects原样保留；仅精确已启新域的命令交该域晚门。
const PoliticsEffects* politics_receipt_for_original(const CommitResult&,const PoliticsReceiptRoutes&);
}
