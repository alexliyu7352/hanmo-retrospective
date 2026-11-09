#include "money_transfer_recipe.hpp"
namespace hanmo::resident::resource_recipe {namespace {
Status shape(const StableId&payer,const StableId&payee,Integer amount){
 if(payer.empty()||payee.empty()||amount<0)
  return common_failure("InvalidOriginalMoneyTransferRecipe",payer,ErrorKind::internal);
 return common_success();
}
Status legs(MoneyTransferLegSink&sink,const StableId&payer,const StableId&payee,Integer amount,bool from,bool to){
 auto valid=sink.wallet_delta(payer,-amount,from);if(!valid.ok)return valid;
 return sink.wallet_delta(payee,amount,to);
}
class AppendLegs final:public MoneyTransferLegSink {
 ResourceRecipe& recipe_;
public:
 explicit AppendLegs(ResourceRecipe&recipe):recipe_(recipe){}
 Status wallet_delta(const StableId&id,Integer delta,bool require_spendable)override{
  recipe_.operations.push_back(WalletDelta{id,delta,require_spendable});
  return common_success();
 }
};
}
Status visit_original_money_transfer(const StableId&payer,const StableId&payee,Integer amount,MoneyTransferPolicy policy,MoneyTransferLegSink&sink){
 auto valid=shape(payer,payee,amount);if(!valid.ok)return valid;
 switch(policy){
 case MoneyTransferPolicy::spendable:return legs(sink,payer,payee,amount,true,true);
 case MoneyTransferPolicy::original_balance:return legs(sink,payer,payee,amount,false,false);
 case MoneyTransferPolicy::spendable_debit_original_credit:return legs(sink,payer,payee,amount,true,false);
 case MoneyTransferPolicy::original_debit_spendable_credit:return legs(sink,payer,payee,amount,false,true);
 default:return common_failure("UnsupportedOriginalMoneyTransferPolicy",payer,ErrorKind::internal);
 }
}
Status append_money_transfer(ResourceRecipe&r,const StableId&payer,const StableId&payee,Integer amount,MoneyTransferPolicy policy){
 AppendLegs sink(r);return visit_original_money_transfer(payer,payee,amount,policy,sink);
}
Status append_reserved_money_payment(ResourceRecipe&r,const SetReservation&update,const StableId&payee,Integer amount,ReservedMoneyPaymentOrder order){
 auto valid=shape(update.wallet_id,payee,amount);if(!valid.ok)return valid;
 if(update.action_id.empty()||update.amount<0||(update.erase&&update.amount!=0))
  return common_failure("InvalidOriginalMoneyReservationUpdate",update.wallet_id,ErrorKind::internal);
 AppendLegs sink(r);
 switch(order){
 case ReservedMoneyPaymentOrder::release_then_spendable:
  r.operations.push_back(update);return legs(sink,update.wallet_id,payee,amount,true,true);
 case ReservedMoneyPaymentOrder::release_then_original_balance:
  r.operations.push_back(update);return legs(sink,update.wallet_id,payee,amount,false,false);
 case ReservedMoneyPaymentOrder::debit_then_release:
  r.operations.push_back(WalletDelta{update.wallet_id,-amount,false});
  r.operations.push_back(update);
  r.operations.push_back(WalletDelta{payee,amount,true});break;
 default:return common_failure("UnsupportedOriginalReservedPaymentOrder",update.wallet_id,ErrorKind::internal);
 }
 return common_success();
}
}
