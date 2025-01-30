#pragma once

#include <cmath>
#include <atomic>
#include <iostream>
#include <mutex>
#include <string>
#include <iomanip>

namespace calc {

	struct number_base {
		const char* symbol_vec{};

		number_base(char const* symbol_vec)
			: symbol_vec(symbol_vec) {}
	};



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



	struct digit_block {

		using size_type = std::size_t;

		digit_block_data* data{}; /* pointer to a data chunk */
		digit_block* next{};      /* for carry opperations */
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



	struct number {

		using size_type = std::size_t;

		number_base* base{};
		std::atomic_bool negative{};

		/* block is both the first and last block in the chain */
		digit_block* block{};
		size_type size{};

		/* locks */
		/* if false prevents indexing of blocks */
		std::atomic<bool> threads_may_index{ true };
		/* if true prevents locking of threads_may_index */
		std::atomic<bool> threads_are_indexing{};
		std::atomic<std::uint64_t> indexing_thread_count{};


		number(number_base* base)
			: base(base) {}
		~number() {
			this->free();
		}


		void free_recursive(digit_block* current_block) {
			if (current_block == nullptr || current_block == &block[size]) return;

			std::cout
				<< "***************** free recursion  *****************\n\n"
				<< "current_block    " << ": is located at 0x" << std::hex << current_block << "\n\n";

			std::cout
				<< "block 0x" << std::hex << current_block << " before being deleted" << "\n"
				<< "    data         " << ": is located at 0x" << std::hex << current_block->data << "\n"
				<< "    &data        " << ": is offset by  0x"
				<< std::hex << reinterpret_cast<std::uint64_t>(&(current_block->data)) - reinterpret_cast<std::uint64_t>(current_block) << "\n"
				<< "    carry_data   " << ": is located at 0x" << std::hex << current_block->carry_data << "\n"
				<< "    &carry_data  " << ": is offset by  0x"
				<< std::hex << reinterpret_cast<std::uint64_t>(&(current_block->carry_data)) - reinterpret_cast<std::uint64_t>(current_block) << "\n"
				<< "    next*        " << ": 0x" << std::hex << current_block->next << "\n"
				<< "    prev*        " << ": 0x" << std::hex << current_block->next << "\n\n";

			std::uint64_t padwidth{ std::to_string(size).size() };
			for (size_type ind{}; ind < size; ind++) {
				std::cout
					<< "block " << std::setw(padwidth) << ind << " : is located at " << std::hex << &block[ind] << "\n"
					<< "    data         " << ": is located at 0x" << std::hex << block[ind].data << "\n"
					<< "    carry_data   " << ": is located at 0x" << std::hex << block[ind].carry_data << "\n\n";
			}

			/* clean up the current block */
			if (current_block->data) {
				delete current_block->data;
				current_block->data = nullptr;
			}
			if (current_block->carry_data) {
				delete current_block->carry_data;
				current_block->carry_data = nullptr;
			}

			current_block->prev->next = nullptr;

			std::cout
				<< "block 0x" << std::hex << current_block << " after being deleted" << "\n"
				<< "    data         " << ": is located at 0x" << std::hex << current_block->data << "\n"
				<< "    &data        " << ": is offset by  0x"
				<< std::hex << reinterpret_cast<std::uint64_t>(&(current_block->data)) - reinterpret_cast<std::uint64_t>(current_block) << "\n"
				<< "    carry_data   " << ": is located at 0x" << std::hex << current_block->carry_data << "\n"
				<< "    &carry_data  " << ": is offset by  0x"
				<< std::hex << reinterpret_cast<std::uint64_t>(&(current_block->carry_data)) - reinterpret_cast<std::uint64_t>(current_block) << "\n"
				<< "    next*        " << ": 0x" << std::hex << current_block->next << "\n"
				<< "    prev*        " << ": 0x" << std::hex << current_block->next << "\n\n";
			
			for (size_type ind{}; ind < size; ind++) {
				std::cout
					<< "block " << std::setw(padwidth) << ind << " : is located at " << std::hex << &block[ind] << "\n"
					<< "    data         " << ": is located at 0x" << std::hex << block[ind].data << "\n"
					<< "    carry_data   " << ": is located at 0x" << std::hex << block[ind].carry_data << "\n\n";
			}

			/* recursively free the next block */
			free_recursive(current_block->next);

			current_block->prev = nullptr;
			current_block = nullptr;
		}
		void free() {
			if (block != nullptr) {
				std::cout
					<< "***************** freeing blocks  *****************\n\n"
					<< "the first block  : is located at 0x" << std::hex << &block[0] << "\n"
					<< "the last block   : is located at 0x" << std::hex << &block[size - 1] << "\n\n";

				free_recursive(block);
				size = 0;
				delete[] block;
				block = nullptr;

				std::cout
					<< "*************** end free recursion  ***************\n\n";
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

			std::uint64_t padwidth{ std::to_string(size).size() };
			for (size_type ind{}; ind < size; ind++) {
				std::cout
					<< "block " << std::setw(padwidth) << ind << " : is located at " << std::hex << &block[ind] << "\n"
					<< "    data         " << ": is located at 0x" << std::hex << block[ind].data << "\n"
					<< "    carry_data   " << ": is located at 0x" << std::hex << block[ind].carry_data << "\n\n";
			}

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


		digit_block* get_block_recursive(size_type ind, digit_block* block) {
			return (ind > 0) ? get_block_recursive(--ind, block->next) : block;
		}
		digit_block* get_block(size_type ind) {
			/* check if threads are allowed to index */
			if (threads_may_index.load()) {
				threads_are_indexing.store(true);
				indexing_thread_count++;
			}
			else {
				/* otherwise wait until the list can be indexed */
				while (!threads_may_index.load());
				threads_are_indexing.store(true);
				indexing_thread_count++;
			}

			/* get the block at the specified index */
			digit_block* result{ get_block_recursive(ind, block) };
			indexing_thread_count--;

			/* if this thread was the last one indexing, reset the indexing flag */
			if (indexing_thread_count.load() == 0) {
				threads_are_indexing.store(false);
			}

			return result;
		}


		void assign(const char* str) {
			/* calculate the number of digit blocks required to fit all the digits of the string */
			std::uint64_t digit_blocks_req{ (strlen(str) + 7) / 8 };

			this->free();

			/* one block is always created */
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
				block[ind].data = new digit_block_data{};
				block[ind].carry_data = new digit_block_data{};

				/* link the blocks */
				block[ind - 1].next = &block[ind]; /* set previous block's next ptr */
				block[ind].prev = &block[ind - 1]; /* set current block's prev ptr */
			}

			/* final link to make the blocks circular */
			block[digit_blocks_req - 1].next = &block[0]; /* last block points to the first */
			block[0].prev = &block[digit_blocks_req - 1]; /* first block points to the last */

			std::uint64_t digit_ind{};
			/* loop over the string and assign the digits to blocks */
			for (std::uint64_t str_ind{}; str_ind < strlen(str); str_ind++) {
				for (std::uint64_t symbol_ind{}; symbol_ind < strlen(base->symbol_vec); symbol_ind++) {
					if (str[str_ind] != base->symbol_vec[symbol_ind]) continue;

					block[digit_ind / 8] |= symbol_ind << (digit_ind * 8);

					break;
				}

				/* increment index */
				digit_ind++;
			}
		}

		void resize(const std::size_t& new_size) {
		}

		friend problem* operator+(const number& lhs, number rhs) {

		}
		digit_block* operator[](size_type ind) {
			return get_block(ind);
		}
	};

}