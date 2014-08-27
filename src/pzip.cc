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

struct config {

	bool help = false;
	bool decompress = false;
	bool compress = false;
	bool keep = false;
	bool force = false;
	bool std = false;
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

bool pz_process_file(const config& cfg, const char *filename) {

	int fd;
	struct stat sb;

	std::cout << std::setw(12) << filename << ": ";

	if(stat(filename, &sb) == -1) {
		std::cout << strerror(errno) << std::endl;
		return false;
	}

	if(S_ISDIR(sb.st_mode)) {
		std::cout << "ignoring directory" << std::endl;
		return false;
	}

	fd = open(filename, O_RDONLY);
	if(fd == -1) {
		std::cout << strerror(errno) << std::endl;
		return false;
	}

	if(not cfg.compress and not cfg.decompress) {
		// check filename for .pz extension.
	}

	std::cout << std::endl;

	close(fd);
	return true;
}

int main(int argc, char **argv) {

	int opt;

	config cfg;

	while ((opt = getopt(argc, argv, "hdzkfcqv")) != -1) {

		switch (opt) {

			case 'h': cfg.help       = true; break;
			case 'd': cfg.decompress = true; break;
			case 'z': cfg.compress   = true; break;
			case 'k': cfg.keep       = true; break;
			case 'f': cfg.force      = true; break;
			case 'c': cfg.std        = true; break;
			case 'q': cfg.quiet      = true; break;
			case 'v': cfg.verbose    = true; break;

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

	for(auto file : cfg.files) {
		pz_process_file(cfg, file.c_str());
	}

	return 0;
}
