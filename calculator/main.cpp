#include "calc_numbers.h"

#include <vector>

//calc::number_base base2{ { '0', '1'} };
//calc::number_base base3{ { '0', '1', '2' } };
//calc::number_base base5{ { '0', '1', '2', '3', '4' } };
//calc::number_base base10{ { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' } };
//calc::number_base base16{ { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' } };

calc::number_base base2{ "01" };
calc::number_base base3{ "012" };
calc::number_base base5{ "01234" };
calc::number_base base10{ "0123456789" };
calc::number_base base16{ "0123456789abcdef" };

int main() {
	calc::number num1{ &base10 };
	calc::number num2{ &base10 };

	std::vector<int> vec{};

	num1.assign("1234567890");

	return 0;
}