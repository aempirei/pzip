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

#include <cstdlib>
#include <cstdio>

#include <string>
#include <list>

#include <unistd.h>

#include <pzip.hh>

struct config;

void usage(const char *);

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
		"\t-v\tbe verbose" << std::endl <<

		"If no file names are given, " << prog << " uses stdin/stdout." << std::endl << std::endl;
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

	for(int i = optind + 1; i <= argc; i++)
		cfg.files.push_back(argv[optind]);

	if(cfg.help) {
		usage(*argv);
		return 0;
	}

	for(auto file : cfg.files)
		std::cout << "handling file: " << file << std::endl;

	return 0;
}
