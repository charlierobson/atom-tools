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

#include "shared/argcrack.h"
#include "shared/defines.h"
#include "shared/atmheader.h"

#include <math.h>
#ifndef PI
#define PI 3.14159265358979323846
#endif

#define VSNSTR "1.2.0"

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
		_rawData(rawdata),
		_out(out),
		_writtenSampleCount(0)
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

			_sine[i] = short(s);
			val += step;
		}

		_is8bit = false;
		BITSPERSAMPLE = 16;
	}

	// Output a 1 bit, 8 cycles of 24khz
	//
	virtual void out1(void)
	{
		for (int i = 0; i < 147; ++i)
		{
			// Double step the table for a higher frequency
			//
			short val = _sine[(i * 2) % 147];
			BYTE lo = (val >> 8) & 255;
			BYTE hi = val & 255;
			_out.put(hi);
			_out.put(lo);
		}
		_writtenSampleCount += 147;
	}

	// Output a 0 bit, 4 cycles of 12khz
	//
	virtual void out0(void)
	{
		for (int i = 0; i < 147; ++i)
		{
			short val = _sine[i];
			BYTE lo = (val >> 8) & 255;
			BYTE hi = val & 255;
			_out.put(hi);
			_out.put(lo);
		}
		_writtenSampleCount += 147;
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

		_checksum += value;
	}


	// Write atom formatted data to the wave file.
	//
	bool write(bool shortheaders)
	{
		atmheader atm;
		atm.read(_rawData);

		BYTE* dataEnd = _rawData + atm.header.length;

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

			_checksum = 0;

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

			int blockLen = int(dataEnd - _rawData);
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
				outByte(_rawData[i]);
			}

			// The checksum itself gets added to the internal count, but not
			// until after it's written. It's reset again at the start of the
			// loop. So go ahead, corrupt it all you like.
			//
			outByte(_checksum);

			blockLoadAddr+= 0x100;
			_rawData += 0x100;
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
		atm.read(_rawData);

		BYTE* dataEnd = _rawData + atm.header.length;

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
			outByte(_rawData[i]);
		}

		return true;
	}


	// NASTY HACK FOR WRITING WAVS. PLEASE LOOK AWAY NOW.

	void out32(int val)
	{
		char chars[4];
		*(int*)chars = val;
		_out << chars[0] << chars[1] << chars[2] << chars[3];
	}

	void out16(short val)
	{
		char chars[2];
		*(short*)chars = val;
		_out << chars[0] << chars[1];
	}

	void createWaveFile()
	{
		// chunk descriptor
		_out << "RIFF";
		out32(0);			// dummy chunk size, will be overwritten
		_out << "WAVE";

		// sub chunk descriptor
		int BYTERATE = SAMPLERATE*(BITSPERSAMPLE / 8 * CHANNELS);
		short BLOCKALIGN = BITSPERSAMPLE / 8 * CHANNELS;
		_out << "fmt ";
		out32(16);			// chunk size, bytes
		out16(1);			// format, pcm uncompressed
		out16(CHANNELS);
		out32(SAMPLERATE);
		out32(BYTERATE);
		out16(BLOCKALIGN);		// block align
		out16(BITSPERSAMPLE);

		// chunk descriptor
		_out << "data";
		out32(0);			// dummy block length, will be overwritten
	}


	void finaliseWaveFile()
	{
		_out.seekp(40);
		int bytesWrit = _writtenSampleCount * (_is8bit ? 1 : 2);
		out32(bytesWrit);

		_out.seekp(4);
		out32(bytesWrit + 36);
	}

	// OK IT'S SAFE TO LOOK AGAIN

	bool _is8bit;

	short _sine[147];

	BYTE* _rawData;

	int _writtenSampleCount;

	BYTE _checksum;

	std::ofstream& _out;

	static const int CHANNELS = 1;
	static const int SAMPLERATE = 44100;
	int BITSPERSAMPLE;
};


class freqout8 : public freqout
{
public:
	freqout8(BYTE* rawdata, std::ofstream& out) :
		freqout(rawdata, out)
	{
		_is8bit = true;
		BITSPERSAMPLE = 8;
	}

	// Output a 1 bit, 8 cycles of 24khz
	//
	virtual void out1(void)
	{
		for (int i = 0; i < 147; ++i)
		{
			// Double step the table for a higher frequency
			//
			short val = _sine[(i * 2) % 147];
			_out.put(val < 0 ? (unsigned char)0x40 : (unsigned char)0xc0);
		}
		_writtenSampleCount += 147;
	}

	// Output a 0 bit, 4 cycles of 12khz
	//
	virtual void out0(void)
	{
		for (int i = 0; i < 147; ++i)
		{
			short val = _sine[i];
			_out.put(val < 0 ? (unsigned char)0x40 : (unsigned char)0xc0);
		}
		_writtenSampleCount += 147;
	}
};





int main(int argc, char** argv)
{
	argcrack param(argc, argv);

	if (argc < 2 || param.ispresent("/?") || param.ispresent("-?") || param.ispresent("?"))
	{
		std::cout << "ATM2WAV V" << VSNSTR << std::endl;
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
		std::cerr << "8bit     Save as 8 bit unsigned WAV." << std::endl;
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

	freqout* fo = param.ispresent("8bit") ? new freqout8(rawdata, out) : new freqout(rawdata, out);
	fo->createWaveFile();

	bool written;
	if (param.ispresent("unnamed"))
	{
		written = fo->writeunnamed(param.ispresent("short"));
	}
	else
	{
		written = fo->write(param.ispresent("short"));
	}

	if (!written)
	{
		std::cout << "Failed to write WAV." << std::endl;
		_unlink(outName.c_str());
		return 1;
	}

	fo->finaliseWaveFile();

	std::cout << "Written Atom program to '" << outName.c_str() << "'." << std::endl;

	return 0;
}
