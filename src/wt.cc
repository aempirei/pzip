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

using alphabet = char;
using sequence = std::list<alphabet>;
using table = std::list<sequence>;
using string = std::basic_string<alphabet>;

void usage(const char *);
std::string sequence_string(const sequence&);
void print_table(const table&);

int main(int argc, char **argv) {

	table F;
	string X;
	string Y;
	sequence seq;

	if(argc == 1) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	for(int n = 1; n < argc; n++) {
		char *p = argv[n];
		while(*p != '\0')
			X.push_back(*p++);
		X.push_back(' ');
	}

	X.pop_back();
	X.push_back('$');

	seq.assign(X.begin(), X.end());
	seq.push_front(seq.back());
	seq.pop_back();

	Y.assign(seq.begin(), seq.end());

	std::cout << "dom: " << X << std::endl;
	std::cout << "cod: " << Y << std::endl;

	exit(EXIT_SUCCESS);
}

void print_table(const table& t) {
	for(const auto& s : t)
		std::cout << sequence_string(s) << std::endl;
	std::cout << std::endl;
}

std::string sequence_string(const sequence& seq) {
	std::stringstream ss;
	for(char ch : seq) {
		if(ch == '\0')
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
