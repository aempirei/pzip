// 
// pzip
// file compression tool
//
// Copyright(c) 2014 256 LLC
// Written by Christopher Abad
// 20 GOTO 10 
//

#include <iomanip>
#include <iostream>

#include <cstring>
#include <cstdlib>
#include <cstdio>

#include <string>
#include <list>
#include <map>

extern "C" {
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
}

#include <libpz.hh>

struct config;

void usage(const char *);

bool pz_process_file(const config&, const char *);
bool pz_process_fd(const config&, int, int);

bool pz_compress(const config&, int, int);
bool pz_decompress(const config&, int, int);

const char *pz_extension = ".pz";

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

	std::cout << prog << " file compressor." << std::endl << std::endl <<

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

bool pz_process_fd(const config& cfg, int fdin, int fdout) {
}

bool pz_process_file(const config& cfg, const char *filenamein) {

	int fdin;
	int fdout;
	struct stat sb;

	std::cout << std::setw(12) << filenamein << ": ";

	if(lstat(filenamein, &sb) == -1) {
		std::cout << strerror(errno) << std::endl;
		return false;
	}

	if(S_ISDIR(sb.st_mode)) {
		std::cout << "ignoring directory" << std::endl;
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
			std::cout << "ignoring " << get_file_type(sb) << std::endl;
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
				std::cout << "already has " << pz_extension << " suffix -- ignored" << std::endl;
				return false;
			}
			filenameout.erase(filenameout.end() - extension_sz, filenameout.end());
		} else {
			if(not cfg.compress) {
				std::cout << "unknown suffix -- ignored" << std::endl;
				return false;

			}
			filenameout += pz_extension;
		}

		std::cout << "=> " << filenameout;

		fdout = open(filenameout.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0664);
		if(fdout == -1) {
			std::cout << strerror(errno) << std::endl;
			return false;
		}
	}

	fdin = open(filenamein, O_RDONLY);
	if(fdin == -1) {
		std::cout << strerror(errno) << std::endl;
		close(fdout);
		return false;
	}

	pz_process_fd(cfg, fdin, fdout);

	std::cout << std::endl;

	close(fdin);
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
				  std::cout << "Try `" << *argv << " -h' for more information." << std::endl;
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
