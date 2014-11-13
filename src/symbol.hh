#pragma once

enum struct symbol : int {
    wildcard  =  -1,
    ascii_min =   0,
    ascii_max = 255,
    first     = 256
};

#define overload_infix(R,T)     inline T operator R(const T& x, int y) { return T((int)x R y); }
#define overload_compound(R,T)  inline T& operator R##=(T& x, int y) { return ( x = operator R(x,y) ); } 
#define overload_operator(R,T)  overload_infix(R,T) overload_compound(R,T) static_assert(true,"")

overload_operator(+,symbol);
overload_operator(-,symbol);
overload_operator(*,symbol);
overload_operator(%,symbol);

inline symbol& operator++(symbol& x) { return operator+=(x,1); }
inline symbol& operator--(symbol& x) { return operator-=(x,1); }

inline symbol operator++(symbol& x, int) { symbol y = x; operator++(x); return y; }
inline symbol operator--(symbol& x, int) { symbol y = x; operator--(x); return y; }
