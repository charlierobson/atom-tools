/*

bas2atm
By Charlie Robson

charlie_robson@hotmail.com
arduinonut.blogspot.com

Read an AtomBASIC program from an ansi text file and convert it to an .ATM format file.

The program can be presented without linenumbers, specifying auto on the command line
will insert them. Atom labels are very useful when doing this. Labels can be specified
either as lower-case letters, or prefixed with a circumflex ('hat') character. Why have
the alternate form? Well AtomBASIC can be very hard on the eyes with it's shouty upper-
case appearance. To this end I added an option to do the case conversion on the fly.
'upper' specified on the command line will do this.

To enable adding profuse commentary to the source program you can write text preceeded
by '~~'. This will be snipped during conversion.

The atommc MMC card system is made simpler internally by having 512 byte ATM headers.
This can be enabled with the 'pad' switch.

*/

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>

#include <assert.h>

#include "..\..\shared\argcrack.h"
#include "..\..\shared\defines.h"
#include "..\..\shared\atmheader.h"

#define VERSION "1.1.0"


bool pc2atom(const char* inName, std::string& outName)
{
	std::string in(inName);
	size_t slashpos = in.find_last_of("\\/");
	if (slashpos != std::string::npos)
	{
		in = in.substr(slashpos + 1);
	}

	size_t dotpos = in.find_first_of(".");
	if (dotpos != std::string::npos)
	{
		in = in.substr(0, dotpos);
	}

	// internal program name without path or extension
	//
	outName = in;
	if (outName.length() > 13)
	{
		outName = outName.substr(0, 13);
	}
	std::transform(outName.begin(), outName.end(), outName.begin(), toupper);

	return true;
}



int main(int argc, char** argv)
{
	argcrack param(argc, argv);

	if (argc < 2 || param.ispresent("/?") || param.ispresent("-?") || param.ispresent("?"))
	{
		std::cout << "BAS2ATM V" << VERSION << std::endl;
		std::cout << std::endl;
		std::cout << "Produces an ATM format file from a text file containing" << std::endl;
		std::cout << "Atom basic. Text file should be ansi format." << std::endl;
		std::cout << std::endl;
		std::cout << "Usage: bas2atm textfile[.txt] [options]" << std::endl;
		std::cout << std::endl;
		std::cout << "Options:" << std::endl;
		std::cout << std::endl;
		std::cout << "out=   Output file name. Optional, defaults to {infile}.atm." << std::endl;
		std::cout << "auto   Add line numbers. Optional." << std::endl;
		std::cout << "upper  Convert source to uppercase on the fly. Optional." << std::endl;
		std::cout << "name=  Specify atom format name. Optional, built from <infile>." << std::endl;
		std::cout << "load=  Load address. Optional, defaults to #2900" << std::endl;
		std::cout << "exec=  Exec address. Optional, defaults to #C2B2" << std::endl;
		std::cout << "pad    Use 512 byte header ready for ATOMMC. Optional." << std::endl;
		std::cout << std::endl;
		std::cout << "When upper-casing programs that are also auto-numbered, you can use" << std::endl;
		std::cout << "GOTO/SUB with labels specified by preceeding the character with '^'." << std::endl;
		std::cout << "Data following a double-tilde ('~~') is discarded, which is good for" << std::endl;
		std::cout << "adding 'soft' comments to the source which take no room in the Atom." << std::endl;
		return 1;
	}


	std::string inName = argv[1];
	std::ifstream in(inName.c_str(), std::ios_base::in | std::ios_base::binary);
	if (!in.is_open())
	{
		inName += ".txt";
		in.open(inName.c_str(), std::ios_base::in | std::ios_base::binary);
		if (!in.is_open())
		{
			std::cout << "Invalid input file " << argv[1] << "." << std::endl;
			return 1;
		}
	}


	std::string outName;
	if (!param.getstring("out", outName))
	{
		outName = inName;
		outName += ".atm";
	}


	// Generate Atom style program name from input, if not already specified
	//
	std::string progName;
	if (!param.getstring("name", progName))
	{
		if (!pc2atom(outName.c_str(), progName))
		{
			std::cout << "Error generating Atom program name from " << outName.c_str() << "." << std::endl;
			return 1;
		}
	}
	else
	{
		std::transform(progName.begin(), progName.end(), progName.begin(), toupper);
	}

	bool autoNumber = param.ispresent("auto");
	bool autoUpper = param.ispresent("upper");

	std::vector<BYTE> program;

	int lineNum = 10;
	int sourceLineNum = 0;

	char buffer[1024];

	while(in.getline(buffer, sizeof(buffer)))
	{
		++sourceLineNum;

		std::string line(buffer);

		// crop any soft comments
		//
		size_t pos = line.find("~~");
		if (pos != std::string::npos)
		{
			line.erase(pos);
		}

		// discard leading and trailing whitespace
		//
		line.erase(line.find_last_not_of(" \t\r\n")+1);
		line.erase(0, line.find_first_not_of(" \t\r\n"));

		if (line.length() == 0)
		{
			continue;
		}

		// Endpointer can be updated by the strtol for programs contining line numbers
		//
		char* chars = (char*)line.data();

		if (!autoNumber)
		{
			// Attempt to read a linenumber. No validation is done.
			//
			lineNum = strtol(chars, &chars, 10);
			if (chars == buffer)
			{
				std::cout << "Error parsing line number @ line " << sourceLineNum << ": '" << buffer << "'" << std::endl;
				return 1;
			}
		}

		program.push_back(0x0d);

		program.push_back(BYTE((lineNum & 0xff00) >> 8));
		program.push_back(BYTE(lineNum & 0xff));

		lineNum += 10;

		while(*chars)
		{
			int c = *chars;

			if (autoUpper)
			{
				c = toupper(c);
			}
			if (c == '^')
			{
				// ^X or ^x decodes to lower case x,
				// or inverse X in the case of the Atom. IOW a label.
				// There will always be 1 more byte, even if hat is the
				// last char in the line.
				//
				++chars;
				c = tolower(*chars);
			}

			program.push_back(BYTE(c));
			++chars;
		}
	}

	program.push_back(0x0d);
	program.push_back(0xff);

	atmheader atm;

	int loadAddr = 0x2900;
	param.getint("load", loadAddr);

	int execAddr = 0xc2b2;
	param.getint("exec", execAddr);
	
	strcpy_s(atm.header.filename, 16, progName.c_str());
	atm.header.exec = execAddr;
	atm.header.start = loadAddr;
	atm.header.length = WORD(program.size());

	atm.islarge = param.ispresent("pad");

	std::ofstream out(outName.c_str(), std::ios_base::out | std::ios_base::binary);
	if (out)
	{
		atm.write(out);
		out.write((const char*)&program.front(), std::streamsize(atm.header.length));
		std::cout << "Written Atom BASIC program '" << progName << "' as " << outName.c_str() << "." << std::endl;
	}
	else
	{
		std::cout << "Couldn't write output file: " << outName.c_str() << "." << std::endl;
	}

	return 0;
}