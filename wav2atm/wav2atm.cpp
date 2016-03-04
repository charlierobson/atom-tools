/*

wav2atm
By Charlie Robson

charlie_robson@hotmail.com
arduinonut.blogspot.com

Read recording of an Atom program from a WAV file, create .ATM file.

This is not intended to be production quality code, there may well be one or more
of the following: Mistakes, misapprehensions, booboos, blunders or fluffups.

This is however to the best of my knowledge servicable code and may well provide
one or more of the following: Understanding, amusement, usefulness.


Information sources:

Atom technical manual:
http://acorn.chriswhy.co.uk/docs/Acorn/Manuals/Acorn_AtomTechnicalManual.pdf

Atomic theory and practice:
http://www.xs4all.nl/~fjkraan/comp/atom/atap/

Atom Rom disassembly:
http://www.stairwaytohell.com/atom/wouterras/romdisas.zip


*/

#define VERSION "1.1.0"


#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>

#include <math.h>

#include "shared\argcrack.h"
#include "shared\defines.h"
#include "shared\atmheader.h"
#include "shared\nameconv.h"



#define _BV(x) (1<<(x))
#define SGN(x) ((x)<0?-1:1)
#define SUPERCHEESYFUNC


// Set structure packing to byte boundaries
// ... else strange things are happening under vs2005.
//
// This may well be MS specific, adjust to taste.
//
#pragma pack(push, 1)

typedef struct
{
	char chunkid[4];
	DWORD chunkSize;
	char format[4];
}
RIFFHEADER;

typedef struct
{
	char chunkid[4];
	DWORD chunkSize;
	WORD foramtTag;
	WORD channels;
	DWORD samplesPerSec;
	DWORD avgBytesPerSec;
	WORD blockAlign;
	WORD bitsPerSample;
	// BYTE extradata [chunkSize-16]
}
FMTHEADER;

typedef struct
{
	char chunkid[4];
	DWORD chunkSize;
}
DATACHUNK;

struct 
{
	// Ordered as received from tape
	//
	BYTE flags;
	BYTE hiBlockNum, loBlockNum;
	BYTE bytesInBlockMinus1;
	BYTE hiRunAddress, loRunAddress;
	BYTE hiBlockLoadAddress, loBlockLoadAddress;
}
atomTapeHeader;

// Restore default structure packing
//
#pragma pack(pop)



SUPERCHEESYFUNC const char* torf(bool val)
{
	return val ? "Y" : "N";
}

SUPERCHEESYFUNC std::string hex(int val, int width)
{
	char fmtstring[16];
	sprintf_s(fmtstring, 16, "%%0%dX", width);

	char buffer[16];
	sprintf_s(buffer, 16, fmtstring, val);
	return std::string(buffer);
}




// And no, I didn't miss the obvious comical acronym ;)
//
typedef std::vector<short>::iterator IT;

class cuts
{
public:
	cuts(std::vector<short>& tape, int aspc) :
	  m_tape(tape),
		  m_aspc(aspc)
	  {
		  m_tapehead = m_tape.begin();
	  };

	  int m_aspc;

	  std::vector<short>& m_tape;

	  IT m_tapehead;


	  // Start here.
	  //
	  // Finds tone data in the stream. Should probably use some kind of filter,
	  //  but I'm not that smart. Plus simple is good, right?
	  //
	  // Advances tapehead to first sample of new cycle of header tone.
	  //
	  bool findLeader(void)
	  {
		  // Locate leader.
		  //
		  int cycles = 0;
		  while(cycles < 4096)
		  {
			  int count = 0;
			  if (!countSimilarSamples(count))
			  {
				  return false;
			  }

			  // If the count we see is within 5% of the expected value,
			  //  bump the cycle counter. The expected value is the number
			  //  of samples that represents one cycle at 2400hz, IOW a high tone.
			  //
			  int diff = (abs(count - (m_aspc/2)) * 100) / m_aspc;
			  if (diff < 6)
			  {
				  ++cycles;
			  }
			  else
			  {
				  cycles = 0;
			  }
		  }

		  // Now go on to find start bit! Fly little one! Be free!

		  return true;
	  }


	  // Counts the number of similarly-signed samples at the tapehead onward.
	  // Assumes tapehead is at 1st sample with a sign different to that of its
	  //  predecessor.
	  // Advances tapehead to 1st sample with new sign.
	  //
	  bool countSimilarSamples(int& count)
	  {
		  count = 0;

		  int hilo = SGN(*m_tapehead);
		  while (m_tapehead != m_tape.end() && SGN(*m_tapehead) == hilo)
		  {
			  ++m_tapehead;
			  ++count;
		  }

		  return m_tapehead != m_tape.end();
	  }


