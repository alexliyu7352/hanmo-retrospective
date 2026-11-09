// Past production is read from original receipts. No account identifiers or cause-fact graph is exposed.
#pragma once
#include "local_economy_view.hpp"
namespace hanmo::resident {
struct ProductionHistoryCursor { LocalEconomyCursor source; StableId unit_id; };
struct ProductionHistoryQuery {
    StableId observer_actor_id,place_id,unit_id;
    std::optional<ActingAuthority> authority;
    std::optional<ProductionHistoryCursor> cursor;
};
struct ProductionHistoryQuantity { StableId item_id; Integer quantity=0; };
struct ProductionHistoryReceipt {
    StableId receipt_id;
    Integer tick=0,due_tick=0,allocated_workers=0,effective_workers=0,batches=0;
    std::vector<ProductionHistoryQuantity> inputs,outputs,unrealized_output;
    std::optional<ProductionContext> context;
};
struct ProductionHistoryPage {
    WorldIdentity identity;
    StableId observer_actor_id,place_id,unit_id;
    std::vector<ProductionHistoryReceipt> rows;
    std::optional<ProductionHistoryCursor> next_cursor;
};
// Called at the same final-identity boundary as the ordinary economy index, never in a query.
void index_production_history(const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,LocalEconomyIndex&);
void index_production_history(const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,LocalEconomyIndex&,const EconomicTakeoverReadContext*);
Outcome<ProductionHistoryPage> query_production_history(const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,const OrganizationRead&,const LocalEconomyIndex&,const ProductionHistoryQuery&);
Outcome<ProductionHistoryQuery> decode_production_history_query(const BoundaryValue&,const StableId& observer_actor_id);
BoundaryValue encode_production_history_page(const ProductionHistoryPage&);
}
