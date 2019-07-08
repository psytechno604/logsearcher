// logsearcher.cpp : Defines the entry point for the console application.
//
#include "LogReader.h"
#include <stdio.h>
#include <conio.h>
#include <stdlib.h> 

#define BUF_SIZE 8192

#define print_line() printf("[line %d]: ", reader.LineNumber()); for(int i=0; i<bufsize && buf[i]!='\n'; i++) printf("%c", buf[i]); printf("\n");

int main(int argc, char* argv[])
{
	if (argc < 3) {
		printf("Usage: logsearcher.exe <filename> <mask> [options]\n-f           - full output,\n-b <bufsize> - set buffer size (default=%d).\n", BUF_SIZE);
		return 0;
	}
	CLogReader reader;

	

	if (reader.Open(argv[1])) {
		reader.SetFilter(argv[2]);

		bool fullOutput = false;
		for (int i = 3; i < argc; i++) {
			if (!strcmp(argv[i], "-f")) {
				fullOutput = true;
			}
		}

		int bufsize = BUF_SIZE;
		for (int i = 3; i < argc; i++) {
			if (!strcmp(argv[i], "-b") && i < argc - 1) {
				bufsize = atoi(argv[i + 1]);
			}
		}
		char* buf = new char[bufsize];

		int numFound = 0;
		while (reader.GetNextLine(buf, bufsize)) {
			numFound++;
			if (fullOutput) {
				print_line();
			}
		}
		printf("Found %d from total of %d lines\n", numFound, reader.LineNumber());

		reader.Close();

		delete[] buf;
	}
	return 0;
}

