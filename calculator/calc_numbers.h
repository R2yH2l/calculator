#pragma once



#include <cmath>
#include <atomic>
#include <iostream>
#include <mutex>
#include <string>
#include <iomanip>
#include <bitset>
#include <locale>
#include <codecvt>
#include <vector>



namespace calc {



    constexpr std::uint64_t BITS_PER_DIGIT = 8;
    constexpr std::uint64_t DIGITS_PER_BLOCK = 8;
    constexpr std::uint64_t DIGITS_PER_COMMA = 3;



    template<typename T>
    std::wstring get_bits(T& bin) {
        std::bitset<sizeof(T)* CHAR_BIT> bits{ bin };
        std::string str{ bits.to_string() };
        std::size_t len{ str.size() + 1 }; /* including null terminator */
        std::wstring wstr(len, L'\0');

        std::size_t converted{};

        /* convert multi-byte string to wide string */
        mbstowcs_s(&converted, &wstr[0], wstr.size(), str.c_str(), str.size());

        return wstr;
    }


    /*  number_base: The object.
    /*  symbol_vec:  A string representing the unique numerals.
    /*  
    /*    The string "0123456789" decribes numerals with a base of ten. The string's order describes each numerals significance. The string
    /*  "9876543210" would produce the same awnsers to the string "0123456789", only differentiated in their symbol order.
    */
    struct alignas(std::uint64_t) number_base {
        const char* symbol_vec{};

        number_base(char const* symbol_vec)
            : symbol_vec(symbol_vec)
        {
        }
    };



    /*  ┌────────┬──────────┐ 
    /*  │ Digit  │ Bits     │
    /*  ╞════════╪══════════╡
    /*  │ 0      │ 00110010 │
    /*  │ 1      │ 00010111 │
    /*  │ 2      │ 00011100 │
    /*  │ 3      │ 10010100 │
    /*  │ 4      │ 00000000 │
    /*  │ 5      │ 00000000 │
    /*  │ 6      │ 00000000 │
    /*  │ 7      │ 00000000 │
    /*  │ 8      │ 00000000 │
    /*  │ 9      │ 00000000 │
    /*  └────────┴──────────┘
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



    enum struct operand_type {
        unknown = 0,
        oparen,
        cparen,
        exp,
        mul,
        div,
        add,
        sub
    };
    constexpr inline std::wostream& operator<<(std::wostream& lhs, const operand_type& rhs) {
        switch (rhs) {
        case operand_type::oparen:
            return lhs << L"oparen";
        case operand_type::cparen:
            return lhs << L"cparen";
        case operand_type::exp:
            return lhs << L"exp";
        case operand_type::mul:
            return lhs << L"mul";
        case operand_type::div:
            return lhs << L"div";
        case operand_type::add:
            return lhs << L"add";
        case operand_type::sub:
            return lhs << L"sub";
        default:
            return lhs << L"unknown";
        }
    }



    struct alignas(std::uint64_t) number;



    struct alignas(std::uint64_t) operation {
        number* term{};
        operand_type operand{};

        operation(number* term, const operand_type& operand)
            : term(term), operand(operand)
        {
        }
    };



    struct alignas(std::uint64_t) problem {
        std::vector<operation*> expression{};
    };



    struct alignas(std::uint64_t) number {

        using size_type = std::size_t;

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
        std::condition_variable index_halt_condition{};
        std::atomic<std::uint64_t> indexing_thread_count{};


        number(number_base* base)
            : base(base)
        {
        }
        ~number() {
            this->free();
        }



        void free_iterative(digit_block* current_block) {
            /* loop over blocks */
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
                /*  +----------+           +----------+           +----------+           +----------+
                /*  | Other    |--- ... -->| Previous |- - -X- - >| Current  |--- ... -->| Other    |
                /*  | Blocks   |<-- ... ---| Block    |<----------| Block    |<-- ... ---| Blocks   |
                /*  +----------+           +----------+           +----------+           +----------+
                /*
                /*  For each block sever the link connecting the previous block to the current block.
                */
                if (current_block->prev) {
                    current_block->prev->next = nullptr;
                }

