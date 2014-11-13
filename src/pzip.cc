/////////////////////////////////
//                             //
// pzip                        //
// file compression tool       //
//                             //
// Copyright(c) 2014 256 LLC   //
// Written by Christopher Abad //
// aempirei@256.bz             //
// 20 GOTO 10                  //
//                             //
/////////////////////////////////

#include <iomanip>
#include <iostream>

#include <cstring>
#include <cstdlib>
#include <cstdio>

#include <string>
#include <sstream>
#include <iterator>
#include <list>
#include <vector>
#include <map>
#include <algorithm>

extern "C" {
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
}

#include <libpz.hh>
#include <config.hh>
#include <symbol.hh>

template <typename T> using meta = std::pair<bool, T>;
template <typename T> using metric = std::map<T,size_t>;

using block = std::list<symbol>;
using histogram = metric<block>;
using dictionary = std::map<symbol,block>;
using rdictionary = std::map<block,symbol>;

const char *pz_extension = ".pz";

bool pz_process_file(const config&, const char *);
bool pz_process_fd(const config&, int, int);

bool pz_compress(const config&, int, int);
bool pz_decompress(const config&, int, int);

bool pz_compare_symbols(symbol, symbol);

template <typename T> bool pz_match_block(T, T, const block&);
template <typename T> T pz_find_block(T, T, const block&);
template <typename T> typename T::iterator pz_erase_block(T&, typename T::iterator, const block&);
template <typename T> typename T::iterator pz_replace_next_block(T&, typename T::iterator, const block&, symbol);
template <typename T> int pz_replace_block(T&, const block&, symbol);
template <typename T> histogram pz_get_histogram(const T&, size_t);

size_t pz_bytes_used(const histogram::value_type&);
histogram::iterator pz_get_best_block(histogram&);
meta<block> pz_get_block(int);
metric<symbol> pz_symbol_histogram(const block&, const dictionary&);

const static std::map<unsigned int, const char *> file_type = {
	{ S_IFBLK,  "block device" }, 
	{ S_IFCHR,  "character device" }, 
	{ S_IFDIR,  "directory" }, 
	{ S_IFIFO,  "FIFO/pipe" }, 
	{ S_IFLNK,  "symlink" }, 
	{ S_IFREG,  "regular file" }, 
	{ S_IFSOCK, "socket" }
};

const char *get_file_type(const struct stat& sb) {

	unsigned int mode = sb.st_mode &  S_IFMT;
	auto iter = file_type.find(mode);

	if(iter == file_type.end())
		return "unknown";

	return iter->second;
}

bool pz_compare_symbols(symbol a, symbol b) {
	return a == b or a == symbol::wildcard or b == symbol::wildcard;
}

template <class T> bool pz_match_block(T begin, T end, const block& s) {

	auto ster = s.begin();
	auto bter = begin;

	while(ster != s.end())
		if(bter == end or not pz_compare_symbols(*ster++, *bter++))
			return false;
	
	return true;
}

template <class T> T pz_find_block(T begin, T end, const block& s) {

	for(auto position = begin; position != end; ++position)
		if(pz_match_block(position, end, s))
			return position;

	return end;
}

template <typename T> typename T::iterator pz_erase_block(T& b, typename T::iterator pos, const block& s) {

	auto bter = pos--;
	
	for(auto ster = s.begin(); ster != s.end(); ster++) {

		if(bter == b.end())
			throw std::runtime_error("pz_erase_block(): unexpectedly read past last element in object");
		
		auto cter = bter++;

		if(*ster == symbol::wildcard) {
			// do nothing
		} else if(*cter == *ster) {
			b.erase(cter);
		} else {
			throw std::runtime_error("pz_erase_block(): unexpectedly read past last element in object");
		}
	}

	return ++pos;
}

template <typename T> typename T::iterator pz_replace_next_block(T& b, typename T::iterator pos, const block& s, symbol x) {

	pos = pz_find_block(pos, b.end(), s);

	if(pos != b.end()) {
		pos = pz_erase_block(b, pos, s);
		pos = b.insert(pos, x);
	}

	return pos;
}

template <typename T> int pz_replace_block(T& b, const block& s, symbol x) {

	int n = 0;

	auto position = b.begin();

	while((position = pz_replace_next_block(b, position, s, x)) != b.end())
		n++;

	return n;
}

template <typename T> histogram pz_get_histogram(const T& b, size_t maxlen) {


	histogram h;

	for(auto iter = b.begin(); iter != b.end(); iter++) {


		const size_t minlen1 = 3;
		block key1;

		for(auto jter = iter; jter != b.end() and key1.size() < maxlen; jter++) {

			key1.push_back(*jter);

			if(key1.size() >= minlen1)
				h[key1]++;
		}

		const size_t minlen2 = 2;
		block key2;

		key2.push_back(*iter);

		for(auto jter = next(iter); jter != b.end() and key2.size() < maxlen; jter++) {

			key2.push_back(*jter);

			if(key2.size() >= minlen2)
				h[key2]++;

			key2.back() = symbol::wildcard;
		}
	}

	return h;
}

