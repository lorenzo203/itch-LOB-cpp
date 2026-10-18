#pragma once

#include "order_pool.hpp"
#include <map>
#include <unordered_map>

namespace itch {

struct PriceLevel {
    uint32_t price{0};
    uint64_t total_volume{0};
    
    // Head and tail of the doubly-linked list of orders at this price
    uint32_t head_idx{NULL_NODE};
    uint32_t tail_idx{NULL_NODE};
};

class LimitOrderBook {
private:
    OrderPool pool_;
    
    // O(1) lookup: Order ID -> Index in the pool
    std::unordered_map<uint64_t, uint32_t> order_map_;
    
    // Ordered maps for Price Levels (Bid: descending, Ask: ascending)
    std::map<uint32_t, PriceLevel, std::greater<uint32_t>> bids_;
    std::map<uint32_t, PriceLevel, std::less<uint32_t>> asks_;

    // Internal helper to unlink a node from its price level's list
    void unlink_node(OrderNode& node, PriceLevel& level) noexcept;

public:
    explicit LimitOrderBook(size_t pool_capacity = 2000000) 
        : pool_(pool_capacity) {
        order_map_.reserve(pool_capacity);
    }

    // Process ITCH messages
    void on_add_order(uint64_t order_id, char side, uint32_t shares, uint32_t price);
    void on_order_executed(uint64_t order_id, uint32_t executed_shares);
    void on_order_cancel(uint64_t order_id, uint32_t cancelled_shares);
    void on_order_delete(uint64_t order_id);
    void on_order_replace(uint64_t old_id, uint64_t new_id, uint32_t new_shares, uint32_t new_price);

    // Read-only accessors for TUI / Analytics
    [[nodiscard]] const auto& get_bids() const noexcept { return bids_; }
    [[nodiscard]] const auto& get_asks() const noexcept { return asks_; }
    [[nodiscard]] size_t active_orders_count() const noexcept { return order_map_.size(); }
};

} // namespace itch