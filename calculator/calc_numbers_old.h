#pragma once

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace calc {

	struct number_base {
		std::vector<wchar_t> symbol_vec{};

		number_base(const std::vector<wchar_t>& symbol_vec)
			: symbol_vec(symbol_vec) {}
	};

	struct digit {
		std::uint64_t power{};
		wchar_t symbol{};
		std::uint64_t symbol_ind{};

		digit(const std::uint64_t& power, wchar_t symbol, const std::uint64_t& symbol_ind)
			: power(power), symbol(symbol), symbol_ind(symbol_ind) {}
	};

	class number {
		number_base* base{};
		std::vector<digit> digits{};

	public:

		number(number_base* base, const std::wstring& number_str ) {
			assign(number_str, base);
		}

		std::uint64_t base_size() {
			return base->symbol_vec.size();
		}

		void assign(const std::wstring& number_str, number_base* base = nullptr) {
			if (base) this->base = base;

			/* no base is avalible */
			if (!this->base) throw;

			digits.resize(number_str.size(), { 0, 0, 0 });

			bool symbol_exists{};
			std::uint64_t symbol_ind{};

			/* loop over the number backwards as the most significant digit is on the left */
			for (std::uint64_t ind{ number_str.size() - 1 }; ind < number_str.size() && ind >= 0; ind--) {
				symbol_exists = false;
				symbol_ind = 0;

				/* check for valid symbol in base */
				for (auto&& symbol : this->base->symbol_vec) {
					if (symbol != number_str[ind]) {
						symbol_ind++;
						continue;
					}

					symbol_exists = true;
					break;
				}

				if (!symbol_exists) throw;

				digits[number_str.size() - ind - 1] = { number_str.size() - ind - 1, this->base->symbol_vec[symbol_ind], symbol_ind };
			}
		}

		number express_as(number_base* base) {
			number result{ base, L"" };
			result.digits.resize(this->digits.size(), { 0, 0, 0 });

			for (std::uint64_t ind{}; ind < this->digits.size(); ind++) {
				/* put values into the first digit and let the carry do its job */
				result.digits[0].symbol_ind += this->digits[ind].symbol_ind * std::pow(this->base->symbol_vec.size(), ind);

				if (result.digits[0].symbol_ind >= result.base->symbol_vec.size()) {
					std::uint64_t remainder{ result.digits[0].symbol_ind % result.base->symbol_vec.size() };
					std::uint64_t dividend{ result.digits[0].symbol_ind - remainder };
					std::uint64_t carry{ dividend / result.base->symbol_vec.size() };

					result.digits[0].symbol_ind = remainder;
					remainder = 0;

					std::size_t result_ind{ 1 };

					/* solve for the carry */
					do {
						if (result_ind >= result.digits.size()) result.digits.resize(result.digits.size() + 1, { 0, 0, 0 });

						result.digits[result_ind].symbol_ind += carry;
						carry = 0;

						/* recalculate the remainder and carry */
						if (result.digits[result_ind].symbol_ind >= result.base->symbol_vec.size()) {
							remainder = result.digits[result_ind].symbol_ind % result.base->symbol_vec.size();
							dividend = result.digits[result_ind].symbol_ind - remainder;
							carry = dividend / result.base->symbol_vec.size();

							result.digits[result_ind].symbol_ind = remainder;
							remainder = 0;

							result_ind += 1;

							continue;
						}
					} while (carry != 0);
				}
			}

			std::size_t last_zero{};
			/* loop over the digits of the result and fill in the symbol and power values */
			for (std::uint64_t ind{}; ind < result.digits.size(); ind++) {
				if (result.digits[ind].symbol_ind == 0 && last_zero == 0) last_zero = ind;
				else if (result.digits[ind].symbol_ind != 0) last_zero = 0;

				result.digits[ind].power = ind;
				result.digits[ind].symbol = result.base->symbol_vec[result.digits[ind].symbol_ind];
			}

			if (last_zero > 0) {
				result.digits.resize(last_zero, { 0, 0, 0 });
			}

			return result;
		}

		void convert(number_base* base) {
			*this = this->express_as(base);
		}

		friend std::wostream& operator<<(std::wostream& wos, const number& obj) {
			std::wstring buffer{};

			/* loop over the number backwards,
			   as the least significant digit is index 0 and the most significant is index n */
			for (std::uint64_t ind{ obj.digits.size() - 1 }; ind < obj.digits.size() && ind >= 0; ind--) {
				buffer += obj.digits[ind].symbol;
			}

			wos << buffer;

			return wos;
		}

		friend number& operator+(const number& lhs, number& rhs) {
			rhs.convert(lhs.base);

			/* Take the base of the left most number */
			number result{ rhs };

			for (std::uint64_t ind{}; ind < lhs.digits.size(); ind++) {
				/* put values into the first digit and let the carry do its job */
				result.digits[0].symbol_ind += lhs.digits[ind].symbol_ind * std::pow(lhs.base->symbol_vec.size(), ind);

				if (result.digits[0].symbol_ind >= result.base->symbol_vec.size()) {
					std::uint64_t remainder{ result.digits[0].symbol_ind % result.base->symbol_vec.size() };
					std::uint64_t dividend{ result.digits[0].symbol_ind - remainder };
					std::uint64_t carry{ dividend / result.base->symbol_vec.size() };

					result.digits[0].symbol_ind = remainder;
					remainder = 0;

					std::size_t result_ind{ 1 };

					/* solve for the carry */
					do {
						if (result_ind >= result.digits.size()) result.digits.resize(result.digits.size() + 1, { 0, 0, 0 });

						result.digits[result_ind].symbol_ind += carry;
						carry = 0;

						/* recalculate the remainder and carry */
						if (result.digits[result_ind].symbol_ind >= result.base->symbol_vec.size()) {
							remainder = result.digits[result_ind].symbol_ind % result.base->symbol_vec.size();
							dividend = result.digits[result_ind].symbol_ind - remainder;
							carry = dividend / result.base->symbol_vec.size();

							result.digits[result_ind].symbol_ind = remainder;
							remainder = 0;

							result_ind += 1;

							continue;
						}
					} while (carry != 0);
				}
			}

			rhs.digits = result.digits;

			return rhs;
		}
	};

}