size_t pz_bytes_used(const histogram::value_type& kv) {

    size_t n = 0;
    for(auto x : kv.first)
        if(x != symbol::wildcard)
            n++;

    return n * kv.second;
}

histogram::iterator pz_get_best_block(histogram& h) {

    auto iter = std::max_element(
            h.begin(),
            h.end(),
            [] (histogram::value_type a, histogram::value_type b) -> bool { return pz_bytes_used(b) > pz_bytes_used(a); }
            );

    return ( iter != h.end() and iter->second > 1 ) ? iter : h.end();
}

bool pz_trim_histogram(histogram& h) {

    auto best = pz_get_best_block(h);

    if(best == h.end())
        return false;

    size_t minval = (size_t)rint(sqrt(best->second));

    if(minval < 2)
        minval = 2;

    for(auto iter = h.begin(); iter != h.end(); iter++)
        if(iter->second < minval)
            iter = prev(h.erase(iter));

    return true;
}

meta<block> pz_get_block(int fd) {
    block b;

    unsigned char buf[1024];
    int n;

    do {
        n = read(fd, buf, sizeof(buf));
        if(n == -1) {
            if(errno == EAGAIN)
                continue;
            return meta<block>(false,block());
        }

        for(int i = 0; i < n; i++)
            b.push_back((symbol)buf[i]);

    } while(n != 0);

    return meta<block>(true,b);
}

size_t pz_expand_singletons(block& b, dictionary& d, metric<symbol>& sh) {

    size_t n = 0;

    auto pos = b.begin();

    while(pos != b.end()) {

        const symbol sym = *pos;

        if(sym >= symbol::first and sh[sym] == 1) {

            const block& seq = d[sym];

            pos = b.erase(pos);
            pos = b.insert(pos, seq.begin(), seq.end());

            n++;

        } else {

            pos++;
        }
    }

    return n;
}
 
size_t pz_expand_singletons(dictionary& d, metric<symbol>& sh) {

    size_t n = 0;

    for(auto& rule : d) {
        block b(rule.second.begin(), rule.second.end());
        n += pz_expand_singletons(b, d, sh);
        rule.second = block(b.begin(), b.end());
    }

    return n;
}

void write_utf8(unsigned int code_point) {
    if (code_point < 0x80) {
        putchar(code_point);
    } else if (code_point <= 0x7FF) {
        putchar((code_point >> 6) + 0xC0);
        putchar((code_point & 0x3F) + 0x80);
    } else if (code_point <= 0xFFFF) {
        putchar((code_point >> 12) + 0xE0);
        putchar(((code_point >> 6) & 0x3F) + 0x80);
        putchar((code_point & 0x3F) + 0x80);
    } else if (code_point <= 0x10FFFF) {
        putchar((code_point >> 18) + 0xF0);
        putchar(((code_point >> 12) & 0x3F) + 0x80);
        putchar(((code_point >> 6) & 0x3F) + 0x80);
        putchar((code_point & 0x3F) + 0x80);
    }
}

metric<symbol> pz_symbol_histogram(const block& b, const dictionary& d) {

    metric<symbol> h;

    for(symbol x : b)
        if(x >= symbol::first)
            h[x]++;

    for(const auto& r : d)
        for(symbol x : r.second)
            if(x >= symbol::first)
                h[x]++;

    return h;
}

void pz_expand(block& b, const dictionary& d) {

    auto iter = b.begin();

    while(iter != b.end()) {
        symbol x = *iter;
        const auto rule = d.find(x);
        if(rule == d.end()) {
            iter++;
        } else {
            iter = b.erase(iter);
            iter = b.insert(iter, rule->second.begin(), rule->second.end());
        }
    }
}

void pz_remap(block& b, dictionary& d) {

    std::map<symbol,symbol> remap;

    symbol current = symbol::first;

    for(const auto& rule : d)
        if(rule.first >= symbol::first)
            remap[rule.first] = current++;

    for(symbol& x : b)
        if(x >= symbol::first)
            x = remap.at(x);

    dictionary e;

    for(const auto& rule : d) {

        symbol x = rule.first;
        block s = rule.second;

        for(auto iter = s.begin(); iter != s.end(); ++iter)
            if(*iter >= symbol::first)
                *iter = remap.at(*iter);

        e[remap.at(x)] = s;
    }

    d = e;
}

