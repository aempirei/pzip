////////////////////////////////////////
//                                    //
// Copyright(c) 2016 256 LLC          //
// Written by Christopher Abad        //
// aempirei@256.bz                    //
// 20 GOTO 10                         //
//                                    //
////////////////////////////////////////

#include <iomanip>
#include <iostream>

#include <string>
#include <sstream>
#include <list>
#include <map>
#include <algorithm>

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cctype>

namespace ASCII {
	constexpr char STX = '\2';
	constexpr char ETX = '\3';
}

using sequence = std::list<char>;
using table = std::list<sequence>;

void usage(const char *);
std::string sequence_string(const sequence&);

int main(int argc, char **argv) {

	table messages;

	sequence message;
	sequence head;
	sequence back;

	if(argc == 1) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	message.push_back(ASCII::STX);

	for(int n = 1; n < argc; n++) {
		message.insert(message.end(), argv[n], argv[n] + strlen(argv[n]));
		message.push_back(' ');
	}

	message.pop_back();
	message.push_back(ASCII::ETX);

	for(size_t n = 0; n < message.size(); n++) {
		messages.push_back(message);
		message.push_back(message.front());
		message.pop_front();
	}

	messages.sort();

	for(const auto& s : messages) {
		back.push_back(s.back());
		head.push_back(s.front());
	}

	std::cout << "head: " << sequence_string(head) << std::endl;
	std::cout << "back: " << sequence_string(back) << std::endl;

	std::cout << std::endl;

	auto print_table = [](const table& t) {
		for(const auto& s : t)
			std::cout << sequence_string(s) << std::endl;
		std::cout << std::endl;
	};

	print_table(messages);

	messages.clear();

	for(auto ch : head)
		messages.push_back({ch});

	print_table(messages);

	while(messages.front().size() < message.size()) {

		auto mter = messages.begin();
		auto bter = back.begin();

		for(size_t n = 0; n < message.size(); n++)
			(mter++)->push_front(*bter++);

		messages.sort();

		print_table(messages);

		std::cout << "press <ENTER> to continue." << std::flush;

		(void)getchar();
	}

	exit(EXIT_SUCCESS);
}

std::string sequence_string(const sequence& seq) {
	std::stringstream ss;
	for(char ch : seq) {
		if(ch == ASCII::STX)
			ss << "\33[1;33m^\33[0m";
		else if(ch == ASCII::ETX)
			ss << "\33[1;34m$\33[0m";
		else
			ss << ch;
	}
	return ss.str();
}

void usage(const char *prog) {
	std::cerr << std::endl;
	std::cerr << "usage: " << prog << " message" << std::endl;
	std::cerr << std::endl;
}
