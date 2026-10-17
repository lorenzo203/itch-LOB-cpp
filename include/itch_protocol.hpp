#pragma once

#include <cstdint>

namespace itch {

// Message type identifiers
// Size of each message struct obtained in the official giude 
// at: https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/NQTVITCHSpecification.pdf
constexpr char MSG_STOCK_DIRECTORY      = 'R';
constexpr char MSG_ADD_ORDER            = 'A';
constexpr char MSG_ADD_ORDER_MPID       = 'F';
constexpr char MSG_ORDER_EXECUTED       = 'E';
constexpr char MSG_ORDER_EXECUTED_PRICE = 'C';
constexpr char MSG_ORDER_CANCEL         = 'X';
constexpr char MSG_ORDER_DELETE         = 'D';
constexpr char MSG_ORDER_REPLACE        = 'U';

#pragma pack(push, 1)

// Type 'R': Stock Directory (39 bytes)
// Maps an 8-char ticker symbol to a 2-byte numeric stock_locate ID
struct ItchStockDirectory {
    char     msg_type;               // 'R'
    uint16_t stock_locate;           // Numeric 2-Bytes ID for this security 
                                     // -> Ticker alternative used to filter securities   
    uint16_t tracking_number;        // Internal NASDAQ tracking number
    uint8_t  timestamp[6];           // Nanoseconds since midnight
    char     stock[8];               // Space-padded ticker (e.g., "AAPL    ")
    char     market_category;        // Listing market category
    char     financial_status;       // Compliance status
    uint32_t round_lot_size;         // Shares per round lot (usually 100)
    char     round_lots_only;        // 'Y' or 'N'
    char     issue_classification;   // Security type code
    char     issue_subtype[2];       // Security subtype code
    char     authenticity;           // 'P' (Production) or 'T' (Test)
    char     short_sale_threshold;   // Reg SHO indicator
    char     ipo_flag;               // IPO indicator
    char     luld_price_tier;        // Limit Up-Limit Down tier
    char     etp_flag;               // Exchange Traded Product flag
    uint32_t etp_leverage_factor;    // Leverage factor for ETPs
    char     inverse_indicator;      // 'Y' or 'N'
};

// Type 'A': Add Order - No MPID Attribution (36 bytes)
struct ItchAddOrder {
    char     msg_type;               // 'A'
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint8_t  timestamp[6];
    uint64_t order_ref;              // Unique order ID assigned by NASDAQ
    char     side;                   // 'B' (Buy) or 'S' (Sell)
    uint32_t shares;                 // Order quantity
    char     stock[8];               // Space-padded ticker
    uint32_t price;                  // Price with 4 implied decimals (x10000)
};

// Type 'F': Add Order - With MPID Attribution (40 bytes)
// Identical layout to 'A' for the first 36 bytes, plus 4-char MPID
struct ItchAddOrderMpid {
    char     msg_type;               // 'F'
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint8_t  timestamp[6];
    uint64_t order_ref;
    char     side;                   // 'B' or 'S'
    uint32_t shares;
    char     stock[8];
    uint32_t price;
    char     attribution[4];         // Market Participant ID (broker code)
};

// Type 'E': Order Executed (31 bytes)
// Reduces remaining shares of an existing order in the book
struct ItchOrderExecuted {
    char     msg_type;               // 'E'
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint8_t  timestamp[6];
    uint64_t order_ref;              // ID of the resting order being executed
    uint32_t executed_shares;        // Quantity matched
    uint64_t match_number;           // Unique trade match ID
};

// Type 'C': Order Executed With Price (36 bytes)
// Same impact on LOB volume as 'E', but includes execution price
struct ItchOrderExecutedPrice {
    char     msg_type;               // 'C'
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint8_t  timestamp[6];
    uint64_t order_ref;
    uint32_t executed_shares;
    uint64_t match_number;
    char     printable;              // 'Y' or 'N' for tape reporting
    uint32_t execution_price;        // Execution price (x10000)
};

// Type 'X': Order Cancel (23 bytes)
// Partial reduction of shares for an existing order
struct ItchOrderCancel {
    char     msg_type;               // 'X'
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint8_t  timestamp[6];
    uint64_t order_ref;              // ID of the order being partially cancelled
    uint32_t cancelled_shares;       // Quantity to subtract from resting order
};

// Type 'D': Order Delete (19 bytes)
// Complete removal of an existing order from the book
struct ItchOrderDelete {
    char     msg_type;               // 'D'
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint8_t  timestamp[6];
    uint64_t order_ref;              // ID of the order to remove completely
};

// Type 'U': Order Replace (35 bytes)
// Deletes orig_order_ref and inserts a new order with new_order_ref
struct ItchOrderReplace {
    char     msg_type;               // 'U'
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint8_t  timestamp[6];
    uint64_t orig_order_ref;         // Old order ID to remove
    uint64_t new_order_ref;          // New order ID to insert
    uint32_t shares;                 // New total displayed quantity
    uint32_t price;                  // New price (x10000)
};

#pragma pack(pop)

// Compile-time verification of exact wire sizes according to ITCH 5.0 spec
static_assert(sizeof(ItchStockDirectory)     == 39, "Invalid R message size");
static_assert(sizeof(ItchAddOrder)           == 36, "Invalid A message size");
static_assert(sizeof(ItchAddOrderMpid)       == 40, "Invalid F message size");
static_assert(sizeof(ItchOrderExecuted)      == 31, "Invalid E message size");
static_assert(sizeof(ItchOrderExecutedPrice) == 36, "Invalid C message size");
static_assert(sizeof(ItchOrderCancel)        == 23, "Invalid X message size");
static_assert(sizeof(ItchOrderDelete)        == 19, "Invalid D message size");
static_assert(sizeof(ItchOrderReplace)       == 35, "Invalid U message size");

} // namespace itch