bool pz_process_fd(const config&, int fdin, int) {

    const size_t block_maxlen = 2;

    symbol current_symbol = symbol::first;

    meta<block> mb = pz_get_block(fdin);

    if(!mb.first) {
        std::cerr << strerror(errno) << std::endl;
        return false;
    }

    block b = mb.second;

    dictionary d;

    for(int i = 0; i < 2; i++) {

        rdictionary r;

        std::cerr << " : " << b.size() << " symbols" << std::endl;

        for(;;) {

            histogram h = pz_get_histogram(b, block_maxlen);

            if(not pz_trim_histogram(h))
                break;

            auto bpos = b.begin();

            while(next(bpos) != b.end()) {

                const block x = { *bpos, *next(bpos) };

                auto jter = h.find(x);

                if(jter == h.end()) {
                    bpos++;
                } else {

                    symbol s;
                    auto kter = r.find(x);

                    if(kter == r.end()) {

                        s = current_symbol;
                        d[current_symbol] = x;
                        r[x] = current_symbol++;

                    } else {

                        s = kter->second;
                    }

                    bpos = pz_erase_block(b, bpos, x);
                    bpos = b.insert(bpos, s);
                }
            }

            std::cerr << "document: " << b.size() << " symbols ~ ";
            std::cerr << "dictionary: " << d.size() << " symbols" << std::endl;
        }

        auto sh1 = pz_symbol_histogram(b,d);

        pz_expand_singletons(b, d, sh1);
        pz_expand_singletons(d, sh1);

        auto sh2 = pz_symbol_histogram(b,d);

        auto iter = d.begin();

        while(iter != d.end()) {
            if(sh2.find(iter->first) == sh2.end())
                iter = d.erase(iter);
            else
                iter++;
        }

        std::cerr << "document: " << b.size() << " symbols ~ ";
        std::cerr << "dictionary: " << d.size() << " symbols" << std::endl;
    }

    pz_remap(b, d);

    if(false) {

        pz_expand(b,d);

        for(symbol x : b) {
            if(x >= (symbol)0 and x < symbol::first)
                putchar((char)x);
            else
                throw std::runtime_error("FUCKED!");
        }

    } else {

        for(const auto& rule : d) {

            write_utf8((unsigned int)rule.first);
            write_utf8(rule.second.size());

            for(symbol x : rule.second)
                write_utf8((unsigned int)x);
        }

        for(symbol x : b)
            write_utf8((unsigned int)x);
    }

    return true;
}

bool pz_process_file(const config& cfg, const char *filenamein) {

	int fdin;
	int fdout;
	struct stat sb;

	std::cerr << std::setw(12) << filenamein << ": ";

	if(lstat(filenamein, &sb) == -1) {
		std::cerr << strerror(errno) << std::endl;
		return false;
	}

	if(S_ISDIR(sb.st_mode)) {
		std::cerr << "ignoring directory" << std::endl;
		return false;
	}

	if(cfg.stdoutput) {

		// if output is standard output then
		// input type doesnt matter 
		// as long as it can be read

		fdout = STDOUT_FILENO;

	} else {

		// if output is not standard output
		// then only compress regular files

		if(not S_ISREG(sb.st_mode)) {
			std::cerr << "ignoring " << get_file_type(sb) << std::endl;
			return false;
		}

		// if output is not standard output
		// then make sure the filenamein suffix
		// matches the mode of operation

		const size_t extension_sz = strlen(pz_extension);
		const size_t filenamein_sz = strlen(filenamein);

		const char *p = filenamein + filenamein_sz;

		std::string filenameout(filenamein);

		if(extension_sz < filenamein_sz)
			p -= extension_sz;

		if(strcmp(p, pz_extension) == 0) {
			if(cfg.compress) {
				std::cerr << "already has " << pz_extension << " suffix -- ignored" << std::endl;
				return false;
			}
			filenameout.erase(filenameout.end() - extension_sz, filenameout.end());
		} else {
			if(not cfg.compress) {
				std::cerr << "unknown suffix -- ignored" << std::endl;
				return false;

			}
			filenameout += pz_extension;
		}

		std::cerr << "=> " << filenameout;

		fdout = open(filenameout.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0664);
		if(fdout == -1) {
			std::cerr << strerror(errno) << std::endl;
			return false;
		}
	}

	fdin = open(filenamein, O_RDONLY);
	if(fdin == -1) {
		std::cerr << strerror(errno) << std::endl;
		if(fdout != STDOUT_FILENO)
			close(fdout);
		return false;
	}

	pz_process_fd(cfg, fdin, fdout);

	std::cerr << std::endl;

	close(fdin);
	if(fdout != STDOUT_FILENO)
		close(fdout);

	return true;
}

int main(int argc, char **argv) {

	config cfg;

        if(not cfg.getopt(argc, argv)) {
            std::cerr << "Try `" << *argv << " -h' for more information." << std::endl;
            return -1;
        }

	if(cfg.help) {
		cfg.usage(*argv);
		return 0;
	}

	if(cfg.files.empty()) {

		pz_process_fd(cfg, STDIN_FILENO, STDOUT_FILENO);

	} else { 

		for(auto file : cfg.files)
			pz_process_file(cfg, file.c_str());
	}

	return 0;
}
