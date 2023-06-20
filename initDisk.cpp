#include <iostream>
#include <string>
#include <fstream>

const short SECTOR_SIZE = 4096;
const short TOTAL_SECTORS = 256; //~ 1 MB
const short ENTRY_SIZE = 28;

unsigned char driveBuffer[SECTOR_SIZE * TOTAL_SECTORS];

void initDrive(std::string diskName)
{
    std::ofstream disk;
    for(int sector = 0; sector < TOTAL_SECTORS; ++sector)
    {
        int entryPos = 0;
        for(int byte = 0; byte < SECTOR_SIZE; ++byte)
        {
            if(sector == 0)
            {
                if(entryPos % ENTRY_SIZE == 0)
                    driveBuffer[byte] = 0xFF;

                else
                    driveBuffer[byte] = '@';
            }

            else if(sector == 1)
            {
                //if(entryPos % ENTRY_SIZE == 0)
                //    driveBuffer[byte] = 0xFF;

                //else
                    driveBuffer[byte + (sector * SECTOR_SIZE)] = '@';
            }

            else
                driveBuffer[byte + (sector * SECTOR_SIZE)] = '!';

            ++entryPos;
        }
    }

    disk.open(diskName, std::ofstream::binary);
    for(int i = 0; i < TOTAL_SECTORS * SECTOR_SIZE; ++i)
        disk << driveBuffer[i];
    disk.close();
}

int main()
{
    initDrive("test.img");
}