	  // Counts the number of samples forming one cycle.
	  // Assumes tapehead is at 1st sample of a new cycle.
	  // Advances tapehead to 1st sample of next cycle.
	  //
	  bool getCycleCount(int& count)
	  {
		  int loper, hiper;
		  if (!countSimilarSamples(loper))
		  {
			  return false;
		  }
		  if (!countSimilarSamples(hiper))
		  {
			  return false;
		  }

		  count = loper + hiper;
		  return true;
	  }


	  // Locates a start bit in the bitstream.
	  // Assumes tapehead is at first sample of a high cycle.
	  // Advances tapehead to the first sample of a startbit.
	  //
	  bool findStartBit(void)
	  {
		  int count;
		  IT cursor;

		  // Look for a cycle with a period greater than the average 
		  // samples per cycle at 2400hz.
		  //
		  // |-- m_aspc --|
		  // |-----||-----|
		  // | lo  ||  hi |
		  //
		  // at 1200hz we have
		  // |     lo     ||     hi     |
		  //                      ^
		  //                      1.5 * m_aspc
		  //
		  // So you can see that any sample count > (ofm_aspc * 1.5)
		  // must be a 1200hz = low tone = 0 bit.
		  //
		  do
		  {
			  cursor = m_tapehead;

			  if (!getCycleCount(count))
			  {
				  return false;
			  }
		  }
		  while (count < m_aspc * 3 / 2);

		  m_tapehead = cursor;
		  return true;
	  }


	  // Gets a bit. 0 or 1 dude.
	  // Assumes tapehead is at first sample of new bit.
	  // Advances tapehead to start of next bit.
	  //
	  bool getBit(bool& bit)
	  {
		  int count;
		  if (!getCycleCount(count))
		  {
			  return false;
		  }

		  // Reject the bit if we see a tone out of sequence.
		  //
		  if (count < m_aspc * 3 / 2)
		  {
			  // 8 cycles of 24khz. One down, 7 left in town.
			  //
			  bit = 1;
			  for (int i = 0; i < 7; ++i)
			  {
				  getCycleCount(count);
				  if (count > m_aspc * 3 / 2)
				  {
					  return false;
				  }
			  }
		  }
		  else
		  {
			  // 4 cycles of 12khz. One's been seen, we need three.
			  //
			  bit = 0;
			  for (int i = 0; i < 3; ++i)
			  {
				  getCycleCount(count);
				  if (count < m_aspc * 3 / 2)
				  {
					  return false;
				  }
			  }
		  }

		  return true;
	  }


	  // This should be pretty obvious.
	  // Assumes tapehead is at first sample of new tone.
	  // Advances tapehead to first sample after stop bit.
	  //
	  bool getByte(BYTE& byte)
	  {
		  // The tapehead can be at the start of a cycle of leader tone.
		  // This leader may be as short as one cycle!
		  // This is evident in fred.wav (recorded from a real Atom) which
		  // exhibits the curious phenomena of having 9 cycles of high tone
		  // in its stop bits. Treating this like a micro-leader allows the
		  // handling of leader to be generic.
		  //
		  if (!findStartBit())
		  {
			  return 1;
		  }

		  bool bit;
		  if (!getBit(bit) || bit)
		  {
			  return false;
		  }

		  byte = 0;
		  for (int i = 0; i < 8; ++i)
		  {
			  if (!getBit(bit))
			  {
				  return false;
			  }
			  if (bit)
			  {
				  byte |= _BV(i);
			  }
		  }

		  if (!getBit(bit) || !bit)
		  {
			  return false;
		  }

		  m_check += byte;

		  return true;
	  }

	  BYTE m_check;
};

// todo - add support for unnamed files.

