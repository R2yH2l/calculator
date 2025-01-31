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

    struct alignas(std::uint64_t) number_base {
        const char* symbol_vec{};

        number_base(char const* symbol_vec)
            : symbol_vec(symbol_vec) {}
    };

    /*
         | Digit | Bits
         +_______+____________
         |  0    | 00110010
         |  1    | 00010111
         |  2    | 00011100
         |  3    | 10010100
         |  4    | 00000000
         |  5    | 00000000
         |  6    | 00000000
         |  7    | 00000000
         |  8    | 00000000
         |  9    | 00000000
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

        digit_block_data* data{};       /* pointer to a data chunk */
        digit_block* next{};
        digit_block* prev{};

        std::atomic<bool> attention{};  /* for multi-threading */
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



    struct problem{};



    struct alignas(std::uint64_t) number {

        using size_type = std::uint64_t;

        number_base* base{};
        std::atomic_bool negative{};

        /* block is both the first and last block in the chain */
        digit_block* block{};
        size_type size{};

        /* locks */
        /* if false: prevents indexing of blocks */
        std::atomic<bool> threads_may_index{ false };
        /* if true: prevents locking of threads_may_index */
        std::atomic<bool> threads_are_indexing{ false };
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
                /* Unlink the current block from the previous block */
                /*
                    +----------+           +---------+
                    | Previous |- - -X- - >| Current |
                    | Block    |<----------| Block   |
                    +----------+   		   +---------+
                */
                if (current_block->prev) {
                    current_block->prev->next = nullptr;
                }
                /*
                    +----------+           +---------+
                    | Previous |           | Current |
                    | Block    |< - -X- - -| Block   |
                    +----------+   		   +---------+

                        Due to the allocation scheme current_block is not a resource of itself. The chain of blocks was the dynamic allocation
                    of an pointer array. So, current_block
                */
                current_block->prev = nullptr;

                /* iteratively free blocks */
                /*
                    +----------+           +----------+           +----------+           +---------+
                    | Other    |--- ... -->| Last     |           | First    |--- ... -->| Other   |
                    | Blocks   |<-- ... ---| Block    |           | Block    |<-- ... ---| Blocks  |
                    +----------+   		   +----------+   		  +----------+   		 +---------+

                    Explanation

                        Iteration ends when control flows from the intial block to the final block. By severing the last and first block of the
                    chain the result is an eventual call to free_recursive with a nullptr. The cain is traversed in order of first to last by
                    following the next pointer of every block. If a block becomes corrupt or lost there is no ledger which tracks the allocation
                    blocks, aside from the chain itself.
                */
                current_block = current_block->next;
            }
        }
        void free() {
            threads_may_index.store(false);
            indexing_thread_count.wait(0);

            if (block != nullptr) {
                free_iterative(block);
                size = 0;
                delete[] block;
                block = nullptr;
            }
        }


        void print_recursive(digit_block* current_block) {
            if (current_block == nullptr) return;

            std::cout
                << "***************** print recursion *****************\n\n"
                << "current_block    " << ": is located at 0x" << std::hex << current_block << "\n\n";

            std::cout
                << "block 0x" << std::hex << current_block << " is being viewed" << "\n"
                << "    data         " << ": is located at 0x" << std::hex << current_block->data << "\n"
                << "    &data        " << ": is offset by  0x"
                << std::hex << reinterpret_cast<std::uint64_t>(&(current_block->data)) - reinterpret_cast<std::uint64_t>(current_block) << "\n"
                << "    carry_data   " << ": is located at 0x" << std::hex << current_block->carry_data << "\n"
                << "    &carry_data  " << ": is offset by  0x"
                << std::hex << reinterpret_cast<std::uint64_t>(&(current_block->carry_data)) - reinterpret_cast<std::uint64_t>(current_block) << "\n"
                << "    next*        " << ": 0x" << std::hex << current_block->next << "\n"
                << "    prev*        " << ": 0x" << std::hex << current_block->next << "\n\n";

            //std::uint64_t padwidth{ std::to_string(size).size() };
            //for (size_type ind{}; ind < size; ind++) {
            //	std::cout
            //		<< "block " << std::setw(padwidth) << ind << " : is located at " << std::hex << &block[ind] << "\n"
            //		<< "    data         " << ": is located at 0x" << std::hex << block[ind].data << "\n"
            //		<< "    carry_data   " << ": is located at 0x" << std::hex << block[ind].carry_data << "\n"
            //		<< "    next*        " << ": 0x" << std::hex << block[ind].next << "\n"
            //		<< "    prev*        " << ": 0x" << std::hex << block[ind].prev << "\n\n";
            //}

            if (current_block == &block[size]) return;

            print_recursive(current_block + 1);
        }
        void print() {
            if (block != nullptr) {
                std::cout
                    << "***************** printing blocks *****************\n\n"
                    << "the first block  : is located at 0x" << std::hex << &block[0] << "\n"
                    << "the last block   : is located at 0x" << std::hex << &block[size - 1] << "\n\n";

                print_recursive(block);

                std::cout
                    << "*************** end print recursion ***************\n\n";
            }
        }

        digit_block* get_block(size_type ind) {
            if (ind > size) return nullptr;

            threads_may_index.wait(true);
            indexing_thread_count++;

            digit_block* result{ &block[ind] };

            indexing_thread_count--;

            return result;
        }

        void assign(const char* str) {
            this->free();

            /* calculate the number of digit blocks required to fit all the digits of the string */
            std::uint64_t digit_blocks_req{ (strlen(str) + DIGITS_PER_BLOCK - 1) / DIGITS_PER_BLOCK };

            /* atleast one block is always created */
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
                /* create the new block */
                /*
                    +----------+           +----------+           +---------+
                    | Other    |--- ... -->| Previous |           | New     |
                    | Blocks   |<-- ... ---| Block    |           | Block   |
                    +----------+   		   +----------+   		  +---------+
                */
                block[ind].data = new digit_block_data{};
                block[ind].carry_data = new digit_block_data{};

                /* link the blocks */
                /*
                    +----------+           +----------+           +---------+
                    | Other    |--- ... -->| Previous |--- mew -->| New     |
                    | Blocks   |<-- ... ---| Block    |           | Block   |
                    +----------+   		   +----------+   		  +---------+
                */
                block[ind - 1].next = &block[ind]; /* set the previous block's next ptr */
                /*
                    +----------+           +----------+           +---------+
                    | Other    |--- ... -->| Previous |---------->| New     |
                    | Blocks   |<-- ... ---| Block    |<-- new ---| Block   |
                    +----------+   		   +----------+   		  +---------+
                */
                block[ind].prev = &block[ind - 1]; /* set the new block's prev ptr */
            }

            /* final link to make the block chain circular */
            /*
                    +----------+           +----------+           +----------+           +---------+
                    | Other    |--- ... -->| Last     |--- new -->| First    |--- ... -->| Other   |
                    | Blocks   |<-- ... ---| Block    |           | Block    |<-- ... ---| Blocks  |
                    +----------+   		   +----------+   		  +----------+   		 +---------+
            */
            block[digit_blocks_req - 1].next = &block[0]; /* the last block points to the first block */
            /*
                    +----------+           +----------+           +----------+           +---------+
                    | Other    |--- ... -->| Last     |---------->| First    |--- ... -->| Other   |
                    | Blocks   |<-- ... ---| Block    |<-- new ---| Block    |<-- ... ---| Blocks  |
                    +----------+   		   +----------+   		  +----------+   		 +---------+
            */
            block[0].prev = &block[digit_blocks_req - 1]; /* the first block points to the last block */

            std::uint64_t digit_ind{};
            std::uint64_t str_len{ strlen(str) };
            std::uint64_t symbol_len{ strlen(base->symbol_vec) };

            /* loop over the string and assign the digits to blocks */
            for (std::uint64_t str_ind{}; str_ind < str_len; str_ind++) {
                /* Loop over the symbols in the base's symbol_vec */
                for (std::uint64_t symbol_ind{}; symbol_ind < symbol_len; symbol_ind++) {
                    if (str[str_ind] != base->symbol_vec[symbol_ind]) continue;

                    block[digit_ind / DIGITS_PER_BLOCK] |= symbol_ind << (digit_ind * BITS_PER_BYTE);

                    break;
                }

                /* increment index */
                digit_ind++;
            }
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