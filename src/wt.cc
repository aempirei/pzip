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

template <typename T> T rotate(const T& x) {
	T y;
	if(not x.empty()) {
		y.push_back(x.back());
		y.insert(y.end(), x.begin(), std::prev(x.end()));
	}
	return y;
}

template <typename T> table zip(const T& x) {
	table z;
	T y = x;
	for(size_t n = 0; n < x.size(); n++) {
		z.push_back(sequence(y.begin(), y.end()));
		y = rotate(y);
	}
	return z;
}

using sequence_method = const alphabet& (sequence::*)() const;

string extract(const table& t, sequence_method f) {
	string s;
	for(const auto& x : t)
		s.push_back((x.*f)());
	return s;
}

void print_function(const string& X, const string& Y) {
	std::cout << "dom: " << X << std::endl;
	std::cout << "cod: " << Y << std::endl;
	std::cout << std::endl;
}

void print_table(const table& t) {
	for(const auto& s : t) {
		for(auto ch : s)
			std::cout << ch;
		std::cout << std::endl;
	}
	std::cout << std::endl;
}

template <typename T> void step_transform(table& S, const T& Y) {

	auto spos = S.begin();
	auto ypos = Y.begin();

	while(spos != S.end() and ypos != Y.end())
		(spos++)->push_front(*ypos++);

	S.sort();
}

void usage(const char *prog) {
	std::cerr << std::endl;
	std::cerr << "usage: " << prog << " message" << std::endl;
	std::cerr << std::endl;
}

int main(int argc, char **argv) {

	table F;
	string X;
	string Y;
	table S;

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
	X.push_back('\0');

	F = zip(X);

	F.sort();

	X = extract(F, &sequence::front);
	Y = extract(F, &sequence::back);

	print_function(X,Y);

	S.assign(Y.size(), {});

	while(S.front().size() < Y.size())
		step_transform(S, Y);

	print_table(S);

	exit(EXIT_SUCCESS);
}
