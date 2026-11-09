// 完整命令候选借原人物捕获保存发布标记；不进入CommonState或存档。
#pragma once
#include "hanmo/resident/model_contract.hpp"

namespace hanmo::resident {
class CommonStore;
// 只由完整命令捕获开启一次；Opening和普通人物捕获不获得命令发布身份。
Status begin_complete_command_publication(CommonStore &);
// 首次真实封口预留基准R+1，后续封口核同一候选和原预留值；失败丢弃整个候选。
Status reserve_complete_command_publication_revision(CommonStore &);
}
