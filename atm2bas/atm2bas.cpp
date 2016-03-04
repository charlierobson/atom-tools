/*

atm2bas
By Charlie Robson

charlie_robson@hotmail.com
arduinonut.blogspot.com

Read an AtomBASIC program stored in an ATM file and export it to an ansi text file.

Many programs have extra bytes following the BASIC, and these will normally be dumped
as hex. This can be turned off by specifying the nodumpex flag.

I have encountered BASIC programs with a couple of different start addresses and
no doubt there may be more. I decided to abort conversion if an unrecognised start
address is found. This can be turned off by specifying nocheckex.

*/

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>

#include "shared\argcrack.h"
#include "shared\defines.h"
#include "shared\atmheader.h"

#define VERSION "1.1.2"


std::string hex(int val, int width)
{
	char fmtstring[16];
	sprintf_s(fmtstring, 16, "%%0%dX", width);

	char buffer[16];
	sprintf_s(buffer, 16, fmtstring, val);
	return std::string(buffer);
}



int main(int argc, char** argv)
{
	argcrack param(argc, argv);

	if (argc < 2 || param.ispresent("/?") || param.ispresent("-?") || param.ispresent("?"))
	{
		std::cerr << "ATM2BAS V" << VERSION << std::endl;
		std::cerr << std::endl;
		std::cerr << "Produces an ansi .txt file of the basic code stored in an ATM file." << std::endl;
		std::cerr << std::endl;
		std::cerr << "Usage: atm2bas atmfile[.atm] [options]" << std::endl;
		std::cerr << std::endl;
		std::cerr << "Options:";
		std::cerr << std::endl;
		std::cerr << "out=       Output filename. Optional, defaults to <infile>.txt" << std::endl;
		std::cerr << "nocheckex  Allow conversion even if exec. addr. != 0xc2b2 or 0xce86." << std::endl;
		std::cerr << "nodumpex   Discard bytes following BASIC." << std::endl;
		return 1;
	}

	std::string inName = argv[1];
	std::ifstream in(inName.c_str(), std::ios_base::in | std::ios_base::binary);
	if (!in.is_open())
	{
		inName += ".atm";
		in.open(inName.c_str(), std::ios_base::in | std::ios_base::binary);
		if (!in.is_open())
		{
			std::cerr << "Invalid input file " << argv[1] << "." << std::endl;
			return 1;
		}
	}


	std::string outName;
	if (!param.getstring("out", outName))
	{
		outName = inName;
		outName += ".txt";
	}

	// Prepare output.
	//
	std::ofstream out(outName.c_str(), std::ios_base::out);
	if (!out.is_open())
	{
		std::cerr << "Invalid output file " << outName.c_str() << std::endl;
		return 1;
	}

	// Read the .atm into ram.
	//
	in.seekg(0, std::ios_base::end);
	std::vector<BYTE> byteBuffer((unsigned int)in.tellg());
	in.seekg(0, std::ios_base::beg);

	BYTE* rawdata = &byteBuffer.front();
	in.read((char*)rawdata, (std::streamsize)byteBuffer.size());

	atmheader atm;
	atm.read(rawdata);

	std::cout << atm.header.filename << "     "
		<< " " << hex(int(atm.header.start), 4)
		<< " " << hex(int(atm.header.exec), 4)
		<< std::endl;

	// known execution addresses associated with BASIC programs.
	//
	if (!param.ispresent("nocheckex") && atm.header.exec != 0xc2b2 && atm.header.exec != 0xce86)
	{
		std::cerr << "Probably not a basic program." << std::endl;
		return 1;
	}

	BYTE* enddata = rawdata + atm.header.length; 

	int lastLineNum = -1;

	while(1)
	{
		if (*rawdata != 0x0d)
		{
			std::cerr << "No EOL found where there should have been one." << std::endl;
			return 1;
		}

		++rawdata;

		if (rawdata[0] > 127)
		{
			// all done - there may be data following however.
			//
			++rawdata;
			break;
		}

		int lineNum = rawdata[0] * 256 + rawdata[1];
		if (lineNum <= lastLineNum)
		{
			// Acornsoft programs often have a line 0 at the end of a program.
			//
			std::cerr << "Possible inconsistency in line numbering after line " << lastLineNum << "." << std::endl;
		}

		lastLineNum = lineNum;
		rawdata += 2;

		out << lineNum;

		while (*rawdata != 0x0d && rawdata != enddata)
		{
			out << *rawdata;
			++rawdata;
		}

		if (rawdata == enddata)
		{
			std::cerr << "Premature line ending found." << std::endl;
			return 1;
		}

		out << std::endl;
	}

	if (!param.ispresent("nodumpex") && rawdata != enddata)
	{
		out << "~~ Extra bytes";

		int outct = 0;
		while (rawdata != enddata)
		{
			if ((outct % 16) == 0)
			{
				out << std::endl << "~~";
			}

			out << hex(*rawdata, 2).c_str();
			++rawdata;
			++outct;
		}

		out << std::endl;
	}

	std::cerr << "Decoded Atom BASIC program to '" << outName.c_str() << "'." << std::endl;

	return 0;
}