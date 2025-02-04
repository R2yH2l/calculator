#pragma once

#include <cmath>
#include <atomic>
#include <iostream>
#include <mutex>
#include <string>
#include <iomanip>

namespace calc {

    constexpr std::size_t BITS_PER_BYTE = 8;
    constexpr std::size_t DIGITS_PER_BLOCK = 8;



    /*
    * symbol_vec: a string representing the unique numerals
    * 
    *   The string "0123456789" decribes numerals with a base of ten. The string's order describes each numerals significance. The string
    * "9876543210" would produce the same awnsers to the string "0123456789", only differentiated in their symbol order.
    */
    struct alignas(std::uint64_t) number_base {
        const char* symbol_vec{};

        number_base(char const* symbol_vec)
            : symbol_vec(symbol_vec) {}
    };



    /*
    *   ┌────────┬──────────┐ 
    *   │ Digit  │ Bits     │
    *   ╞════════╪══════════╡
    *   │ 0      │ 00110010 │
    *   │ 1      │ 00010111 │
    *   │ 2      │ 00011100 │
    *   │ 3      │ 10010100 │
    *   │ 4      │ 00000000 │
    *   │ 5      │ 00000000 │
    *   │ 6      │ 00000000 │
    *   │ 7      │ 00000000 │
    *   │ 8      │ 00000000 │
    *   │ 9      │ 00000000 │
    *   └────────┴──────────┘
    */
    struct alignas(std::uint64_t) digit_block_data {
        std::uint64_t field_00_07 : 8 {};
        std::uint64_t field_08_15 : 8 {};
        std::uint64_t field_16_23 : 8 {};
        std::uint64_t field_24_31 : 8 {};
        std::uint64_t field_32_39 : 8 {};
        std::uint64_t field_40_47 : 8 {};
        std::uint64_t field_48_55 : 8 {};
        std::uint64_t field_56_63 : 8 {};
    };



    struct alignas(std::uint64_t) digit_block {

        using size_type = std::size_t;

        digit_block_data* data{};       /* pointer to a data chunk           */
        digit_block* next{};
        digit_block* prev{};

        std::atomic<bool> attention{};  /* for multi-threading               */
        digit_block_data* carry_data{}; /* carry storage for multi-threading */

        void set_fields(std::uint64_t& new_fields) const {
            *reinterpret_cast<std::uint64_t*>(data) = new_fields;
        }

        constexpr std::uint8_t operator[](size_type ind) const {
            switch (ind) {
            case 0: return data->field_00_07;
            case 1: return data->field_08_15;
            case 2: return data->field_16_23;
            case 3: return data->field_24_31;
            case 4: return data->field_32_39;
            case 5: return data->field_40_47;
            case 6: return data->field_48_55;
            case 7: return data->field_56_63;
            default: throw std::out_of_range{ "Index out of bounds" };
            }
        }
        void operator=(const std::uint64_t& new_fields) {
            *reinterpret_cast<std::uint64_t*>(data) = new_fields;
        }
        void operator|=(const std::uint64_t& new_fields) {
            *reinterpret_cast<std::uint64_t*>(data) |= new_fields;
        }

    };



    struct alignas(std::uint64_t) problem{};



    struct alignas(std::uint64_t) number {

        using size_type = std::uint64_t;

        number_base* base{};
        std::atomic_bool negative{};

        /* block is the array of blocks in the chain */
        digit_block* block{};
        size_type size{};

        /* if false: prevents indexing of blocks */
        std::atomic<bool> threads_may_index{ false };
        /* claims the privilege to write to threads_may_index */
        std::mutex index_control_lock{};
        /* theads halted priour to indexing get notified via this variable */
        std::condition_variable index_control_release{};
        std::atomic<std::uint64_t> indexing_thread_count{};


        number(number_base* base)
            : base(base) {}
        ~number() {
            this->free();
        }


