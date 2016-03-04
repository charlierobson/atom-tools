/*

bin2atm
By Charlie Robson

charlie_robson@hotmail.com
arduinonut.blogspot.com

Read an atom data or machinecode program and convert it to an .ATM format file.

The load address must be specified. decimal, octal, hex or binary is allowable,
decimal numbers start with a non-zero character. Octal have a preceeding 0. Hex
is specified in the c standard 0x form. Preceed a value with % to have it
interpreted as binary.

The execution address defaults to the load address unless otherwise specified.

The name placed in the ATM header can be passed in or left to be calculated
automagically from the input name.

The atommc MMC card system is made simpler internally by having 512 byte ATM headers.
This can be enabled with the 'pad' switch. The header can be tested for by examining
location 24,25. Finding $51,$2b ('512b') will indicate that this probably has a 512
byte header.

*/

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>

#include <assert.h>

#include "shared\argcrack.h"
#include "shared\defines.h"
#include "shared\atmheader.h"


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
		std::cout << "BIN2ATM V" << VERSION << std::endl;
		std::cout << std::endl;
		std::cout << "Produces an ATM format file from a binary file containing data." << std::endl;
		std::cout << std::endl;
		std::cout << "Usage: bin2atm binaryfile[.bin] load=<address> [options]" << std::endl;
		std::cout << std::endl;
		std::cout << "out=   Output file name. Optional, defaults to <infile>.atm." << std::endl;
		std::cout << "load=  Load address. Mandatory." << std::endl;
		std::cout << "exec=  Execution address. Optional, defaults to load address." << std::endl;
		std::cout << "name=  Specify atom format name. Optional, built from <infile>." << std::endl;
		std::cout << "pad    Create a file with 512 byte header, ready for ATOMMC." << std::endl;
		return 1;
	}

	std::string inName = argv[1];
	std::ifstream in(inName.c_str(), std::ios_base::in | std::ios_base::binary);
	if (!in.is_open())
	{
		inName += ".bin";
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

	int loadAddr;
	if (!param.getint("load", loadAddr))
	{
		std::cout << "Mandatory option not set: Load address." << std::endl;
		return 1;
	}

	int execAddr;
	if (!param.getint("exec", loadAddr))
	{
		execAddr = loadAddr;
		std::cout << "Defaulting execution address to load address." << std::endl;
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
		else
		{
			std::transform(progName.begin(), progName.end(), progName.begin(), toupper);
		}
	}

	in.seekg(0, std::ios_base::end);
	std::vector<BYTE> byteBuffer((unsigned int)in.tellg());
	in.seekg(0, std::ios_base::beg);

	BYTE* rawdata = &byteBuffer.front();
	in.read((char*)rawdata, (std::streamsize)byteBuffer.size());




	atmheader atm;

	strcpy_s(atm.header.filename, 16, progName.c_str());
	atm.header.exec = execAddr;
	atm.header.start = loadAddr;
	atm.header.length = WORD(byteBuffer.size());

	atm.islarge = param.ispresent("pad");

	std::ofstream out(outName.c_str(), std::ios_base::out | std::ios_base::binary);
	if (out)
	{
		atm.write(out);
		out.write((const char*)rawdata, std::streamsize(atm.header.length));
		std::cout << "Written Atom program '" << progName << "' as " << outName.c_str() << "." << std::endl;
	}
	else
	{
		std::cout << "Couldn't write output file: " << outName.c_str() << "." << std::endl;
	}

	return 0;
}
