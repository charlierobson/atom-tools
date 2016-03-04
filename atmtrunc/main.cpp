#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("Usage: truncatm [filename]\n");
        exit(1);
    }

    HANDLE file = CreateFile(&argv[1][0], GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    if (INVALID_HANDLE_VALUE == file)
    {
        printf("Couldn't open file.\n");
        exit(1);
    }

    DWORD actual;
    unsigned char data[22];
    if (ReadFile(file, data, 22, &actual, NULL))
    {
        unsigned short requiredSize = *(unsigned short*)(&data[20]);
        DWORD pos = SetFilePointer(file, (long)requiredSize, 0, 1);

        if (INVALID_SET_FILE_POINTER == pos)
        {
            printf("Couldn't set file pointer.\n", requiredSize);
        }
        else
        {
            if (!SetEndOfFile(file))
            {
                printf("Truncation failed.\n", requiredSize);
            }
            else
            {
                printf("Truncated to %d (#%04x) bytes.\n", requiredSize, requiredSize);
            }
        }
    }
    else
    {
        printf("Couldn't read file.\n");
    }

    CloseHandle(file);
}
