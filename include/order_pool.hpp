#pragma once

#include <vector>
#include <cstdint>
#include <stdexcept>

namespace itch {

// We use 32-bit indices instead of 64-bit pointers to save L1 cache space.
constexpr uint32_t NULL_NODE = 0xFFFFFFFF;

struct PriceLevel; // Forward declaration

struct OrderNode {
    uint64_t    order_id;
    uint32_t    shares;
    uint32_t    price;
    char        side;       // 'B' (Buy) or 'S' (Sell)
    
    // Intrusive doubly-linked list pointers (as indices)
    uint32_t    prev_idx{NULL_NODE};
    uint32_t    next_idx{NULL_NODE};
    
    // The HFT Trick: Direct pointer to the parent price level
    PriceLevel* parent_level{nullptr}; 
};

// Object Pool to avoid dynamic memory allocation during trading hours
class OrderPool {
private:
    std::vector<OrderNode> pool_;
    uint32_t free_head_{NULL_NODE};

public:
    explicit OrderPool(size_t capacity = 2000000) {
        pool_.resize(capacity);
        // Initialize the free-list: every node points to the next one
        for (uint32_t i = 0; i < capacity - 1; ++i) {
            pool_[i].next_idx = i + 1;
        }
        pool_[capacity - 1].next_idx = NULL_NODE;
        free_head_ = 0;
    }

    [[nodiscard]] uint32_t allocate() {
        if (free_head_ == NULL_NODE) {
            throw std::runtime_error("OrderPool exhausted! Increase capacity.");
        }
        const uint32_t allocated_idx = free_head_;
        free_head_ = pool_[allocated_idx].next_idx; // Pop from free-list
        
        // Clean up links for safety
        pool_[allocated_idx].prev_idx = NULL_NODE;
        pool_[allocated_idx].next_idx = NULL_NODE;
        return allocated_idx;
    }

    void deallocate(uint32_t index) noexcept {
        // Push back onto the free-list
        pool_[index].next_idx = free_head_;
        free_head_ = index;
    }

    [[nodiscard]] OrderNode& get(uint32_t index) noexcept {
        return pool_[index];
    }
};

} // namespace itch