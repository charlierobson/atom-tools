#ifndef __atmheader_h
#define __atmheader_h

#pragma pack(push,1)

typedef struct
{
	char filename[16];
	WORD start;
	WORD exec;
	WORD length;
	// BYTE data[];
}
ATMHEADER;

#pragma pack(pop)
// Beware the cheesy hardcoded sector sizes. but there's stuff predicated on this
// the whole place over. so I don't care ;)

class atmheader
{
public:
	ATMHEADER header;

	atmheader()
	{
		memset(this, 0, sizeof(atmheader));
	}

	bool read(BYTE*& data)
	{
		ATMHEADER* atm = (ATMHEADER*)data;

		header.exec = atm->exec;
		memcpy(&header.filename, atm->filename, 16);
		header.length = atm->length;
		header.start = atm->start;

		data += sizeof(ATMHEADER);

		return true;
	}

   size_t size(void)
	{
		return sizeof(ATMHEADER);
	}

   bool write(std::ofstream& out)
	{
		out.write((const char*)&header, std::streamsize(size()));

		// todo return status of write
		return true;
	}
};

#endif
