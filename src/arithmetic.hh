#pragma once

#include <climits>

#define overload_infix(B,R,T)    inline T  operator R   (const T& x, B y) { return T((B)x R y); }
#define overload_compound(B,R,T) inline T& operator R##=(      T& x, B y) { return ( x = operator R(x,y) ); } 
#define overload_operator(B,R,T) overload_infix(B,R,T) overload_compound(B,R,T) static_assert(true,"")

#define arithmetic(K,B)							\
									\
enum struct K : B {							\
	min = std::numeric_limits<B>::min(),				\
	max = std::numeric_limits<B>::max()				\
};									\
									\
overload_operator(B,+,K);						\
overload_operator(B,-,K);						\
overload_operator(B,*,K);						\
overload_operator(B,%,K);						\
									\
inline K& operator++(K& x) { return operator+=(x,1); }			\
inline K& operator--(K& x) { return operator-=(x,1); }			\
									\
inline K operator++(K& x, int) { K y=x; operator++(x); return y; }	\
inline K operator--(K& x, int) { K y=x; operator--(x); return y; }	\
static_assert(true,"")

template <typename T> constexpr std::size_t ordinality(const T&) {
	static_assert(not std::is_class   <T>::value, "ordinality() shouldn't be a class");
	static_assert(not std::is_void    <T>::value, "ordinality() shouldn't be void");
	static_assert(not std::is_function<T>::value, "ordinality() shouldn't be a function");
	return 1L << (sizeof(T) * CHAR_BIT);
}
