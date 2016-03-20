/*

atm2wav
By Charlie Robson

charlie_robson@hotmail.com
arduinonut.blogspot.com

Convert an .atm format file to a WAV playable back to a real Atom or similar.

This is not intended to be production quality code, there may well be one or more
of the following: Mistakes, misapprehensions, booboos, blunders or nasty hard-coding.

This is however to the best of my knowledge servicable code and may well provide
one or more of the following: Understanding, amusement, usefulness.

Writes 16 bit 44.1khz mono wav file.

Enjoy!


Information sources:

Atom technical manual:
http://acorn.chriswhy.co.uk/docs/Acorn/Manuals/Acorn_AtomTechnicalManual.pdf

Atomic theory and practice:
http://www.xs4all.nl/~fjkraan/comp/atom/atap/

Atom Rom disassembly:
http://www.stairwaytohell.com/atom/wouterras/romdisas.zip

*/

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <unistd.h>

#include "shared/argcrack.h"
#include "shared/defines.h"
#include "shared/atmheader.h"

#include <math.h>
#ifndef PI
#define PI 3.14159265358979323846
#endif

#define VERSION "1.1.0"

class freqout
{
public:

	/* From some old text or other (ATAP probably):

		A logic 0 consists of 4 cycles of a 1.2 kHz tone,
		and a logic 1 consists of 8 cycles of a 2.4 kHz tone.

		Each byte of data is preceeded by a logic zero start bit,
		and is terminated by a logic 1 stop bit.
	*/

	// 1 cycle of 1200hz = 36.75 samples @ 44100hz
	// 36.75 * 4 = 147 samples
	//
	// 1 cycle of 2400hz = 18.375 samples @ 44100hz
	// 18.375 * 8 = 147 samples

	// Bit position (0..7) to mask
	//
	inline BYTE _BV(int x) const
	{
		return (BYTE)(1 << x);
	}

	freqout(BYTE* rawdata, std::ofstream& out) :
		m_rawdata(rawdata),
		m_out(out),
		m_writtenSampleCount(0)
	{
		// Build the output table. 4 cycles into 147 bytes.
		//
		double val = 0;
		double step = (PI * 8.0) / 147.0;

		for (int i = 0; i < 147; ++i)
		{
			double s = sin(val) * 16384.0;

			// Sinusoidal data doesn't look to good at this audio resolution,
			// so square it off.
			//
			if (s >= 0.0)
			{
				s = -16384;
			}
			else
			{
				s = 16384;
			}

			m_sine[i] = short(s);
			val += step;
		}
	}

	// Output a 1 bit, 8 cycles of 24khz
	//
	void out1(void)
	{
		for (int i = 0; i < 147; ++i)
		{
			// Double step the table for a higher frequency
			//
			short val = m_sine[(i * 2) % 147];
			BYTE lo = (val >> 8) & 255;
			BYTE hi = val & 255;
			m_out.put(hi);
			m_out.put(lo);
		}
		m_writtenSampleCount += 147;
	}

	// Output a 0 bit, 4 cycles of 12khz
	//
	void out0(void)
	{
		for (int i = 0; i < 147; ++i)
		{
			short val = m_sine[i];
			BYTE lo = (val >> 8) & 255;
			BYTE hi = val & 255;
			m_out.put(hi);
			m_out.put(lo);
		}
		m_writtenSampleCount += 147;
	}

	// Output a byte plus its surrounding start & stop bit.
	// Add the byte's value to the rolling checksum.
	//
	void outByte(BYTE value)
	{
		out0();
		for (int i = 0; i < 8; ++i)
		{
			if (value & _BV(i))
			{
				out1();
			}
			else
			{
				out0();
			}
		}
		out1();

		m_checksum += value;
	}


