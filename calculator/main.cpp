#include "calc_numbers.h"

#include <vector>
#include <io.h>      // For _setmode
#include <fcntl.h>   // For _O_U16TEXT

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
	if (!_setmode(_fileno(stdout), _O_U16TEXT)) {
		return 1;
	}

	calc::number num1{ &base10 };
	calc::number num2{ &base10 };
	calc::number num3{ &base10 };
	calc::number num4{ &base10 };

	std::vector<int> vec{};

	num1.assign("11145 67821 34621 78346 17823 64");
	num2.assign("22222222");
	num3.assign("33333333");
	num4.assign("44444444");

	//num1.print();
	//num2.print();
	//num3.print();
	//num4.print();

	//std::wcout << num1;

	auto&& prob{ num1 * (num3 * num4 + num2) };

	auto&& exp_size{ prob->expression.size() };

	for (std::size_t ind{}; ind < exp_size; ind++) {
		if (prob->expression[ind]->term == nullptr)
			continue;

		std::wcout << *prob->expression[ind]->term << " " << prob->expression[ind]->operand;

		if (ind < exp_size - 1)
			std::wcout << "\n";
	}

	return 0;
}