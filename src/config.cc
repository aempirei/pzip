#include <sstream>
#include <iostream>

extern "C" {
#include <unistd.h>
}

#include <config.hh>

void config::usage(const char *prog) const {

	auto option = [](char ch, const char *msg) -> std::string {
		std::stringstream ss;
		ss << '\t' << '-' << ch << '\t' << msg << std::endl;
		return ss.str();
	};

	std::cerr << prog << " file compressor." << std::endl << std::endl <<

		"usage: " << prog << " [{option|file}]..." << std::endl << std::endl <<

		option('h',"print this message") <<
		option('d',"force decompression") <<
		option('z',"force compression") <<
		option('k',"keep (don't delete) input files") <<
		option('f',"overwrite existing output files") <<
		option('c',"output to standard out") <<
		option('q',"suppress output messages") <<
		option('0',"no compression") <<
		option('0' + level, "default compression") <<
		option('1',"fastest compression") <<
		option('9',"best compression") <<
		option('v',"be verbose") <<

		std::endl <<

		"If no file names are given, " << prog << " uses stdin/stdout." << std::endl << std::endl;
}

bool config::getopt(int argc, char **argv) {

	int opt;

	while ((opt = ::getopt(argc, argv, "hdzkfcqv0123456789")) != -1) {

		if(isdigit(opt)) {

			level = opt - '0';

		} else switch (opt) {

			case 'h': help      = true  ; break;
			case 'd': compress  = false ; break;
			case 'z': compress  = true  ; break;
			case 'k': keep      = true  ; break;
			case 'f': force     = true  ; break;
			case 'c': stdoutput = true  ; break;
			case 'q': quiet     = true  ; break;
			case 'v': verbose   = true  ; break;

			default : return false;
		}
	}

	for(int i = optind; i < argc; i++)
		files.push_back(argv[i]);

	if(files.empty())
		stdoutput = true;

	return true;
}
