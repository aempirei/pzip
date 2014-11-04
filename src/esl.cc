////////////////////////////////////////
//                                    //
// esl (english as a second language) //
// automatic language modeling tool   //
//                                    //
// Copyright(c) 2014 256 LLC          //
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

enum struct atom : int { wildcard = -1 };

using histogram_type = std::map<atom, size_t>;
using arrow_type = histogram_type::value_type;
using sequence_type = std::list<atom>;

inline bool atom_wildcard_eq_compare(const atom& a, const atom& b) { return a == b or a == atom::wildcard or b == atom::wildcard; }
inline bool arrow_value_lt_compare(const arrow_type& a, const arrow_type& b) { return a.second < b.second; }

template <typename T> bool prefix_matches(T haystack_pos, T haystack_end, T needle_pos, T needle_end) {
		while(haystack_pos != haystack_end and needle_pos != needle_end) {
				if(not atom_wildcard_eq_compare(*haystack_pos, *needle_pos))
						return false;
				haystack_pos++;
				needle_pos++;
		}
		return (needle_pos == needle_end);
}

template <typename T> T find_subseq(T haystack_pos, T haystack_end, T needle_pos, T needle_end) {

		while(haystack_pos != haystack_end) {
				if(prefix_matches(haystack_pos, haystack_end, needle_pos, needle_end))
						return haystack_pos;
				haystack_pos++;
		}

		return haystack_end;
}

#define OVERLOAD_RELATION(F, T, S) \
		inline bool operator F(T a, S b) { return ((S)a F b); }

#define OVERLOAD_OPERATOR(F, T, S) \
		inline T operator F(T a, S b) { return T((S)a F b); } \
		inline T operator F##=(T& a, S b) { return ( a = a F b ); } 

OVERLOAD_OPERATOR(+, atom, int)
OVERLOAD_OPERATOR(-, atom, int)
OVERLOAD_OPERATOR(*, atom, int)
OVERLOAD_OPERATOR(|, atom, int)
OVERLOAD_OPERATOR(^, atom, int)
OVERLOAD_OPERATOR(&, atom, int)

OVERLOAD_RELATION(> , atom, int)
OVERLOAD_RELATION(< , atom, int)
OVERLOAD_RELATION(>=, atom, int)
OVERLOAD_RELATION(<=, atom, int)
OVERLOAD_RELATION(==, atom, int)
OVERLOAD_RELATION(!=, atom, int)

std::string to_string(const sequence_type& s) {
		std::stringstream ss;
		ss << '/';
		for(atom a : s) {
				if(a == atom::wildcard)
						ss << '.';
				else if(a == '/' or a == '.')
						ss << '\\' << (char)a;
				else if(a >= 32 and a <= 126)
						ss << (char)a;
				else
						ss << "\\x" << std::hex << std::setw(2) << std::setfill('0') << std::noshowbase << (int)a;
		}
		ss << '/';
		return ss.str();
}

int main() {

		histogram_type histogram;
		sequence_type sequence;
		int ch;

		while((ch = getchar()) != EOF) {
				sequence.push_back((atom)ch);
				histogram[(atom)ch]++;
		}

		auto max = std::max_element(histogram.begin(), histogram.end(), arrow_value_lt_compare);

		atom composition = max->first;

		std::cout << "composition operator := '" << (char)composition << "' " << std::hex << std::showbase << (int)composition << std::endl;

		sequence_type pattern = { composition, atom::wildcard, composition };

		std::cout << "nullary pattern := " << to_string(pattern) << std::endl;

		histogram.clear();

		auto match = sequence.begin();

		size_t total = 0;

		while((match = find_subseq(match, sequence.end(), pattern.begin(), pattern.end())) != sequence.end()) {
				match++;
				histogram[*match]++;
				total++;
		}

		std::cout << "nullary histogram (mean " << std::dec << std::noshowbase << (total / histogram.size()) << ") := {" << std::endl;

		for(auto& arrow : histogram)
				if(2 * arrow.second * histogram.size() > total)
						std::cout << (char)arrow.first << " := " << arrow.second << std::endl;

		std::cout << '}' << std::endl;

		return 0;
}
