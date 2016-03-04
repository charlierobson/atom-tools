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



int main(int argc, char** argv)
{
	argcrack param(argc, argv);

	if (argc < 3 || param.ispresent("/?") || param.ispresent("-?") || param.ispresent("?"))
	{
		std::cout << "HEADERPAD V" << VERSION << std::endl;
		std::cout << std::endl;
		std::cout << "Inflate, deflate or remove a header from an ATM format file." << std::endl;
		std::cout << std::endl;
		std::cout << "Usage: headerpad infile outfile [options]" << std::endl;
		std::cout << std::endl;
		std::cout << "Options:" << std::endl;
		std::cout << std::endl;
		std::cout << "I  Inflate a small header (default)" << std::endl;
		std::cout << "D  Deflate a large header" << std::endl;
		std::cout << "R  Remove header completely" << std::endl;
		return 1;
	}

   int action = 0;
   if (param.ispresent("I"))
   {
      ++action;
   }
   if (param.ispresent("D"))
   {
      ++action;
   }
   if (param.ispresent("R"))
   {
      ++action;
   }

   if (action > 1)
   {
		std::cout << "Make your mind up!" << std::endl;
		return 1;
   }

	std::string inName = argv[1];
	std::ifstream in(inName.c_str(), std::ios_base::in | std::ios_base::binary);
	if (!in.is_open())
	{
		std::cout << "Invalid input file " << argv[1] << "." << std::endl;
		return 1;
	}

	std::string outName = argv[2];
	std::ofstream out(outName.c_str(), std::ios_base::out | std::ios_base::binary);
	if (!out.is_open())
	{
		std::cout << "Invalid output file " << argv[2] << "." << std::endl;
		return 1;
	}


	in.seekg(0, std::ios_base::end);
	std::vector<BYTE> byteBuffer(in.tellg());
	in.seekg(0, std::ios_base::beg);

	BYTE* rawdata = &byteBuffer.front();
	in.read((char*)rawdata, std::streamsize(byteBuffer.size()));

	atmheader atm;
	atm.read(rawdata);

   if (param.ispresent("D"))
   {
	   atm.islarge = false;
	   atm.write(out);
	   out.write((const char*)rawdata, std::streamsize(atm.header.length));
   }
   else if (param.ispresent("R"))
   {
	   out.write((const char*)rawdata, std::streamsize(atm.header.length));
   }
   else
   {
	   atm.islarge = true;
	   atm.write(out);
	   out.write((const char*)rawdata, std::streamsize(atm.header.length));
   }

   return 0;
}
