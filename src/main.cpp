#include "endian_utils.hpp"
#include "itch_protocol.hpp"
#include "mmap_file.hpp"
#include "order_book.hpp"

#include <array>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>

namespace {

// Format a ticker string (e.g., "AAPL") into an 8-byte space-padded array ("AAPL    ")
void format_padded_ticker(const char* input, char output[8]) noexcept {
    std::memset(output, ' ', 8);
    const size_t len = std::strlen(input);
    const size_t copy_len = (len < 8) ? len : 8;
    std::memcpy(output, input, copy_len);
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <itch_binary_file> [TICKER]\n";
        return 1;
    }

    const char* filepath = argv[1];
    const bool filter_by_ticker = (argc >= 3);

    char target_ticker_padded[8]{};
    if (filter_by_ticker) {
        format_padded_ticker(argv[2], target_ticker_padded);
    }

    try {
        itch::MemoryMappedFile mmap_file(filepath);
        const uint8_t* ptr = mmap_file.data();
        const uint8_t* const end_ptr = ptr + mmap_file.size();

        // Target stock_locate kept in Big-Endian (network order) for zero-cost comparison
        uint16_t target_locate_be = 0;
        bool locate_resolved = false;

        uint64_t total_messages = 0;
        uint64_t target_book_events = 0;
        std::array<uint64_t, 256> msg_type_counts{};

        itch::LimitOrderBook lob;

        const auto start_time = std::chrono::steady_clock::now();

        // Main zero-allocation parsing loop
        while (ptr + 2 <= end_ptr) {
            // Read 2-byte Big-Endian message length prefix
            const uint16_t msg_len = itch::bswap16(*reinterpret_cast<const uint16_t*>(ptr));
            const uint8_t* const msg_ptr = ptr + 2;
            const uint8_t* const next_ptr = msg_ptr + msg_len;

            if (next_ptr > end_ptr || msg_len == 0) {
                break;
            }

            const uint8_t msg_type = *msg_ptr;
            msg_type_counts[msg_type]++;
            total_messages++;

            // Intercept Stock Directory ('R') to resolve ticker -> stock_locate
            if (msg_type == itch::MSG_STOCK_DIRECTORY && filter_by_ticker && !locate_resolved) {
                if (msg_len == sizeof(itch::ItchStockDirectory)) {
                    const auto* dir = reinterpret_cast<const itch::ItchStockDirectory*>(msg_ptr);
                    if (std::memcmp(dir->stock, target_ticker_padded, 8) == 0) {
                        target_locate_be = dir->stock_locate; // Keep raw Big-Endian bytes
                        locate_resolved = true;
                        std::cout << "[INFO] Resolved ticker '" << argv[2]
                                  << "' -> stock_locate = " << itch::bswap16(target_locate_be) << "\n";
                    }
                }
            }

            // filter: in every ITCH message, bytes [1..2] are always stock_locate
            if (!filter_by_ticker || (locate_resolved &&
                *reinterpret_cast<const uint16_t*>(msg_ptr + 1) == target_locate_be)) {

                // Target logic routing: Cast structs and byte-swap fields
                switch (msg_type) {
                    case itch::MSG_ADD_ORDER: {
                        const auto* raw = reinterpret_cast<const itch::ItchAddOrder*>(msg_ptr);
                        lob.on_add_order(itch::bswap64(raw->order_ref), raw->side,
                                         itch::bswap32(raw->shares), itch::bswap32(raw->price));
                        target_book_events++;
                        break;
                    }
                    case itch::MSG_ADD_ORDER_MPID: {
                        const auto* raw = reinterpret_cast<const itch::ItchAddOrderMpid*>(msg_ptr);
                        lob.on_add_order(itch::bswap64(raw->order_ref), raw->side,
                                         itch::bswap32(raw->shares), itch::bswap32(raw->price));
                        target_book_events++;
                        break;
                    }
                    case itch::MSG_ORDER_EXECUTED: {
                        const auto* raw = reinterpret_cast<const itch::ItchOrderExecuted*>(msg_ptr);
                        lob.on_order_executed(itch::bswap64(raw->order_ref), itch::bswap32(raw->executed_shares));
                        target_book_events++;
                        break;
                    }
                    case itch::MSG_ORDER_EXECUTED_PRICE: {
                        const auto* raw = reinterpret_cast<const itch::ItchOrderExecutedPrice*>(msg_ptr);
                        lob.on_order_executed(itch::bswap64(raw->order_ref), itch::bswap32(raw->executed_shares));
                        target_book_events++;
                        break;
                    }
                    case itch::MSG_ORDER_CANCEL: {
                        const auto* raw = reinterpret_cast<const itch::ItchOrderCancel*>(msg_ptr);
                        lob.on_order_cancel(itch::bswap64(raw->order_ref), itch::bswap32(raw->cancelled_shares));
                        target_book_events++;
                        break;
                    }
                    case itch::MSG_ORDER_DELETE: {
                        const auto* raw = reinterpret_cast<const itch::ItchOrderDelete*>(msg_ptr);
                        lob.on_order_delete(itch::bswap64(raw->order_ref));
                        target_book_events++;
                        break;
                    }
                    case itch::MSG_ORDER_REPLACE: {
                        const auto* raw = reinterpret_cast<const itch::ItchOrderReplace*>(msg_ptr);
                        lob.on_order_replace(itch::bswap64(raw->orig_order_ref), itch::bswap64(raw->new_order_ref),
                                             itch::bswap32(raw->shares), itch::bswap32(raw->price));
                        target_book_events++;
                        break;
                    }
                    default:
                        break;
                }
            }
            /*  ----------------------------------------------------------------------------------------
                NOTE: Due to the nature of the NASDAQ's marker mechanics (System Clear), each trading
                day ends up with a LOB summing up to 0 for every Ticker. Therefore, to gain an immediate
                snapshot of the system capability to correctly parse and reconstruct the LOB, it is
                necessary to perform a mid-day live snapshot here.
                ----------------------------------------------------------------------------------------*/
            if (target_book_events == 800000) {
                std::cout << "\n--- MID-DAY LIVE SNAPSHOT ---\n"
                          << "Active orders: " << lob.active_orders_count() << "\n";
                          
                if (!lob.get_asks().empty()) {
                    std::cout << "Best Ask: $" << (lob.get_asks().begin()->first / 10000.0) 
                              << " (Vol: " << lob.get_asks().begin()->second.total_volume << ")\n";
                }
                if (!lob.get_bids().empty()) {
                    std::cout << "Best Bid: $" << (lob.get_bids().begin()->first / 10000.0) 
                              << " (Vol: " << lob.get_bids().begin()->second.total_volume << ")\n";
                }
                std::cout << "-----------------------------\n";
            }
            ptr = next_ptr;
        }

        const auto end_time = std::chrono::steady_clock::now();
        const double elapsed_sec = std::chrono::duration<double>(end_time - start_time).count();

        // Print performance metrics
        std::cout << "\n--- Parsing Summary ---\n"
                  << "File size:          " << (mmap_file.size() / (1024 * 1024)) << " MB\n"
                  << "Total messages:     " << total_messages << "\n"
                  << "Target LOB events:  " << target_book_events << "\n"
                  << "Elapsed time:       " << std::fixed << std::setprecision(3) << elapsed_sec << " s\n"
                  << "Throughput:         " << (total_messages /elapsed_sec / 1e6) << " M msgs/sec\n\n";

        // Print final Limit Order Book state
        std::cout << "--- Limit Order Book Status ---\n"
                  << "Active orders in memory: " << lob.active_orders_count() << "\n";
                  
        if (!lob.get_bids().empty()) {
            std::cout << "Best Bid: $" << (lob.get_bids().begin()->first / 10000.0) 
                      << " (Vol: " << lob.get_bids().begin()->second.total_volume << ")\n";
        }
        if (!lob.get_asks().empty()) {
            std::cout << "Best Ask: $" << (lob.get_asks().begin()->first / 10000.0) 
                      << " (Vol: " << lob.get_asks().begin()->second.total_volume << ")\n";
        }

    } catch (const std::exception& ex) {
        std::cerr << "[ERROR] " << ex.what() << "\n";
        return 1;
    }

    return 0;
}