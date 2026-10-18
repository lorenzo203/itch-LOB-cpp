#include "order_book.hpp"

namespace itch {

// Unlink an OrderNode from its PriceLevel's intrusive doubly-linked list in O(1)
void LimitOrderBook::unlink_node(OrderNode& node, PriceLevel& level) noexcept {
    if (node.prev_idx != NULL_NODE) {
        pool_.get(node.prev_idx).next_idx = node.next_idx;
    } else {
        // Node was the head of the queue
        level.head_idx = node.next_idx;
    }

    if (node.next_idx != NULL_NODE) {
        pool_.get(node.next_idx).prev_idx = node.prev_idx;
    } else {
        // Node was the tail of the queue
        level.tail_idx = node.prev_idx;
    }

    node.prev_idx = NULL_NODE;
    node.next_idx = NULL_NODE;
}

// Type 'A' and 'F': Insert a new resting limit order at the tail of its price level (FIFO)
void LimitOrderBook::on_add_order(uint64_t order_id, char side, uint32_t shares, uint32_t price) {
    // 1. Find or create the PriceLevel in the appropriate tree (Bid or Ask)
    PriceLevel* level_ptr = nullptr;
    if (side == 'B') {
        auto [it, inserted] = bids_.try_emplace(price, PriceLevel{price, 0, NULL_NODE, NULL_NODE});
        level_ptr = &(it->second);
    } else {
        auto [it, inserted] = asks_.try_emplace(price, PriceLevel{price, 0, NULL_NODE, NULL_NODE});
        level_ptr = &(it->second);
    }

    // 2. Allocate a pre-existing node slot from the pool in O(1)
    const uint32_t node_idx = pool_.allocate();
    OrderNode& node = pool_.get(node_idx);

    node.order_id     = order_id;
    node.shares       = shares;
    node.price        = price;
    node.side         = side;
    node.parent_level = level_ptr;

    // 3. Append node to the tail of the PriceLevel's FIFO doubly-linked list
    node.prev_idx = level_ptr->tail_idx;
    node.next_idx = NULL_NODE;

    if (level_ptr->tail_idx != NULL_NODE) {
        pool_.get(level_ptr->tail_idx).next_idx = node_idx;
    } else {
        // Queue was empty; this node becomes the head as well
        level_ptr->head_idx = node_idx;
    }
    level_ptr->tail_idx = node_idx;

    // 4. Update aggregate volume at this price level and register in Hash Map
    level_ptr->total_volume += shares;
    order_map_[order_id] = node_idx;
}

// Type 'E' and 'C': Match against a resting order (partial or full fill)
void LimitOrderBook::on_order_executed(uint64_t order_id, uint32_t executed_shares) {
    // executing shares has the exact same effect on resting book liquidity as cancelling shares
    on_order_cancel(order_id, executed_shares);
}

// Type 'X': Partial reduction of shares on a resting order
void LimitOrderBook::on_order_cancel(uint64_t order_id, uint32_t cancelled_shares) {
    auto it = order_map_.find(order_id);
    if (it == order_map_.end()) {
        return; // Order not tracked
    }

    const uint32_t node_idx = it->second;
    OrderNode& node = pool_.get(node_idx);
    PriceLevel* level = node.parent_level;

    // If reduction depletes the entire order, remove it completely
    if (cancelled_shares >= node.shares) {
        on_order_delete(order_id);
        return;
    }

    // Otherwise, reduce shares in O(1) via direct parent_level pointer
    node.shares -= cancelled_shares;
    level->total_volume -= cancelled_shares;
}

// Type 'D': Complete removal of a resting order
void LimitOrderBook::on_order_delete(uint64_t order_id) {
    auto it = order_map_.find(order_id);
    if (it == order_map_.end()) {
        return;
    }

    const uint32_t node_idx = it->second;
    OrderNode& node = pool_.get(node_idx);
    PriceLevel* level = node.parent_level;

    // 1. Subtract remaining volume and unlink from FIFO queue in O(1)
    level->total_volume -= node.shares;
    unlink_node(node, *level);

    // 2. If no orders remain at this price level, erase the level from the book
    if (level->head_idx == NULL_NODE) {
        if (node.side == 'B') {
            bids_.erase(node.price);
        } else {
            asks_.erase(node.price);
        }
    }

    // 3. Return node slot to the pool and erase ID from Hash Map
    pool_.deallocate(node_idx);
    order_map_.erase(it);
}

// Type 'U': Replace an existing order with a new Order ID, quantity, and price
void LimitOrderBook::on_order_replace(uint64_t old_id, uint64_t new_id, uint32_t new_shares, uint32_t new_price) {
    auto it = order_map_.find(old_id);
    if (it == order_map_.end()) {
        return;
    }

    // Retrieve the side ('B' or 'S') from the old order before deleting it!
    const char side = pool_.get(it->second).side;

    on_order_delete(old_id);
    on_add_order(new_id, side, new_shares, new_price);
}

} // namespace itch