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

struct config;
struct sequence;
struct pz;

enum struct symbol;

symbol operator+(const symbol&, int);
symbol& operator+=(symbol&, int);

using _sequence = std::vector<symbol>;
struct sequence : _sequence {
	using _sequence::_sequence;
	explicit operator std::string() const;
};

using histogram = std::map<sequence,size_t>;
using dictionary = std::map<symbol,sequence>;
using block = std::list<symbol>;
template <typename T> using meta = std::pair<bool, T>;

const char *pz_extension = ".pz";

void usage(const char *);

bool pz_process_file(const config&, const char *);
bool pz_process_fd(const config&, int, int);

bool pz_compress(const config&, int, int);
bool pz_decompress(const config&, int, int);

bool pz_compare_symbols(symbol, symbol);

template <typename T> bool pz_match_sequence(T, T, const sequence&);
template <typename T> T pz_find_sequence(T, T, const sequence&);
template <typename T> typename T::iterator pz_erase_sequence(T&, typename T::iterator, const sequence&);
template <typename T> typename T::iterator pz_replace_next_sequence(T&, typename T::iterator, const sequence&, symbol);
template <typename T> int pz_replace_sequence(T&, const sequence&, symbol);
template <typename T> histogram pz_get_histogram(const T&, size_t);
size_t pz_bytes_used(const histogram::value_type&);
histogram::iterator pz_get_best_sequence(histogram&);
meta<block> pz_get_block(int);

struct config {

	bool help = false;
	bool compress = true;
	bool keep = false;
	bool force = false;
	bool stdoutput = false;
	bool quiet = false;
	bool verbose = false;

	std::list<std::string> files;
};

enum struct symbol : int {
	wildcard = -1,
	first = 256
};

symbol operator+(const symbol& x, int y) {
	return symbol((int)x + y);
}

symbol& operator+=(symbol& x, int y) {
	x = x + y;
	return x;
}

sequence::operator std::string() const {
	std::stringstream ss;
	auto iter = begin();
	if(iter != end())
		ss << (int)(*iter++);
	while(iter != end())
		ss << ' ' << (int)(*iter++);
	return ss.str();
}

void usage(const char *prog) {

	std::cerr << prog << " file compressor." << std::endl << std::endl <<

		"usage: " << prog << " [{option|file}]..." << std::endl << std::endl <<

		"\t-h\tprint this message" << std::endl <<
		"\t-d\tforce decompression" << std::endl <<
		"\t-z\tforce compression" << std::endl <<
		"\t-k\tkeep (don't delete) input files" << std::endl <<
		"\t-f\toverwrite existing output files" << std::endl <<
		"\t-c\toutput to standard out" << std::endl <<
		"\t-q\tsuppress output messages" << std::endl <<
		"\t-v\tbe verbose" << std::endl << std::endl <<

		"If no file names are given, " << prog << " uses stdin/stdout." << std::endl << std::endl;
}

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

template <class T> bool pz_match_sequence(T begin, T end, const sequence& s) {

	auto ster = s.begin();
	auto bter = begin;

	while(ster != s.end())
		if(bter == end or not pz_compare_symbols(*ster++, *bter++))
			return false;
	
	return true;
}

template <class T> T pz_find_sequence(T begin, T end, const sequence& s) {

	for(auto position = begin; position != end; position++)
		if(pz_match_sequence(position, end, s))
			return position;

	return end;
}

template <typename T> typename T::iterator pz_erase_sequence(T& b, typename T::iterator pos, const sequence& s) {

	auto bter = pos--;
	
	for(auto ster = s.begin(); ster != s.end(); ster++) {

		if(bter == b.end())
			throw std::runtime_error("pz_erase_sequence(): unexpectedly read past last element in object");
		
		auto cter = bter++;

		if(*ster == symbol::wildcard) {
			// do nothing
		} else if(*cter == *ster) {
			b.erase(cter);
		} else {
			throw std::runtime_error("pz_erase_sequence(): unexpectedly read past last element in object");
		}
	}

	return ++pos;
}

