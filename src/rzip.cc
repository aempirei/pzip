/////////////////////////////////
//                             //
// rzip                        //
// file compression tool       //
//                             //
// Copyright(c) 2016 256 LLC   //
// Written by Christopher Abad //
// aempirei@256.bz             //
// 20 GOTO 10                  //
//                             //
/////////////////////////////////

#include <iomanip>
#include <iostream>
#include <cassert>

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
#include <set>

extern "C" {
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
}

#include <config.hh>

struct term;
struct dictionary;

using symbol = int32_t;
using expression = std::list<term>;
using rdictionary = std::map<expression, symbol>;

using term_baseclass = std::pair<size_t, symbol>;
using dictionary_baseclass = std::map<symbol, expression>;

using rule = dictionary_baseclass::value_type;

struct term : term_baseclass {
		using term_baseclass::term_baseclass;
		term(const symbol&);
};

struct dictionary : dictionary_baseclass {
		using dictionary_baseclass::dictionary_baseclass;
		expression& expand(expression&) const;
		key_type next_key() const;
};

term::term(const symbol& x) : term(1,x) {
}

dictionary::key_type dictionary::next_key() const {
		return -(size() + 256);
}

expression& dictionary::expand(expression& expr) const {

	auto pos = expr.begin();

	while(pos != expr.end()) {

			auto rule = find(pos->second);

			if(rule != end()) {
					for(size_t n = 0; n < pos->first; n++)
							expr.insert(std::next(pos), rule->second.begin(), rule->second.end());
					pos = expr.erase(pos);
			} else {
					while(pos->first-- > 1)
							expr.insert(pos, pos->second);
					pos++;
			}
	}

	return expr;
}

const char *rz_extension = ".rz";

bool rz_process_file(const config&, const char *);
bool rz_process_fd(const config&, int, int);

bool rz_compress_block(const config&, void *, size_t, int);

bool rz_compress(const config&, int, int);
bool rz_decompress(const config&, int, int);

constexpr char ansi_save_pos[] = "\033[s";
constexpr char ansi_load_pos[] = "\033[u";
constexpr char ansi_clear_line[] = "\033[K";

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

ssize_t read_block(int fd, void *block, size_t block_sz) {

		char *p = (char *)block;

		ssize_t left = block_sz;
		ssize_t done = 0;
		ssize_t n;

		while((n = read(fd, p, left)) != 0) {
				if(n > 0) {
						left -= n;
						done += n;
						p += n;
				} else if(n == -1 && errno != EINTR) {
						return -1;
				}
		}

		return done;
}

bool rz_compress_block(const config&, void *block, size_t block_sz, int) {
		
		expression expr((unsigned char *)block, (unsigned char *)block + block_sz);

		dictionary d;
		rdictionary r;

		size_t last_sz;

		std::cerr << std::endl;

		do {

				std::map <expression,size_t> histogram;

				last_sz = d.size();

				std::cerr << "d: " << d.size() << " e: " << expr.size() << std::endl;

				auto pos = expr.begin();

				while(std::next(pos) != expr.end()) {

						if(pos->second == std::next(pos)->second) {
								std::next(pos)->first += pos->first;
								pos = expr.erase(pos);
						} else {
								histogram[expression(pos, std::next(pos,2))]++;
								pos++;
						}
				}

				for(const auto& sr : histogram) {

						if(sr.second > 1) {
								const auto& x = sr.first;
								symbol s = d.next_key();
								d[s] = x;
								r[x] = s;
						}
				}

				pos = expr.begin();

				while(std::next(pos) != expr.end()) {

						auto rrule = r.find(expression(pos, std::next(pos,2)));

						if(rrule != r.end()) {
								expr.insert(pos, rrule->second);
								pos = expr.erase(pos, std::next(pos,2));
						} else {
								pos++;
						}
				}

		} while(d.size() > last_sz);

		std::map<symbol, size_t> singletons;

		for(const auto& x : expr)
				singletons[x.second] += x.first;

		for(const auto& rule : d)
				for(const auto& x : rule.second)
						singletons[x.second] += x.first;

		auto iter = singletons.begin();
		while(iter != singletons.end())  {
				if(iter->first >= 0 or iter->second > 1)
						iter = singletons.erase(iter);
				else
						iter++;
		}

		auto lift = [&](expression::iterator pos) -> expression::iterator {
				const symbol& s = pos->second;
				auto singleton = singletons.find(s);
				if(singleton != singletons.end()) {
						auto rule = d.find(s);
						const auto& x = rule->second;
						expr.insert(pos, x.begin(), x.end());
						pos = expr.erase(pos);
						d.erase(s);
				}
				return pos;
		};

		size_t ss = 0;

		for(auto pos = expr.begin(); pos != expr.end(); pos++)
				pos = lift(pos);

		for(auto& rule : d) {
				for(auto pos = rule.second.begin(); pos != rule.second.end(); pos++)
						pos = lift(pos);
				++ss += rule.second.size();
		}

		std::cerr << "dictionary entries: " << d.size() << std::endl;
		std::cerr << "dictionary size: " << ss << std::endl;
		std::cerr << "expression size: " << expr.size() << std::endl;

		d.expand(expr);

		for(auto x : expr)
				std::cout << (char)x.second;

		return false;
}

bool rz_compress(const config& cfg, int fdin, int fdout) {

		constexpr size_t block_sz = (1 << 20);

		char block[block_sz];

		ssize_t n;

		while((n = read_block(fdin, block, block_sz)) > 0)
				rz_compress_block(cfg, block, n, fdout);

		return (n != -1);
}

bool rz_decompress(const config&, int, int) {
		return false;
}

bool rz_process_fd(const config& cfg, int fdin, int fdout) {
		return cfg.compress ? rz_compress(cfg, fdin, fdout) : rz_decompress(cfg, fdin, fdout);
}

bool rz_process_file(const config& cfg, const char *filenamein) {

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

				fdout = dup(STDOUT_FILENO);

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

				const size_t extension_sz = strlen(rz_extension);
				const size_t filenamein_sz = strlen(filenamein);

				const char *p = filenamein + filenamein_sz;

				std::string filenameout(filenamein);

				if(extension_sz < filenamein_sz)
						p -= extension_sz;

				if(strcmp(p, rz_extension) == 0) {
						if(cfg.compress) {
								std::cerr << "already has " << rz_extension << " suffix -- ignored" << std::endl;
								return false;
						}
						filenameout.erase(filenameout.end() - extension_sz, filenameout.end());
				} else {
						if(not cfg.compress) {
								std::cerr << "unknown suffix -- ignored" << std::endl;
								return false;

						}
						filenameout += rz_extension;
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
				close(fdout);
				return false;
		}

		rz_process_fd(cfg, fdin, fdout);

		std::cerr << std::endl;

		close(fdin);
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

		if(cfg.stdoutput and not cfg.force) {
				std::cerr << "compressed data not written to a terminal. Use -f to force ";
				if(not cfg.compress)
						std::cerr << "de";
				std::cerr << "compression." << std::endl;
				return -1;
		}

		if(cfg.files.empty()) {

				rz_process_fd(cfg, STDIN_FILENO, STDOUT_FILENO);

		} else
				for(auto file : cfg.files)
						rz_process_file(cfg, file.c_str());

		return 0;
}
