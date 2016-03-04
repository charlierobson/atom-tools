/*

dsk2atm
By Charlie Robson

charlie_robson@hotmail.com
arduinonut.blogspot.com

*/

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>

#include <direct.h>

#include <assert.h>

#include "shared\argcrack.h"
#include "shared\defines.h"
#include "shared\atmheader.h"


#define VERSION "1.0.0"


int main(int argc, char** argv)
{
	argcrack param(argc, argv);

	if (argc < 2 || param.ispresent("/?") || param.ispresent("-?") || param.ispresent("?"))
	{
		std::cout << "DSK2ATM V" << VERSION << std::endl;
		std::cout << std::endl;
		std::cout << "Produces ATM format files from an atom 40 track disk image." << std::endl;
		std::cout << std::endl;
		std::cout << "Usage: dsk2atm diskfile[.dsk] [options]" << std::endl;
		std::cerr << std::endl;
		std::cerr << "Options:" << std::endl;
		std::cerr << std::endl;
		std::cerr << "mkdir  - put the disk content in a directory named after .dsk file" << std::endl;
		std::cerr << std::endl;
		return 1;
	}

   std::string inName = argv[1];
	std::ifstream in(inName.c_str(), std::ios_base::in | std::ios_base::binary);
	if (!in.is_open())
	{
		inName += ".dsk";
		in.open(inName.c_str(), std::ios_base::in | std::ios_base::binary);
		if (!in.is_open())
		{
			std::cout << "Invalid input file " << argv[1] << "." << std::endl;
			return 1;
		}
	}


   std::string outDir(".");
   if (param.ispresent("mkdir"))
   {
      outDir = inName;
      size_t pos = outDir.find_first_of(".");
      if (pos != std::string::npos)
      {
         outDir.erase(pos);
      }
   }


   in.seekg(0, std::ios_base::end);
   long n = long(in.tellg());
	std::vector<BYTE> byteBuffer(n);
	in.seekg(0, std::ios_base::beg);

	BYTE* rawdata = &byteBuffer.front();
   in.read((char*)rawdata, (std::streamsize)byteBuffer.size());

   int numDirEnts = rawdata[0x105] / 8;

   for (int i = 0; i < numDirEnts; ++i)
   {
      BYTE* base = rawdata + (i*8) + 8;
      if ((base[7] & 0x20) != 0x20)
      {
         continue;
      }

      unsigned short* infoBaseS = (unsigned short*)(rawdata + 256 + (i*8) + 8);
      BYTE* infoBaseB = rawdata + 256 + (i*8) + 14;

      atmheader atm;
      atm.islarge = false;
      atm.header.start = infoBaseS[0];
      atm.header.exec = infoBaseS[1];
      atm.header.length = infoBaseS[2];

      unsigned short sector = infoBaseB[0] * 256 + infoBaseB[1];

      std::string outName((const char*)base);
      outName.erase(7);
      size_t pos = outName.find_first_of(" ");
      if (pos != std::string::npos)
      {
         outName.erase(pos);
      }

      memset(atm.header.filename, 0, 16);
      strcpy_s(atm.header.filename, 16, outName.c_str());

      char fnbuff[256];

      if (outDir != ".")
      {
         _mkdir(outDir.c_str());
      }

      if (outName.back() == '.')
      {
         outName.pop_back();
      }

      //sprintf_s(fnbuff, 256, "%s\\%s.atm", outDir.c_str(), outName.c_str());
      sprintf_s(fnbuff, 256, "%s\\%s", outDir.c_str(), outName.c_str());

      std::ofstream out(fnbuff, std::ios_base::out | std::ios_base::binary);
	   if (out)
	   {
		   atm.write(out);
		   out.write((const char*)rawdata+(0x100*sector), std::streamsize(atm.header.length));
		   std::cout << "Written Atom program '" << outName << "'." << std::endl;
         std::cout << std::hex << atm.header.start << "  " << atm.header.exec << "  " << atm.header.length <<  std::endl;
	   }
	   else
	   {
		   std::cout << "Couldn't write output file: " << outName.c_str() << "." << std::endl;
	   }
   }

	return 0;
}