int main(int argc, char** argv)
{
	argcrack param(argc, argv);

	if (argc < 2 || param.ispresent("/?") || param.ispresent("-?") || param.ispresent("?"))
	{
		std::cout << "WAV2ATM V" << VERSION << std::endl;
		std::cout << std::endl;
		std::cout << "Produces .ATM file image of an atom program in WAV form." << std::endl;
		std::cout << "WAVs should be 16 bit, mono. Programs should be BASIC, SAVEd" << std::endl;
		std::cout << std::endl;
		std::cout << "Usage: wav2atm wavfile[.wav] [options]" << std::endl;
		std::cout << std::endl;
		std::cout << "Options:" << std::endl;
		std::cout << std::endl;
		std::cout << "out=   Specify output name. Optional, defaults to <infile>.atm" << std::endl;
		return 1;
	}

	std::string inName = argv[1];
	std::ifstream in(inName.c_str(), std::ios_base::in | std::ios_base::binary);
	if (!in.is_open())
	{
		inName += ".wav";
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


	BYTE buffer[1024];
	in.read((char*)buffer, sizeof(RIFFHEADER));
	RIFFHEADER* riffhdr = (RIFFHEADER*)buffer;

	FMTHEADER fmthdr;
	in.read((char*)&fmthdr, sizeof(FMTHEADER));

	if (fmthdr.bitsPerSample != 16 || fmthdr.channels != 1)
	{
		std::cout << "Wav should be mono, 16 bit please." << std::endl;
		return 1;
	}

	int avgSamplesPerCycleAt2400hz = fmthdr.samplesPerSec / 2400;

	in.read((char*)buffer, sizeof(DATACHUNK));
	DATACHUNK* datachk = (DATACHUNK*)buffer;

	std::vector<short> databuffer(datachk->chunkSize);

	short* data = &databuffer.front();
	in.read((char*)data, (std::streamsize)databuffer.size() * sizeof(short));


	BYTE atomFname[14];

	atmheader atm;
	std::vector<BYTE> byteBuffer(0);

	cuts likeAKnife(databuffer, avgSamplesPerCycleAt2400hz);

	bool lastBlock = false;

	while(!lastBlock)
	{
		if (!likeAKnife.findLeader())
		{
			std::cout << "Didn't find leader tone." << std::endl;
			return 1;
		}

		if (!likeAKnife.findStartBit())
		{
			std::cout << "Didn't find start bit." << std::endl;
			return 1;
		}

		int i;
		BYTE byte;

		likeAKnife.m_check = 0;

		// Read header preamble: '****'
		//
		for (i = 0; i < 4; ++i)
		{
			if (!likeAKnife.getByte(byte) || byte != '*')
			{
				std::cout << "Failed reading preamble." << std::endl;
				return 1;
			}
		}

		// Now get the filename up to and includeing the 0x0d terminator.
		// Max size is 13 chars + terminator = 14.
		//
		i = -1;
		do
		{
			if (!likeAKnife.getByte(atomFname[++i]))
			{
				std::cout << "Failed reading filename." << std::endl;
				return 1;
			}
		}
		while(atomFname[i] != 0x0d && i != 13);
		atomFname[i] = 0x0;

		// Read header
		//
		BYTE* headBytes = (BYTE*)&atomTapeHeader;
		for (i = 0; i < 8; ++i)
		{
			if (!likeAKnife.getByte(headBytes[i]))
			{
				std::cout << "Failed reading header." << std::endl;
				return 1;
			}
		}

		// Courtesy calculations :)
		//
		bool firstBlock = (atomTapeHeader.flags & _BV(5)) == 0;
		bool doLoad = (atomTapeHeader.flags & _BV(6)) != 0;
		lastBlock = (atomTapeHeader.flags & _BV(7)) == 0;

		if (firstBlock)
		{
			memcpy_s(atm.header.filename, 16, atomFname, 14);
			atm.header.exec = atomTapeHeader.loRunAddress + 256 * atomTapeHeader.hiRunAddress;
			atm.header.start = atomTapeHeader.loBlockLoadAddress + 256 * atomTapeHeader.hiBlockLoadAddress;
			atm.header.length = 0;
		}

		atm.header.length += atomTapeHeader.bytesInBlockMinus1 + 1;

		size_t writeOffs = byteBuffer.size();
		byteBuffer.resize(writeOffs + atm.header.length);

		BYTE* data = &byteBuffer.front();
		data += writeOffs;

		// Read data block
		//
		for (i = 0; i < atomTapeHeader.bytesInBlockMinus1 + 1; ++i)
		{
			if (!likeAKnife.getByte(data[i]))
			{
				std::cout << "Failed reading data block." << std::endl;
				return 1;
			}
		}

		// Check some checksum
		//
		BYTE sum, expected = likeAKnife.m_check;
		if (!likeAKnife.getByte(sum))
		{
			std::cout << "Failed reading checksum byte." << std::endl;
			return 1;
		}

		if (sum != expected)
		{
			std::cout << "SUM" << std::endl;
			return 1;
		}
	}

	std::cout << atomFname << "     "
		<< " " << hex(int(atomTapeHeader.loBlockLoadAddress) + 256 * int(atomTapeHeader.hiBlockLoadAddress), 4)
		<< " " << hex(int(atomTapeHeader.loRunAddress) + 256 * int(atomTapeHeader.hiRunAddress), 4)
		<< " " << hex(int(atomTapeHeader.loBlockNum), 4)
		<< " " << hex(atomTapeHeader.bytesInBlockMinus1, 2)
		<< std::endl;

	std::cout << ">";


	std::ofstream out(outName.c_str(), std::ios_base::out | std::ios_base::binary);
	if (out)
	{
		atm.write(out);
		out.write((const char*)&byteBuffer.front(), std::streamsize(atm.header.length));
		std::cout << "Written ATM '" << outName.c_str() << "'." << std::endl;
	}
	else
	{
		std::cout << "Couldn't write output file: " << outName.c_str() << "." << std::endl;
	}


	return 0;
}
