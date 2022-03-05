#include "read_input_functions.h"
#include <iostream>


std::string ReadLine()
{
	std::string s;
	std::getline(std::cin, s);
	return s;
}

int ReadLineWithNumber()
{
	int result;
	std::cin >> result;
	ReadLine();
	return result;
}

std::vector<int> ReadRatingsLine()
{
	int n;
	std::cin >> n;
	std::vector<int> result(n, 0);
	for (int& rating : result)
	{
		std::cin >> rating;
	}
	ReadLine();
	return result;
}