                /*
                /*  +----------+           +----------+           +----------+           +----------+
                /*  | Other    |--- ... -->| Previous |           | Current  |--- ... -->| Other    |
                /*  | Blocks   |<-- ... ---| Block    |< - -X- - -| Block    |<-- ... ---| Blocks   |
                /*  +----------+           +----------+           +----------+           +----------+
                /*
                /*  Sever the link which connected the current block with the previous block.
                */
                current_block->prev = nullptr;

                /*
                /*  +----------+           +----------+           +----------+           +----------+
                /*  | Other    |--- ... -->| Last     |           | First    |--- ... -->| Other    |
                /*  | Blocks   |<-- ... ---| Block    |           | Block    |<-- ... ---| Blocks   |
                /*  +----------+           +----------+           +----------+           +----------+
                /*
                /*  Explanation:
                /*
                /*      Iteration ends when control flows from the intial block to the final block. By severing the first and last block of the
                /*  chain the result is an eventual nullptr held in current_block. The cain is traversed in order of first to last by following
                /*  the next pointer of every block. If a block becomes corrupt or lost there is no ledger which tracks the allocation blocks,
                /*  aside from the chain itself.
                */
                current_block = current_block->next;
            }
        }
        void free() {
            std::lock_guard<std::mutex> lock{ index_control_lock };
            threads_may_index.store(false);

            /* normal thread order is sufficent */
            if (indexing_thread_count.load() > 0)
                indexing_thread_count.wait(0);

            if (block != nullptr) {
                free_iterative(block);
                size = 0;
                delete[] block;
                block = nullptr;
            }
        }



        void print_iterative(digit_block* block) {
            digit_block* current_block{ new digit_block };
            current_block = block;

            do {
                std::bitset<DIGITS_PER_BLOCK* BITS_PER_DIGIT> bits{ reinterpret_cast<std::uint64_t>(current_block->data) };
                std::wcout
                    << L"    location         : 0x" << std::hex << current_block << "\n";
                std::wstring bit_str{ get_bits(*reinterpret_cast<std::uint64_t*>(current_block->data)) };

                for (std::uint64_t ind{}; ind <= BITS_PER_DIGIT * DIGITS_PER_BLOCK; ind += 9) {
                    bit_str.insert(ind, 1, L'-');
                }

                std::wcout
                    << L"    fields           : 0b" << bit_str << "\n";

                std::wstring wstr{};

                /* print in reverse order */
                wstr += std::to_wstring(current_block->data->field_56_63);
                wstr += std::to_wstring(current_block->data->field_48_55);
                wstr += std::to_wstring(current_block->data->field_40_47);
                wstr += std::to_wstring(current_block->data->field_32_39);
                wstr += std::to_wstring(current_block->data->field_24_31);
                wstr += std::to_wstring(current_block->data->field_16_23);
                wstr += std::to_wstring(current_block->data->field_08_15);
                wstr += std::to_wstring(current_block->data->field_00_07);

                std::wcout
                    << L"    value            : " << wstr << "\n";

                if (current_block != block->prev) {
                    std::wcout
                        << L"    \n";
                }

                current_block = current_block->prev;
            } while (current_block != block);
        }
        void print() {
            if (block == nullptr) return;

            std::wstring num_str{*this};

            std::wcout
                << L"number" << "\n"
                << L"  location           : 0x" << std::hex << this << "\n"
                << L"  base               : " << std::dec << std::strlen(base->symbol_vec) << "\n"
                << L"  symbol vector      : " << std::dec << base->symbol_vec << "\n"
                << L"  value              : ";

            digit_block* current_block{ block[0].prev };
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
                
                << L"  \n"
                << L"  last block         : 0x" << std::hex << &block[size - 1] << "\n"
                << L"  first block        : 0x" << std::hex << &block[0] << "\n"
                << L"  \n"
                << L"  blocks\n";

            print_iterative(block[0].prev);

            std::wcout << std::endl;
        }



        digit_block* get_block(size_type ind) {
            if (ind > size)
                return nullptr;

            return &block[ind];
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
                /*
                /*  +----------+           +----------+           +----------+
                /*  | Other    |--- ... -->| Previous |           | New      |
                /*  | Blocks   |<-- ... ---| Block    |           | Block    |
                /*  +----------+           +----------+           +----------+
                */
                block[ind].data = new digit_block_data{};
                block[ind].carry_data = new digit_block_data{};

                /*  link the new and previous blocks, the previous block's next ptr addresses the new block
                /*
                /*  +----------+           +----------+           +----------+
                /*  | Other    |--- ... -->| Previous |--- new -->| New      |
                /*  | Blocks   |<-- ... ---| Block    |           | Block    |
                /*  +----------+           +----------+           +----------+
                */
                block[ind - 1].next = &block[ind];

                /*  the new block's prev ptr adresses the previous block
                /*
                /*  +----------+           +----------+           +----------+
                /*  | Other    |--- ... -->| Previous |---------->| New      |
                /*  | Blocks   |<-- ... ---| Block    |<-- new ---| Block    |
                /*  +----------+           +----------+           +----------+
                */
                block[ind].prev = &block[ind - 1];
            }

            /*  final links make the block chain circular; the last block points to the first block
            /*
            /*  +----------+           +----------+           +----------+           +----------+
            /*  | Other    |--- ... -->| Last     |--- new -->| First    |--- ... -->| Other    |
            /*  | Blocks   |<-- ... ---| Block    |           | Block    |<-- ... ---| Blocks   |
            /*  +----------+           +----------+           +----------+           +----------+
            */
            block[digit_blocks_req - 1].next = &block[0];

            /*  the first block points to the last block
            /*
            /*  +----------+           +----------+           +----------+           +----------+
            /*  | Other    |--- ... -->| Last     |---------->| First    |--- ... -->| Other    |
            /*  | Blocks   |<-- ... ---| Block    |<-- new ---| Block    |<-- ... ---| Blocks   |
            /*  +----------+           +----------+           +----------+           +----------+
            */
            block[0].prev = &block[digit_blocks_req - 1];

            std::uint64_t digit_ind{ str_len - 1 };
            std::uint64_t symbol_len{ std::strlen(base->symbol_vec) };

            /*      Loop over the string and assign the digits to blocks. The digits are read in reverse order as the least significant digit is
            /*  assumed to be on the right. It is persumed intuitive that the number would be given in order of most to least significant as with
            /*  english you would read the number from left to right.
            */
            for (std::uint64_t str_ind{}; str_ind < str_len; str_ind++) {
                /* loop over the symbols in the base's symbol_vec */
                for (std::uint64_t symbol_ind{}; symbol_ind < symbol_len; symbol_ind++) {
                    if (str[str_ind] != base->symbol_vec[symbol_ind]) continue;

                    block[digit_ind / DIGITS_PER_BLOCK] |= symbol_ind << (digit_ind * BITS_PER_DIGIT);

                    break;
                }

                /* increment index */
                digit_ind--;
            }

            std::lock_guard<std::mutex> lock{ index_control_lock };
            threads_may_index.store(true);
        }
        void resize(const std::size_t& new_size) {
            if (new_size == 0) {
                /* passing nullptr to assign allocates a single block */
                this->assign(nullptr);
                return;
            }

            /* claim control and camly wait */
            std::lock_guard<std::mutex> lock{ index_control_lock };
            threads_may_index.store(false, std::memory_order_acq_rel);

            /* normal thread order is sufficent */
            if (indexing_thread_count.load() > 0)
                indexing_thread_count.wait(0);


        }



        /*      Below this comment are four groups each containing three overloaded operators. These overloads serve as the mechanism to serialize
        /*  the operations of some given expression. This is achived by using a third party class which in the end holds the order of operations.
        /*  Take for example the expression 'a + (b - c) * d' this would become 'b - c * d + a' also written '{ {?, b}, {-, c}, {*, d}, {+, a} }'.
        /*  This is an imutable sequence of explicit order expressing each step to achive the correct awnser.
        */

        /* multiplication overload */
        friend problem* operator*(number& lhs, number& rhs) {
            problem* p{ new problem{} };

            p->expression.emplace_back(new operation{ &rhs, operand_type::unknown });
            p->expression.emplace_back(new operation{ &lhs, operand_type::mul });

            return p;
        }
        friend problem* operator*(number& lhs, problem* rhs) {
            rhs->expression.emplace_back(new operation{ &lhs, operand_type::mul });
            return rhs;
        }
        friend problem* operator*(problem* lhs, number& rhs) {
            lhs->expression.emplace_back(new operation{ &rhs, operand_type::mul });
            return lhs;
        }

        /* division overload */
        friend problem* operator/(number& lhs, number& rhs) {
            problem* p{ new problem{} };

            p->expression.emplace_back(new operation{ &rhs, operand_type::unknown });
            p->expression.emplace_back(new operation{ &lhs, operand_type::div });

            return p;
        }
        friend problem* operator/(number& lhs, problem* rhs) {
            rhs->expression.emplace_back(new operation{ &lhs, operand_type::div });
            return rhs;
        }
        friend problem* operator/(problem* lhs, number& rhs) {
            lhs->expression.emplace_back(new operation{ &rhs, operand_type::div });
            return lhs;
        }

        /* addition overload */
        friend problem* operator+(number& lhs, number& rhs) {
            problem* p{ new problem{} };

            p->expression.emplace_back(new operation{ &rhs, operand_type::unknown });
            p->expression.emplace_back(new operation{ &lhs, operand_type::add });

            return p;
        }
        friend problem* operator+(number& lhs, problem* rhs) {
            rhs->expression.emplace_back(new operation{ &lhs, operand_type::add });
            return rhs;
        }
        friend problem* operator+(problem* lhs, number& rhs) {
            lhs->expression.emplace_back(new operation{ &rhs, operand_type::add });
            return lhs;
        }

        /* subtraction overload */
        friend problem* operator-(number& lhs, number& rhs) {
            problem* p{ new problem{} };

            p->expression.emplace_back(new operation{ &rhs, operand_type::unknown });
            p->expression.emplace_back(new operation{ &lhs, operand_type::sub });

            return p;
        }
        friend problem* operator-(number& lhs, problem* rhs) {
            rhs->expression.emplace_back(new operation{ &lhs, operand_type::sub });
            return rhs;
        }
        friend problem* operator-(problem* lhs, number& rhs) {
            lhs->expression.emplace_back(new operation{ &rhs, operand_type::sub });
            return lhs;
        }



        friend std::wostream& operator<<(std::wostream& lhs, const number& rhs) {
            digit_block* current_block{ rhs.block[0].prev };

            /* nothing to print */
            if (current_block == nullptr)
                return lhs;

            bool found_first_non_zero{};
            std::size_t comma_counter{};
            std::size_t total_digits{};

            /* the first block is special since as any leading zeros are omitted for brevity */
            for (std::size_t ind{ DIGITS_PER_BLOCK - 1 }; ind < DIGITS_PER_BLOCK; ind--) {
                if (!(*current_block)[ind] && !found_first_non_zero) continue;
                else if (!found_first_non_zero) {
                    found_first_non_zero = true;

                    total_digits = DIGITS_PER_BLOCK * rhs.size - (DIGITS_PER_BLOCK - (ind + 1));
                    comma_counter = DIGITS_PER_BLOCK - total_digits % DIGITS_PER_COMMA;
                }

                lhs << rhs.base->symbol_vec[(*current_block)[ind]];

                if (comma_counter == ind && !(current_block == &rhs.block[0] && ind == 0)) {
                    comma_counter -= 3;
                    lhs << L',';
                }
            }

            current_block = current_block->prev;

            while (current_block != rhs.block[0].prev) {
                if (comma_counter >= DIGITS_PER_BLOCK)
                    comma_counter = DIGITS_PER_BLOCK - (0xffffffffffffffff - comma_counter + 1);

                /* print in reverse order */
                for (std::size_t ind{ DIGITS_PER_BLOCK - 1 }; ind < DIGITS_PER_BLOCK; ind--) {
                    lhs << rhs.base->symbol_vec[(*current_block)[ind]];

                    if (comma_counter == ind && !(current_block == &rhs.block[0] && ind == 0)) {
                        comma_counter -= 3;
                        lhs << L',';
                    }
                }

                current_block = current_block->prev;
            }

            return lhs;
        }



        digit_block* operator[](size_type ind) {
            return get_block(ind);
        }
    };



} /* end calc */