        void free_iterative(digit_block* current_block) {
            while (current_block != nullptr && current_block != &block[size]) {

                /* clean up the current block */
                if (current_block->data) {
                    delete current_block->data;
                    current_block->data = nullptr;
                }
                if (current_block->carry_data) {
                    delete current_block->carry_data;
                    current_block->carry_data = nullptr;
                }

                /*
                *   +----------+           +----------+           +----------+
                *   | Other    |--- ... -->| Previous |- - -X- - >| Current  |
                *   | Blocks   |<-- ... ---| Block    |<----------| Block    |
                *   +----------+           +----------+           +----------+
                * 
                *       Unlink the current block from the previous block
                */
                if (current_block->prev) {
                    current_block->prev->next = nullptr;
                }

                /*
                *   +----------+           +----------+           +----------+
                *   | Other    |--- ... -->| Previous |           | Current  |
                *   | Blocks   |<-- ... ---| Block    |< - -X- - -| Block    |
                *   +----------+           +----------+           +----------+
                *
                *       Due to the allocation scheme current_block is not a resource of itself. The chain of blocks was the dynamic allocation
                *   of an pointer array. So, current_block
                */
                current_block->prev = nullptr;

                /* iteratively free blocks
                * 
                *   +----------+           +----------+           +----------+           +----------+
                *   | Other    |--- ... -->| Last     |           | First    |--- ... -->| Other    |
                *   | Blocks   |<-- ... ---| Block    |           | Block    |<-- ... ---| Blocks   |
                *   +----------+           +----------+           +----------+           +----------+
                *
                *   Explanation:
                *
                *       Iteration ends when control flows from the intial block to the final block. By severing the first and last block of the
                *   chain the result is an eventual nullptr held in current_block. The cain is traversed in order of first to last by following
                *   the next pointer of every block. If a block becomes corrupt or lost there is no ledger which tracks the allocation blocks,
                *   aside from the chain itself.
                */
                current_block = current_block->next;
            }
        }
        void free() {
            std::lock_guard<std::mutex> lock{ index_control_lock };
            threads_may_index.store(false);

            if (indexing_thread_count.load() > 0)
                indexing_thread_count.wait(0);

            if (block != nullptr) {
                free_iterative(block);
                size = 0;
                delete[] block;
                block = nullptr;
            }
        }

        void print() {
            if (block == nullptr) return;

            std::wcout
                << L"┌number" << "\n"
                << L"└───┬location         : 0x" << std::hex << this << "\n"
                << L"    ├base             : " << std::dec << std::strlen(base->symbol_vec) << "\n"
                << L"    ├symbol vector    : " << std::dec << base->symbol_vec << "\n"
                << L"    ├value            : ";

            digit_block* current_block{ block[0].prev };
            std::uint64_t symbol_len{ std::strlen(base->symbol_vec) };
            std::wstring wstr{};

            do {
                /* print in reverse order */
                wstr += std::to_wstring(current_block->data->field_56_63);
                wstr += std::to_wstring(current_block->data->field_48_55);
                wstr += std::to_wstring(current_block->data->field_40_47);
                wstr += std::to_wstring(current_block->data->field_32_39);
                wstr += std::to_wstring(current_block->data->field_24_31);
                wstr += std::to_wstring(current_block->data->field_16_23);
                wstr += std::to_wstring(current_block->data->field_08_15);
                wstr += std::to_wstring(current_block->data->field_00_07);

                current_block = current_block->prev;
            } while (current_block != block[0].prev);

             std::wcout
                << wstr << "\n"
                << L"    │\n"
                << L"    ├first block      : 0x" << std::hex << &block[0] << "\n"
                << L"    ├last block       : 0x" << std::hex << &block[size - 1] << "\n"
                << "\n";

            //print_iterative(block);
        }

        digit_block* get_block(size_type ind) {
            if (indexing_thread_count.load(std::memory_order::acquire) > 0)
                indexing_thread_count++;

            if (ind > size) {
                indexing_thread_count--;
                return nullptr;
            }

            digit_block* result{ &block[ind] };

            indexing_thread_count--;

            return result;
        }