template <typename T> typename T::iterator pz_replace_next_sequence(T& b, typename T::iterator pos, const sequence& s, symbol x) {

	pos = pz_find_sequence(pos, b.end(), s);

	if(pos != b.end()) {
		pos = pz_erase_sequence(b, pos, s);
		pos = b.insert(pos, x);
	}

	return pos;
}

template <typename T> int pz_replace_sequence(T& b, const sequence& s, symbol x) {

	int n = 0;

	auto position = b.begin();

	while((position = pz_replace_next_sequence(b, position, s, x)) != b.end())
		n++;

	return n;
}

template <typename T> histogram pz_get_histogram(const T& b, size_t maxlen) {


	histogram h;

	for(auto iter = b.begin(); iter != b.end(); iter++) {


		const size_t minlen1 = 3;
		sequence key1;

		for(auto jter = iter; jter != b.end() and key1.size() < maxlen; jter++) {

			key1.push_back(*jter);

			if(key1.size() >= minlen1)
				h[key1]++;
		}

		const size_t minlen2 = 2;
		sequence key2;

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
	/*
	return kv.second;
	*/
	size_t n = 0;
	for(auto x : kv.first)
		if(x != symbol::wildcard)
			n++;
	return n * kv.second;
}

histogram::iterator pz_get_best_sequence(histogram& h) {

	auto iter = std::max_element(
		h.begin(),
		h.end(),
		[] (histogram::value_type a, histogram::value_type b) -> bool { return pz_bytes_used(b) > pz_bytes_used(a); }
	);

	return ( iter != h.end() and iter->second > 1 ) ? iter : h.end();
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

bool pz_process_fd(const config&, int fdin, int) {

	const size_t sequence_maxlen = 6;

	dictionary d;

	symbol current_symbol = symbol::first;

	meta<block> mb = pz_get_block(fdin);

	if(!mb.first) {
		std::cerr << strerror(errno) << std::endl;
		return false;
	}

	block b = mb.second;

	std::cerr << " : " << b.size() << " bytes" << std::endl;

	histogram h = pz_get_histogram(b, sequence_maxlen);

	auto best = pz_get_best_sequence(h);
	size_t best_sz = best->second;

	for(;;) {

		best = pz_get_best_sequence(h);

		if(best == h.end() or best->second * 2 < best_sz) {

			std::cerr << "updating histogram" << std::endl;

			h = pz_get_histogram(b, sequence_maxlen);

			best = pz_get_best_sequence(h);

			if(best == h.end())
				throw std::runtime_error("nothing to compress");

			best_sz = best->second;
		}

		const sequence& best_sequence = best->first;

		// compress

		int replacements = pz_replace_sequence(b, best_sequence, current_symbol);

		if(replacements > 0) {

			std::cerr << "replaced " << replacements << " instances of " << (std::string)best_sequence;
			std::cerr << " with symbol " << (int)current_symbol << " : " << b.size() << " bytes" << std::endl;

			d[current_symbol] = best_sequence;
			current_symbol += 1;
		}

		h.erase(best);
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

	int opt;

	config cfg;

	while ((opt = getopt(argc, argv, "hdzkfcqv")) != -1) {

		switch (opt) {

			case 'h': cfg.help      = true  ; break;
			case 'd': cfg.compress  = false ; break;
			case 'z': cfg.compress  = true  ; break;
			case 'k': cfg.keep      = true  ; break;
			case 'f': cfg.force     = true  ; break;
			case 'c': cfg.stdoutput = true  ; break;
			case 'q': cfg.quiet     = true  ; break;
			case 'v': cfg.verbose   = true  ; break;

			default:
				  std::cerr << "Try `" << *argv << " -h' for more information." << std::endl;
				  return -1;
		}
	}

	for(int i = optind; i < argc; i++)
		cfg.files.push_back(argv[i]);

	if(cfg.help) {
		usage(*argv);
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
