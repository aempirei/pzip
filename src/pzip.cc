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
struct symbol;

template <typename T> using meta = std::pair<bool,T>;

struct symbol {
	using type = int;
	const static type wildcard = -1;
};

using sequence = std::vector<symbol::type>;
using histogram = std::map<sequence,long>;
using dictionary = std::map<symbol::type,sequence>;

const char *pz_extension = ".pz";

void usage(const char *);

bool pz_process_file(const config&, const char *);
bool pz_process_fd(const config&, int, int);

bool pz_compress(const config&, int, int);
bool pz_decompress(const config&, int, int);

bool pz_match_symbol(const symbol::type, const symbol::type);

template <class T> T pz_match_sequence(typename T::const_iterator, typename T::const_iterator, const sequence&);
template <class T> T pz_find_sequence(const T&, const sequence&, const typename T::const_iterator);

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

bool pz_match_symbol(const symbol::type a, const symbol::type b) {
	return a == b or a == symbol::wildcard or b == symbol::wildcard;
}

template <class T> T pz_match_sequence(typename T::const_iterator start, typename T::const_iterator end, const sequence& seq) {

	auto iter = seq.begin();
	auto jter = start;

	while(iter != seq.end())
		if(jter == end or not pz_match_symbol(*iter++, *jter++))
			return end;
	
	return start;
}

template <class T> T pz_find_sequence(const T& block, const sequence& seq, const typename T::const_iterator start = block.begin()) {

	for(auto iter = start; iter != block.end(); iter++)
		if(pz_match_sequence(iter, block.end(), seq))
			return iter;

	return block.end();
}

bool pz_process_fd(const config&, int fdin, int fdout) {

	std::list<symbol::type> block;

	unsigned char buf[1024];
	int n;

	do {
		n = read(fdin, buf, sizeof(buf));
		if(n == -1) {
			if(errno == EAGAIN)
				continue;
			std::cerr << strerror(errno) << std::endl;
			return false;
		}

		for(int i = 0; i < n; i++)
			block.push_back((symbol::type)buf[i]);

	} while(n != 0);

	histogram h;

	for(auto iter = block.begin(); next(iter) != block.end(); iter++) {

		sequence key;

		key.push_back(*iter);
		key.push_back(*next(iter));

		h[key]++;
	}

	auto iter = std::max_element(
		h.begin(),
		h.end(),
		[] (histogram::value_type a, histogram::value_type b) -> bool { return b.second > a.second; }
	);

	int current_symbol = 256;

	const sequence& current_sequence = iter->first;

	dictionary d;

	if(iter->second > 1)
		d[current_symbol] = current_sequence;
	else
		throw std::runtime_error("nothing to compress");

	// compress

	auto position = pz_find_sequence(block.begin(), block.end(), current_sequence);

	if(position != block.end()) {
		std::cerr << "found sequence at position " << std::distance(block.begin(), position) << std::endl;
	}

	current_symbol++;
	
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