        void assign(const char* str) {
            this->free();

            /* calculate the number of digit blocks required to fit all the digits of the string */
            std::uint64_t str_len{ std::strlen(str) };
            std::uint64_t digit_blocks_req{ (str_len + DIGITS_PER_BLOCK - 1) / DIGITS_PER_BLOCK };

            /* allocate at least one block */
            if (digit_blocks_req == 0) digit_blocks_req = 1;

            /* store the size of the list */
            size = digit_blocks_req;

            /* allocate an array of digit_block */
            block = new digit_block[digit_blocks_req]{};

            /* set up the first block's data */
            block->data = new digit_block_data{};
            block->carry_data = new digit_block_data{};

            /* create additional blocks and link them */
            for (std::uint64_t ind{ 1 }; ind < digit_blocks_req; ind++) {
                /*  create a new block
                * 
                *   +----------+           +----------+           +----------+
                *   | Other    |--- ... -->| Previous |           | New      |
                *   | Blocks   |<-- ... ---| Block    |           | Block    |
                *   +----------+           +----------+           +----------+
                */
                block[ind].data = new digit_block_data{};
                block[ind].carry_data = new digit_block_data{};

                /*  link the new and previous blocks, the previous block's next ptr adresses the new block
                * 
                *   +----------+           +----------+           +----------+
                *   | Other    |--- ... -->| Previous |--- new -->| New      |
                *   | Blocks   |<-- ... ---| Block    |           | Block    |
                *   +----------+           +----------+           +----------+
                */
                block[ind - 1].next = &block[ind];

                /*  the new block's prev ptr adresses the previous block
                * 
                *   +----------+           +----------+           +----------+
                *   | Other    |--- ... -->| Previous |---------->| New      |
                *   | Blocks   |<-- ... ---| Block    |<-- new ---| Block    |
                *   +----------+           +----------+           +----------+
                */
                block[ind].prev = &block[ind - 1];
            }

            /*  final links make the block chain circular, the last block points to the first block
            * 
            *       +----------+           +----------+           +----------+           +----------+
            *       | Other    |--- ... -->| Last     |--- new -->| First    |--- ... -->| Other    |
            *       | Blocks   |<-- ... ---| Block    |           | Block    |<-- ... ---| Blocks   |
            *       +----------+           +----------+           +----------+           +----------+
            */
            block[digit_blocks_req - 1].next = &block[0];

            /*  the first block points to the last block
            * 
            *       +----------+           +----------+           +----------+           +----------+
            *       | Other    |--- ... -->| Last     |---------->| First    |--- ... -->| Other    |
            *       | Blocks   |<-- ... ---| Block    |<-- new ---| Block    |<-- ... ---| Blocks   |
            *       +----------+           +----------+           +----------+           +----------+
            */
            block[0].prev = &block[digit_blocks_req - 1];

            std::uint64_t digit_ind{ str_len - 1 };
            std::uint64_t symbol_len{ std::strlen(base->symbol_vec) };

            /*
            *       Loop over the string and assign the digits to blocks. The digits are read in reverse order as the least significant digit is
            *   assumed to be on the right. It is persumed intuitive that a number would be writen in order of most to least significant as with
            *   english you would be reading left to right.
            */
            for (std::uint64_t str_ind{}; str_ind < str_len; str_ind++) {
                /* Loop over the symbols in the base's symbol_vec */
                for (std::uint64_t symbol_ind{}; symbol_ind < symbol_len; symbol_ind++) {
                    if (str[str_ind] != base->symbol_vec[symbol_ind]) continue;

                    block[digit_ind / DIGITS_PER_BLOCK] |= symbol_ind << (digit_ind * BITS_PER_BYTE);

                    break;
                }

                /* increment index */
                digit_ind--;
            }

            std::lock_guard<std::mutex> lock{ index_control_lock };
            threads_may_index.store(true);
        }

        void resize(const std::size_t& new_size) {
            
        }

        friend problem* operator+(const number& lhs, const number& rhs) {

        }
        digit_block* operator[](size_type ind) {
            return get_block(ind);
        }
    };

}