// 原对白确认的跨域薄协调；只复用现有prepare/apply，不建立第二提交或保存动态字典。
#pragma once
#include "world_bundle.hpp"
namespace hanmo::resident {
class CompleteExactCommandCapture;
Outcome<std::shared_ptr<const PreviewResult>> prepare_dialogue_confirmation(const WorldBundle&,const StableId&,const CommandPayload&);
Outcome<ReceiptDetails> apply_dialogue_confirmation(WorldBundle&,const StableId&,const PreparedDomainOp&,CompleteExactCommandCapture* capture=nullptr);
}
