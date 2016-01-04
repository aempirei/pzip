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
struct histogram;

using symbol = int32_t;
using expression = std::list<term>;
using rdictionary = std::map<expression, symbol>;

using term_baseclass = std::pair<size_t, symbol>;
using dictionary_baseclass = std::map<symbol, expression>;
using histogram_baseclass = std::map<expression, size_t>;

using rule = dictionary_baseclass::value_type;

struct term : term_baseclass {
		using term_baseclass::term_baseclass;
		term(const symbol&);
};

term::term(const symbol& x) : term(1,x) {
}

struct dictionary : dictionary_baseclass {
		using dictionary_baseclass::dictionary_baseclass;
		expression& expand(expression&) const;
};

struct histogram : histogram_baseclass {
		using histogram_baseclass::histogram_baseclass;
};

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

		do {

				histogram h;

				last_sz = d.size();

				std::cerr << std::endl << "d: " << d.size() << " r: " << r.size() << " e: " << expr.size();

				for(auto pos = expr.begin(); std::next(pos) != expr.end(); pos++) {

						if(pos->second == std::next(pos)->second) {
								std::next(pos)->first += pos->first;
								pos = std::prev(expr.erase(pos));
						} else {
								h[expression(pos, std::next(pos,2))]++;
						}
				}

				for(auto iter = h.cbegin(); iter != h.cend(); iter++) {

						const auto& x = iter->first;

						if(iter->second > 1) { // and r.find(x) == r.end()) {
								symbol s = -(d.size() + 256); 
								d[s] = x;
								r[x] = s;
						}
				}

				for(auto pos = expr.begin(); std::next(pos) != expr.end(); pos++) {

						auto rrule = r.find(expression(pos, std::next(pos,2)));

						if(rrule != r.end()) {
								expr.insert(pos, rrule->second);
								pos = std::prev(expr.erase(pos, std::next(pos,2)));
						}
				}

		} while(d.size() > last_sz);

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
