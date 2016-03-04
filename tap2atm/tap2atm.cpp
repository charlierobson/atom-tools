/*

tap2atm
By Charlie Robson

charlie_robson@hotmail.com
arduinonut.blogspot.com

Splits a .tap file into its constituent .atm files.

Not much use in itself as I think the only incidence of these files
is to be found in the now pretty historical archives of Wouter Ras:

http://www.stairwaytohell.com/atom/wouterras

*/


#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>

#include <direct.h>

#include "shared\argcrack.h"
#include "shared\defines.h"


#define VERSION "1.1.0"


int main(int argc, char** argv)
{
	argcrack param(argc, argv);

	if (argc < 2 || param.ispresent("/?") || param.ispresent("-?") || param.ispresent("?"))
	{
		std::cerr << std::endl;
		std::cout << "TAP2ATM V" << VERSION << std::endl;
		std::cout << std::endl;
		std::cout << "Splits a .TAP file into its constituent .ATM files." << std::endl;
		std::cout << std::endl;
		std::cout << "Usage: tap2atm tapfile[.tap] [options]" << std::endl;
		std::cerr << std::endl;
		std::cerr << "Options:" << std::endl;
		std::cerr << std::endl;
		std::cerr << "mkdir    - Place .atm files in a directory named after the tap file." << std::endl;
		std::cerr << "detailed - Output filename with extra information." << std::endl;
		std::cerr << std::endl;
		return 1;
	}

	std::string inName = argv[1];
	std::ifstream in(inName.c_str(), std::ios_base::in | std::ios_base::binary);
	if (!in.is_open())
	{
		inName += ".tap";
		in.open(inName.c_str(), std::ios_base::in | std::ios_base::binary);
		if (!in.is_open())
		{
			std::cerr << "Invalid input file " << argv[1] << "." << std::endl;
			return 1;
		}
	}

	in.seekg(0, std::ios_base::end);
	std::vector<char> byteBuffer(long(in.tellg()));
	in.seekg(0, std::ios_base::beg);

	size_t index = 0, size = byteBuffer.size();

	char* buffer = &byteBuffer.front();
	in.read(buffer, (std::streamsize)size);

	int n = 0;

	while (index < size)
	{
		ATMHEADER* header = (ATMHEADER*)&buffer[index];

		// Atom filename may have control chars in. Preserve these in the binary
		//
		std::string atmName(header->filename);
		for(size_t i = 0; i < atmName.length(); ++i)
		{
			if (atmName[i] < 32)
			{
				atmName[i] = '-';
			}
		}

		std::stringstream destName;
		if (param.ispresent("detailed"))
		{
			destName << argv[1] << "." << n << "." << atmName; // << ".atm";
		}
		else
		{
			destName << atmName; // << ".atm";
		}

		int blocksize = header->length + sizeof(ATMHEADER);


      std::string outDir(".");
      if (param.ispresent("mkdir"))
      {
         outDir = inName;
         size_t pos = outDir.find_last_of(".");
         if (pos != std::string::npos)
         {
            outDir.erase(pos);
         }
      }

      std::transform(outDir.begin(), outDir.end(), outDir.begin(), ::tolower);

      char fnbuff[256];
      bool numberFiles = param.ispresent("number");

      if (outDir != ".")
      {
         _mkdir(outDir.c_str());
      }


      sprintf_s(fnbuff, 256, "%s\\%s", outDir.c_str(), destName.str().c_str());

      std::ofstream out(fnbuff, std::ios_base::out | std::ios_base::binary);
		if (out.is_open())
		{
			out.write((const char*)header, blocksize);

			std::cout << "Written " << destName.str() << std::endl;
		}
		else
		{
			std::cout << "Invalid output file: " << destName.str() << std::endl;
		}

		index += header->length + sizeof(ATMHEADER);
		++n;
	}
}