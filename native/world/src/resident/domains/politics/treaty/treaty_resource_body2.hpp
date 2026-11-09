#pragma once
#include "treaty_aid_accepted_resource_source2.hpp"
#include "treaty_aid_recipient_acceptance_model.hpp"
#include "treaty_aid_resource_proposal2.hpp"
namespace hanmo::resident {
// World 仍只拥有 PoliticalTreatyTermsState；这四表是其显式 /2 内部值，
// 不另造一个可独立发布的条约根。激活/终止仍用原父体拥有的真实表。
struct PoliticalTreatyResourceBody2 {
  OrderedTable<TreatyAidResourceProposalSource2> proposals;
  OrderedTable<TreatyResourceSignatureSource2> signatures;
  OrderedTable<TreatyAidRecipientAcceptance> recipient_acceptances;
  OrderedTable<TreatyAidAcceptedResourceSource2> accepted_aid_resources;
};
} // namespace hanmo::resident