	// Write atom formatted data to the wave file.
	//
	bool write(bool shortheaders)
	{
		atmheader atm;
		atm.read(m_rawdata);

		BYTE* dataEnd = m_rawdata + atm.header.length;

		int blockNum = 0;
		int blockLoadAddr = atm.header.start;

		// Bit 7 = last block. Clear = last block.
		// Bit 6 = do load me. Set = load. Clear = don't.
		// Bit 5 = first block. Clear = first block.
		//
		BYTE flags = _BV(7) | _BV(6);

		// Initial header time about 4 1/2 seconds. One out() call puts ~3.3ms of data.
		//
		float headerTime = float(shortheaders ? 2500 : 4550);

		while (flags & _BV(7))
		{
			while(headerTime > 0.0)
			{
				out1();
				headerTime -= 3.3F;
			}

			m_checksum = 0;

			// Preamble
			//
			outByte('*');
			outByte('*');
			outByte('*');
			outByte('*');

			// Header max 14 chars, including terminator.
			//
			for (int i = 0; i < 14 && atm.header.filename[i]; ++i)
			{
				outByte(atm.header.filename[i]);
			}
			outByte(0x0d);

			int blockLen = int(dataEnd - m_rawdata);
			if (blockLen < 257)
			{
				// flags.7 cleared to indicate last block
				//
				flags &= ~_BV(7);
			}
			else if (blockLen > 256)
			{
				// Blocks are 256 bytes max.
				//
				blockLen = 256;
			}

			/* - Header format:
			<*>                      )
			<*>                      )
			<*>                      )
			<*>                      ) Header preamble
			<Filename>               ) Name is 1 to 13 bytes long
			<Status Flag>            ) Bit 7 clear if last block
			                         ) Bit 6 clear to skip block
			                         ) Bit 5 clear if first block
			<MSB block number>       ) Always zero
			<LSB block number>
			<Bytes in block>
			<MSB run address>
			<LSB run address>
			<MSB block load address>
			<LSB block load address>
			*/

			outByte(flags);
			outByte(0);
			outByte(blockNum & 0xff);
			outByte(blockLen-1);
			outByte((atm.header.exec & 0xff00) >> 8);
			outByte(atm.header.exec & 0xff);
			outByte((blockLoadAddr & 0xff00) >> 8);
			outByte(blockLoadAddr & 0xff);

			// Header/data gap is about 1 second. One out() call puts ~3.3ms of data.
			//
			headerTime = float(shortheaders ? 500 : 1000);
			while(headerTime > 0.0)
			{
				out1();
				headerTime -= 3.3F;
			}

			// Now for the data. You know you want it.
			//
			for (int i = 0; i < blockLen; ++i)
			{
				outByte(m_rawdata[i]);
			}

			// The checksum itself gets added to the internal count, but not
			// until after it's written. It's reset again at the start of the
			// loop. So go ahead, corrupt it all you like.
			//
			outByte(m_checksum);

			blockLoadAddr+= 0x100;
			m_rawdata += 0x100;
			++blockNum;

			// flags.5 is clear on first block
			//
			flags |= _BV(5);

			// Reset inter-block header time ready for leader tone at start of loop.
			//
			headerTime = float(shortheaders ? 500 : 1000);
		}

		return true;
	}




	// Write atom formatted data to the wave file - unnamed mode
	//
	bool writeunnamed(bool shortheaders)
	{
		atmheader atm;
		atm.read(m_rawdata);

		BYTE* dataEnd = m_rawdata + atm.header.length;

		int blockLoadAddr = atm.header.start;
		int blockEndAddr = blockLoadAddr + atm.header.length;

		// Initial header time about 4 1/2 seconds. One out() call puts ~3.3ms of data.
		//
		float headerTime = float(shortheaders ? 2500 : 4550);
		while(headerTime > 0.0)
		{
			out1();
			headerTime -= 3.3F;
		}

		outByte(blockEndAddr / 256);
		outByte(blockEndAddr % 256);
		outByte(blockLoadAddr / 256);
		outByte(blockLoadAddr % 256);

		for (int i = 0; i < atm.header.length; ++i)
		{
			outByte(m_rawdata[i]);
		}

		return true;
	}


	short m_sine[147];

	BYTE* m_rawdata;

	int m_writtenSampleCount;

	BYTE m_checksum;

	std::ofstream& m_out;
};




// NASTY HACK FOR WRITING WAVS. PLEASE LOOK AWAY NOW.

static const int ENDIAN0 = 0;
static const int ENDIAN1 = 1;
static const int ENDIAN2 = 2;
static const int ENDIAN3 = 3;

static const int ENDIAN0_S = 0;
static const int ENDIAN1_S = 1;

static const int CHANNELS = 1;
static const int SAMPLERATE = 44100;
static const int BITSPERSAMPLE = 16;

