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

const char *rz_extension = ".rz";

bool rz_process_file(const config&, const char *);
bool rz_process_fd(const config&, int, int);

bool rz_compress(const config&, int, int);
bool rz_decompress(const config&, int, int);

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

bool rz_compress(const config&, int, int) {
		return false;
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

		if(cfg.files.empty())
				rz_process_fd(cfg, STDIN_FILENO, STDOUT_FILENO);

		else
				for(auto file : cfg.files)
						rz_process_file(cfg, file.c_str());

		return 0;
}