void CreateWaveFile(std::ostream& output)
{
	char chars[4];

	output << "RIFF";
	*(int*)chars = 16;
	output << chars[0] << chars[1] << chars[2] << chars[3];

	output << "WAVE";
	output << "fmt ";

	*(int*)chars = 16;
	output << chars[ENDIAN0] << chars[ENDIAN1] << chars[ENDIAN2] << chars[ENDIAN3];

	*(short*)chars = 1;
	output << chars[ENDIAN0_S] << chars[ENDIAN1_S];

	*(short*)chars = CHANNELS;
	output << chars[ENDIAN0_S] << chars[ENDIAN1_S];

	*(int*)chars = SAMPLERATE;
	output << chars[ENDIAN0] << chars[ENDIAN1] << chars[ENDIAN2] << chars[ENDIAN3];

	*(int*)chars = (int)(SAMPLERATE*(BITSPERSAMPLE/8*CHANNELS));
	output << chars[ENDIAN0] << chars[ENDIAN1] << chars[ENDIAN2] << chars[ENDIAN3];

	*(short*)chars = (short)(BITSPERSAMPLE/8*CHANNELS);
	output << chars[ENDIAN0_S] << chars[ENDIAN1_S];

	*(short*)chars = (short)BITSPERSAMPLE;
	output << chars[ENDIAN0_S] << chars[ENDIAN1_S];

	output << "data";
	*(int*)chars = 0;
	output << chars[0] << chars[1] << chars[2] << chars[3];
}


void FinaliseWaveFile(std::ostream& output, size_t bytesWrit)
{
	char chars[4];

	output.seekp(40);

	*(int*)chars = (int)bytesWrit;
	output << chars[ENDIAN0] << chars[ENDIAN1] << chars[ENDIAN2] << chars[ENDIAN3];

	output.seekp(4);

	*(int*)chars = (int)(bytesWrit + 36);
	output << chars[ENDIAN0] << chars[ENDIAN1] << chars[ENDIAN2] << chars[ENDIAN3];
}

// OK IT'S SAFE TO LOOK AGAIN





int main(int argc, char** argv)
{
	argcrack param(argc, argv);

	if (argc < 2 || param.ispresent("/?") || param.ispresent("-?") || param.ispresent("?"))
	{
		std::cout << "ATM2WAV V" << VERSION << std::endl;
		std::cout << std::endl;
		std::cout << "Produces a WAV representing a cassette image of the supplied ATM." << std::endl;
		std::cout << "WAV will be 44.1khz, 16 bit, mono." << std::endl;
		std::cout << std::endl;
		std::cout << "Usage: atm2wav atmfile[.atm] [options]" << std::endl;
		std::cerr << std::endl;
		std::cerr << "Options:";
		std::cerr << std::endl;
		std::cerr << "out=     Output filename. Optional, defaults to <infile>.wav" << std::endl;
		std::cerr << "unnamed  Save as unnamed file." << std::endl;
		std::cout << "short    Short headers - 3 sec. instead of 5, reduced inter-block gap." << std::endl;
		return 1;
	}

	// todo: add support for unnamed files

	std::string inName = argv[1];
	std::ifstream in(inName.c_str(), std::ios_base::in | std::ios_base::binary);
	if (!in.is_open())
	{
		inName += ".atm";
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
		outName += ".wav";
	}

	// Prepare output.
	//
	std::ofstream out(outName.c_str(), std::ios_base::out);
	if (!out.is_open())
	{
		std::cout << "Invalid output file " << outName.c_str() << std::endl;
		return 1;
	}


	// Read the .atm into ram
	//
	in.seekg(0, std::ios_base::end);
	std::vector<BYTE> byteBuffer((unsigned int)(in.tellg()));
	in.seekg(0, std::ios_base::beg);

	BYTE* rawdata = &byteBuffer.front();
	in.read((char*)rawdata, (std::streamsize)byteBuffer.size());

	freqout now(rawdata, out);

	CreateWaveFile(out);

	bool written;
	if (param.ispresent("unnamed"))
	{
		written = now.writeunnamed(param.ispresent("short"));
	}
	else
	{
		written = now.write(param.ispresent("short"));
	}

	if (!written)
	{
		std::cout << "Failed to write WAV." << std::endl;
		unlink(outName.c_str());
		return 1;
	}

	FinaliseWaveFile(out, now.m_writtenSampleCount * 2);

	std::cout << "Written Atom program to '" << outName.c_str() << "'." << std::endl;

	return 0;
}
