#include <iostream>
#include <string>
#include <vector>
#include <fstream>

const short SECTOR_SIZE = 4096;
const short TOTAL_SECTORS = 256; //~ 1 MB
const short ENTRY_SIZE = 28;
const short MAX_FILES = 146;
const short MAX_DIRECTORIES = 256;
const short DIR_ENTRY_SIZE = 14;

const short START_OF_DATA_SECTION = 0x2000;

const unsigned short TOTAL_BLOCK_SIZE = 256;
const unsigned char BLOCK_SIZE_DATA = 250;
const short TOTAL_BLOCKS = 4096;

unsigned char currentDirId = 0x0;

unsigned char driveBuffer[SECTOR_SIZE * TOTAL_SECTORS];
std::vector <unsigned char> fileDataBuffer;

void loadDiskBuffer(std::string diskName)
{
    std::ifstream disk;
    disk.open(diskName);
    for(int i = 0; i < SECTOR_SIZE * TOTAL_SECTORS; ++i)
        disk >> std::noskipws >> driveBuffer[i];
}

int bytesRemaining()
{
    int totalBytesFree = 0;
    //for(int i = 2; i < TOTAL_SECTORS; ++i)
    for(int i = 0; i < TOTAL_BLOCKS; ++i)
    {
        if(driveBuffer[0x2000 + (i * TOTAL_BLOCK_SIZE)] == '!') //is free?
            totalBytesFree += TOTAL_BLOCK_SIZE;
    }

    return totalBytesFree;
}

int countFileBlocks(int dataAddress)
{
    int blocks = 0;
    while(driveBuffer[dataAddress + 1] != '$') //end of file
    {
        ++blocks;
        dataAddress += 256;
    }
    return blocks;
}

void dir(unsigned char folderId)
{
    int numberOfFiles = 0;
    for(int i = 0; i < MAX_FILES - 1; ++i)
    {
        char fileNameBuff[11];
        if(driveBuffer[i * ENTRY_SIZE] != 0xFF) //scan entries
        {
            if(driveBuffer[i * ENTRY_SIZE + 19] == folderId)
            {
                ++numberOfFiles;
                int byte = 0;
                for(int j = i * ENTRY_SIZE; j < 11; ++j)
                    fileNameBuff[byte++] = driveBuffer[j];

                //print file name
                int pos = 0;
                for(int j = 10; j >= 0; --j)
                {
                    if(driveBuffer[j + ENTRY_SIZE * i] != ' ')
                    {
                        pos = j;
                        break;
                    }
                }
                std::cout << "pos: " << pos << std::endl;
                for(int j = 0; j <= pos; ++j)
                    std::cout << driveBuffer[j + ENTRY_SIZE * i];

                //print file ext.
                std::cout << '.' << driveBuffer[i * ENTRY_SIZE + 11];
                std::cout << driveBuffer[i * ENTRY_SIZE + 12] << driveBuffer[i * ENTRY_SIZE + 13];

                int fileStartAddr = (int)(driveBuffer[(i * ENTRY_SIZE) + 17] << 16) | 
                (int)(driveBuffer[(i * ENTRY_SIZE) + 18] << 8) |
                (int)(driveBuffer[(i * ENTRY_SIZE) + 19]);

                std::cout << "\t" << (countFileBlocks(fileStartAddr) * 250) / 1024 << "KB";

                std::cout << '\t' << (int)driveBuffer[14 + i * ENTRY_SIZE] << '-' <<
                (int)driveBuffer[15 + i * ENTRY_SIZE] << '-' << (int)driveBuffer[16 + i * ENTRY_SIZE];
                std::cout << std::endl;
            }
        }
    }

    for(int i = 0; i < MAX_DIRECTORIES; ++i)
    {
        std::string dirName;
        if(driveBuffer[0x1000 + (i * DIR_ENTRY_SIZE)] == '*') //scan used entries
        {
            if(driveBuffer[0x1000 + (i * DIR_ENTRY_SIZE) + 13] == folderId)
            {
                for(int j = 0; j < 11; ++j)
                    dirName += driveBuffer[0x1000 + (i * DIR_ENTRY_SIZE) + 1 + j];

                std::cout << dirName << "   (DIRECTORY)\n";
            }
        }   
    }

    std::cout << "\n\n" << numberOfFiles << " file(s) found\n";

    int totalBytes = bytesRemaining();
    if(totalBytes > 1500)
        std::cout << totalBytes / 1000 << "KB remaining on disk\n\n\n";
    
    else
        std::cout << totalBytes << "Bytes remaining on disk\n\n\n";
}

int findBlankEntry()
{
    int entryIndx = 0;
    bool entryFound = false;
    for(int i = 0; i < MAX_FILES - 1; ++i)
    {
        if(driveBuffer[i * ENTRY_SIZE] == 0xFF) //scan entries
        {
            entryFound = true;
            entryIndx = i;
            break;
        }
    }

    if(entryFound)
    {
        return entryIndx;
    }

    else
        std::cout << "\nError: No space left in entry table!\n";

    return -1; //error
}

void writeEntry(int firstBlockAddress, int index, std::string fileName, std::string extName, unsigned char day, unsigned char month, unsigned char year, unsigned char folderId)
{
    //write file name
    for(int i = 0; i < 11; ++i)
    {
        if(i < fileName.size())
            driveBuffer[i + (index * ENTRY_SIZE)] = fileName.at(i);

        else //add padding if it doesn't use all 11 bytes
            driveBuffer[i + (index * ENTRY_SIZE)] = ' ';
    }

    //write ext
    for(int i = 0; i < 3; ++i)
        driveBuffer[i + 11 + (index * ENTRY_SIZE)] = extName.at(i);

    driveBuffer[14 + index * ENTRY_SIZE] = day;
    driveBuffer[15 + index * ENTRY_SIZE] = month;
    driveBuffer[16 + index * ENTRY_SIZE] = year;

    unsigned char blockAddrByte3 = (firstBlockAddress >> 24) & 0xFF; //MSB
    unsigned char blockAddrByte2 = (firstBlockAddress >> 16) & 0xFF;
    unsigned char blockAddrByte1 = (firstBlockAddress >> 8) & 0xFF;
    unsigned char blockAddrByte0 = firstBlockAddress & 0xFF; //LSB

    driveBuffer[17 + index * ENTRY_SIZE] = blockAddrByte2;
    driveBuffer[18 + index * ENTRY_SIZE] = blockAddrByte1;
    driveBuffer[19 + index * ENTRY_SIZE] = blockAddrByte0;
    driveBuffer[20 + index * ENTRY_SIZE] = folderId;
}

void saveToDiskFile(std::string diskName)
{
    std::ofstream disk;
    disk.open(diskName, std::ofstream::binary);

    for(int i = 0; i < TOTAL_SECTORS * SECTOR_SIZE; ++i)
        disk << driveBuffer[i];

    disk.close();
}

int findFreeSector()
{
    int blockNumberAddr = 0;
    for(int i = 0; i < TOTAL_BLOCKS; ++i)
    {
        if(driveBuffer[START_OF_DATA_SECTION + i * TOTAL_BLOCK_SIZE] == '!') //search the begining of each file block for the first blank
        {
            blockNumberAddr = START_OF_DATA_SECTION + (i * TOTAL_BLOCK_SIZE);
            return blockNumberAddr;
        }
    }

    //if it gets here there is no free space
    return -1;
}

int findFreeSector(int skip)
{
    int blockNumberAddr = 0;
    for(int i = 0; i < TOTAL_BLOCKS; ++i) //there are 240 total blocks available (240 * 256 = 15 sectors)
    {
        if(driveBuffer[START_OF_DATA_SECTION + i * TOTAL_BLOCK_SIZE] == '!') //search the begining of each file block for the first blank
        {
            if(skip) //skip this block
            {
                --skip;
                continue;
            }

            blockNumberAddr = START_OF_DATA_SECTION + (i * TOTAL_BLOCK_SIZE);
            return blockNumberAddr;
        }
    }

    //if it gets here there is no free space
    return -1;
}

void getFileData(std::string filePath)
{
    std::ifstream file;

    int size = 0;
    file.open(filePath, std::ios::binary);
    file.seekg(0, std::ios::end);
    size = file.tellg();
    file.seekg(0, std::ios::beg);

    fileDataBuffer.clear();
    for(int i = 0; i < size; ++i)
    {
        unsigned char byte;
        file >> std::noskipws >> byte;
        fileDataBuffer.push_back(byte);
    }

    file.close();
}

int attachFileData(std::string filePath)
{
    int firstBlockAddress = 0;

    int lastBlockSize = 0;

    getFileData(filePath); //get file data contents into buffer
    int blocksNeeded = 0;

    blocksNeeded = fileDataBuffer.size() / BLOCK_SIZE_DATA; //actual space in block is 250 not 256 because of header

    //check if data does not fit perfectly into block spacing, if not will use a partial of next block
    if(fileDataBuffer.size() - ((fileDataBuffer.size() / BLOCK_SIZE_DATA) * BLOCK_SIZE_DATA))
        ++blocksNeeded;
    lastBlockSize = fileDataBuffer.size() - ((fileDataBuffer.size() / BLOCK_SIZE_DATA) * BLOCK_SIZE_DATA);

    //write data to buffer in block data format
    for(int block = 0; block < blocksNeeded; ++block)
    {
        std::vector <unsigned char> tmp;
        int nextFree = findFreeSector();
        if(nextFree == -1) //not enough space
        {
            return -1;
        }

        unsigned char usedByte;
        unsigned char linkByte;
        unsigned char nextSectorByte[3];
        unsigned char lengthByte;
        bool lastBlock = false;

        //Beginning of block header
		//1 byte '!' Free, 0xAA first block of file, or 0x0 contiguous file block
		//1 byte if linked '&' linked block, or '$' single block file
		//	($ = if whole file fits in one block or end of linked block file)
		//3 bytes next sector
		//1 byte length of data in block 
		//	(in bytes, e.g. completely filled block = 0xFA)

        if(blocksNeeded > 1)
        {
            if(block == 0) //first block
            {
                usedByte = 0xAA;
                firstBlockAddress = nextFree;
            }

            else //all other blocks
                usedByte = 0x0;

            //last block
            if(block == blocksNeeded - 1)
            {
                linkByte = '$';

                //last block so just fill zero
                for(int i = 0; i < 3; ++i)
                    nextSectorByte[i] = 0x0;

                lengthByte = lastBlockSize;
                lastBlock = true;
            }

            else //not last block
            {
                linkByte = '&';
                int blockAddress = findFreeSector(1); //get the address of the next free block
                nextSectorByte[0] = (blockAddress >> 16) & 0xFF;
                nextSectorByte[1] = (blockAddress >> 8) & 0xFF;
                nextSectorByte[2] = blockAddress & 0xFF;
                lengthByte = 0xFA; //not the last block so block will be full FA=250 size
                lastBlock = false;
            }

            tmp.push_back(usedByte);
            tmp.push_back(linkByte);
            tmp.push_back(nextSectorByte[0]);
            tmp.push_back(nextSectorByte[1]);
            tmp.push_back(nextSectorByte[2]);
            tmp.push_back(lengthByte);

            if(lastBlock)
            {
                for(int byte = 0; byte < lastBlockSize; ++byte)
                    tmp.push_back(fileDataBuffer.at((block * BLOCK_SIZE_DATA) + byte));
            }

            else
            {
                for(int byte = 0; byte < BLOCK_SIZE_DATA; ++byte)
                    tmp.push_back(fileDataBuffer[byte] = fileDataBuffer.at((block * BLOCK_SIZE_DATA) + byte));
            }

            //commit tmp buffer to main drive buffer
            for(int i = 0; i < tmp.size(); ++i)
                driveBuffer[nextFree + i] = tmp.at(i);
        }

        //file fits in one block
        else
        {
            usedByte = 0xAA; //only block so just set start byte, no conditional
            firstBlockAddress = nextFree;

            //fits in signle block so will always be last block
            linkByte = '$';

            //again there is no next sector so just make all zeros
            for(int i = 0; i < 3; ++i)
                nextSectorByte[i] = 0x0;

            lengthByte = lastBlockSize;
            lastBlock = true;

            tmp.push_back(usedByte);
            tmp.push_back(linkByte);
            tmp.push_back(nextSectorByte[0]);
            tmp.push_back(nextSectorByte[1]);
            tmp.push_back(nextSectorByte[2]);
            tmp.push_back(lengthByte);

            for(int byte = 0; byte < lastBlockSize; ++byte)
                tmp.push_back(fileDataBuffer.at((block * BLOCK_SIZE_DATA) + byte));
                //fileDataBuffer[byte] = fileDataBuffer.at((block * 250) + byte);

            //commit tmp buffer to main drive buffer
            for(int i = 0; i < tmp.size(); ++i)
                driveBuffer[nextFree + i] = tmp.at(i);
        }
    }

    //save address of start of file sector block to file table entry

    return firstBlockAddress; //successful, so return address
}

void writeFile(std::string diskName)
{
    int entryIndx = findBlankEntry();

    std::cout << "Enter file name: ";
    std::string fileName;
    std::cin >> fileName;

    std::cin.ignore();
    std::cout << "Enter ext: ";
    std::string extName;
    std::cin >> extName;

    std::cin.ignore();
    std::cout << "Enter path to file data: ";
    std::string fileDataPath;
    std::cin >> fileDataPath;

    int firstBlockAddress = attachFileData(fileDataPath);
    if(firstBlockAddress == -1)
    {
        std::cout << "Error: Not enough space available!\n";
        return;
    }

    //if(alreadyExits(fileName, extName))
    //{
    //    std::cout << "Already exists\n";
    //    return;
    //}

    unsigned char folderId = currentDirId; //root

    unsigned char day, month, year;
    day = 30;
    month = 6;
    year = 22;

    writeEntry(firstBlockAddress, entryIndx, fileName, extName, day, month, year, folderId);
    saveToDiskFile(diskName);
}

void extractFile(unsigned char folderId)
{
    std::string fileName;
    std::string extName;

    std::cout << "Enter file name: ";
    std::cin >> fileName;
    while(fileName.size() > 11)
        std::cin >> fileName;

    std::cout << "\nEnter file extension: ";
    std::cin >> extName;
    while(extName.size() > 3)
        std::cin >> extName;

    //add padding to file name and ext
    int sz = 11 - fileName.size();
    for(int i = 0; i < sz; ++i)
        fileName += " ";
    sz = 3 - extName.size();
    for(int i = 0; i < 3 - extName.size(); ++i)
        extName += " ";

    std::string fileNameTmp;
    std::string extTmp;
    int fileIndex;
    for(int i = 0; i < MAX_FILES - 1; ++i)
    {
        fileNameTmp.clear();
        extTmp.clear();
        fileIndex = i;
        if(driveBuffer[i * ENTRY_SIZE] != 0xFF) //scan entries
        {
            for(int j = 0; j < 11; ++j)
                fileNameTmp += driveBuffer[j + ENTRY_SIZE * i];

            for(int j = 0; j < 3; ++j)
                extTmp += driveBuffer[(11 + j) + (ENTRY_SIZE * i)];
        }

        //find matching file
        unsigned int fileStartAddr = 0;
        std::vector <unsigned char> dataBuffer;
        if(std::strcmp(fileName.c_str(), fileNameTmp.c_str()) == 0)
        {
            //with matching extension
            if(std::strcmp(extName.c_str(), extTmp.c_str()) == 0 && currentDirId == driveBuffer[(fileIndex * ENTRY_SIZE) + 19])
            {
                if(driveBuffer[i * ENTRY_SIZE + 19] == folderId) //exists in dir?
                {
                    //std::cout << "File found\n";
                    //std::cout << "Index " << fileIndex << std::endl;

                    fileStartAddr = (int)(driveBuffer[(fileIndex * ENTRY_SIZE) + 17] << 16) | 
                    (int)(driveBuffer[(fileIndex * ENTRY_SIZE) + 18] << 8) |
                    (int)(driveBuffer[(fileIndex * ENTRY_SIZE) + 19]);

                    //meta data
                    unsigned char usedByte = driveBuffer[fileStartAddr + i];
                    unsigned char linkedByte; 
                    unsigned char addrBytes[3];
                    unsigned char lenByte;
        
                    do
                    {
                        for(int i = 0; i < TOTAL_BLOCK_SIZE; ++i)
                        {
                            if(i < 6)
                            {
                                switch(i)
                                {
                                    case 0:
                                        usedByte = driveBuffer[fileStartAddr + i];
                                        break;

                                    case 1:
                                        linkedByte = driveBuffer[fileStartAddr + i];
                                        break;

                                    case 2:
                                        addrBytes[0] = driveBuffer[fileStartAddr + i];
                                        break;

                                    case 3:
                                        addrBytes[1] = driveBuffer[fileStartAddr + i];
                                        break;

                                    case 4:
                                        addrBytes[2] = driveBuffer[fileStartAddr + i];
                                        break;

                                    case 5:
                                        lenByte = driveBuffer[fileStartAddr + i];
                                }
                            }

                            //data section
                            else if(i >= 6)
                            {
                                //add 6 to compensate for max size of data only being 250 (total block = 256)
                                if(i < (lenByte + 6))
                                    dataBuffer.push_back(driveBuffer[fileStartAddr + i]);
                            }
                        }
                        fileStartAddr = (addrBytes[0] << 16) | (addrBytes[1] << 8) | addrBytes[2]; //get next block address

                    }while(linkedByte == '&');

                    //for(int i = 0; i < dataBuffer.size(); ++i)
                    //    std::cout << std::hex << (int)dataBuffer.at(i) << ' ';
                    //std::cout << std::endl;
                    
                    std::ofstream fileOut;

                    //remove padding from name
                    int idx = 0;
                    for(int i = fileName.size() - 1; i >= 0; --i)
                    {
                        if(fileName.at(i) != ' ')
                        {
                            idx = i;
                            break;
                        }
                    }

                    fileOut.open(fileName.substr(0, idx + 1) + '.' + extName);              
                    for(int i = 0; i < dataBuffer.size(); ++i)
                        fileOut << dataBuffer.at(i);
                    fileOut.close();
                    return;
                }
            }
        }
    }

    std::cout << "Error: File not found.\n";  
}

void deleteFile(std::string diskName)
{
    std::string fileName;
    std::string extName;

    std::cout << "Enter file name: ";
    std::cin >> fileName;
    while(fileName.size() > 11)
        std::cin >> fileName;

    std::cout << "\nEnter file extension: ";
    std::cin >> extName;
    while(extName.size() > 3)
        std::cin >> extName;

    //add padding to file name and ext
    int sz = 11 - fileName.size();
    for(int i = 0; i < sz; ++i)
        fileName += " ";
    sz = 3 - extName.size();
    for(int i = 0; i < 3 - extName.size(); ++i)
        extName += " ";

    std::string fileNameTmp;
    std::string extTmp;
    int fileIndex;
    for(int i = 0; i < MAX_FILES - 1; ++i)
    {
        fileNameTmp.clear();
        extTmp.clear();
        fileIndex = i;
        if(driveBuffer[i * ENTRY_SIZE] != 0xFF) //scan entries
        {
            for(int j = 0; j < 11; ++j)
                fileNameTmp += driveBuffer[j + ENTRY_SIZE * i];

            for(int j = 0; j < 3; ++j)
                extTmp += driveBuffer[(11 + j) + (ENTRY_SIZE * i)];
        }

        //find matching file
        unsigned int fileStartAddr = 0;
        std::vector <unsigned char> dataBuffer;
        if(std::strcmp(fileName.c_str(), fileNameTmp.c_str()) == 0)
        {
            //with matching extension
            if(std::strcmp(extName.c_str(), extTmp.c_str()) == 0)
            {
                std::cout << "File found\n";
                std::cout << "Index " << fileIndex << std::endl;

                //17, 18 are pointing to address bytes in entry table
                //fileStartAddr = (driveBuffer[(fileIndex * ENTRY_SIZE) + 17] << 8) | 
                //(int)driveBuffer[(fileIndex * ENTRY_SIZE) + 18];

                fileStartAddr = (int)(driveBuffer[(fileIndex * ENTRY_SIZE) + 17] << 16) | 
                (int)(driveBuffer[(fileIndex * ENTRY_SIZE) + 18] << 8) |
                (int)(driveBuffer[(fileIndex * ENTRY_SIZE) + 19]);

                //erase entry
                for(int i = 0; i < ENTRY_SIZE; ++i)
                {
                    if(i == 0)
                        driveBuffer[(fileIndex * ENTRY_SIZE)] = 0xFF;

                    else
                        driveBuffer[(fileIndex * ENTRY_SIZE) + i] = '@';
                }

                //meta data
                //unsigned char usedByte = driveBuffer[fileStartAddr + i];
                unsigned char linkedByte; 
                unsigned char addrBytes[3];
                unsigned char lenByte;
       
                do
                {
                    linkedByte  = driveBuffer[fileStartAddr + i];
                    addrBytes[0] = driveBuffer[fileStartAddr + i + 1];    
                    addrBytes[1] = driveBuffer[fileStartAddr + i + 2];        
                    addrBytes[2] = driveBuffer[fileStartAddr + i + 3];

                    driveBuffer[fileStartAddr] = '!'; //erase file links   
                    fileStartAddr = (addrBytes[0] << 16) | (addrBytes[1] << 8) | addrBytes[2]; //get next block address
                }while(linkedByte == '&');
                
                saveToDiskFile(diskName);
                return;
            }
        }
    }

    std::cout << "Error: File not found.\n";  
}

int findBlankDirEntry()
{
    int entryIndx = 0;
    bool entryFound = false;
    for(int i = 0; i < MAX_FILES - 1; ++i)
    {
        if(driveBuffer[0x1000 + i * 14] == '@') //scan entries
        {
            entryFound = true;
            entryIndx = i;
            break;
        }
    }

    if(entryFound)
    {
        return entryIndx;
    }

    else
        std::cout << "\nError: No space left in entry table!\n";

    return -1; //error
}

void createDir(int entryIndx, std::string dirName, unsigned char folderId, unsigned char containingFolder)
{
    //Starts @ 0x1000
	//Ends @ 0x1FFF

	//Total size of entry 14 bytes
	//	Used 1 byte (used '*', free '@')
	//	Directory name 11 bytes
	//	Directory ID number 1 byte
	//	Folder ID 1 byte, so folders can be in folders creating tree structure

	//Total directory entries = 292 (4096 / 14)

    driveBuffer[0x1000 + (entryIndx * DIR_ENTRY_SIZE)] = '*'; //used

    for(int i = 0; i < 11; ++i)
    {
        if(dirName.size() - 1 < i)
            driveBuffer[0x1000 + (entryIndx * DIR_ENTRY_SIZE) + i + 1] = ' ';

        else
            driveBuffer[0x1000 + (entryIndx * DIR_ENTRY_SIZE) + i + 1] = dirName.at(i);
    }

    driveBuffer[0x1000 + (entryIndx * DIR_ENTRY_SIZE) + 12] = folderId;
    driveBuffer[0x1000 + (entryIndx * DIR_ENTRY_SIZE) + 13] = containingFolder;
}

unsigned char generateNewId(unsigned char &folderId)
{
    std::vector <unsigned char> freeIds;
    for(int i = 1; i < 256; ++i) //skip 0 since that is root ID
        freeIds.push_back(i);

    bool idCreated = false;
    //create list of all used entry Id's
    for(int entry = 0; entry < MAX_DIRECTORIES; ++entry)
    {
        //find entries that already exist
        if(driveBuffer[0x1000 + (entry * DIR_ENTRY_SIZE)] == '*')
        {
            unsigned char id = driveBuffer[0x1000 + (entry * DIR_ENTRY_SIZE) + 12];

            //remove from free Id's
            for(int i = 0; i < freeIds.size(); ++i)
            {
                if(freeIds.at(i) == (int)id)
                    freeIds.at(i) = 0;
            }
        }
    }

    //get first free ID
    int index = 0;
    while(true)
    {
        if(freeIds.at(index) == 0)
            ++index;
        
        else
        {
            folderId = freeIds.at(index);
            return folderId;
        }

        if(index == 255)
            return 0;
    }
    return 0;
}

bool doesDirExist(std::string dirName)
{
    char buff[11];
    for(int entry = 0; entry < MAX_DIRECTORIES; ++entry)
    {
        for(int i = 0; i < 11; ++i)
        {
            buff[i] = driveBuffer[0x1000 + (entry * DIR_ENTRY_SIZE) + 1 + i];
        }

        if(strcmp(dirName.c_str(), buff) == 0)
            return true;
    }

    return false;
}

void addDir(std::string diskName)
{
    int entryIndx = findBlankDirEntry();

    std::string dirName;
    std::cout << "Enter name of directory: ";
    std::cin >> dirName;

    unsigned char containingFolder = currentDirId; //default is root 0x0

    if(doesDirExist(dirName))
        std::cout << "Error: Unable to create folder, a folder with this name already exists.\n";

    unsigned char folderId;
    if(generateNewId(folderId));
    
    else
    {
        std::cout << "Error: Unable to create folder, max number of directories already exist.\n";
        return;
    }

    createDir(entryIndx, dirName, folderId, containingFolder);
    saveToDiskFile(diskName);
}

int getDirId(std::string dirName)
{
    //add padding to dirName, if needed
    while(dirName.size() < 11)
        dirName += 0x20; //spaces

    for(int entry = 0; entry < MAX_DIRECTORIES; ++entry)
    {
        //find current entry, then go to containing folder (walk up tree)
        if(driveBuffer[0x1000 + (entry * DIR_ENTRY_SIZE)] == '*' && //only look at active entries...
        driveBuffer[0x1000 + (entry * DIR_ENTRY_SIZE) + 13] == currentDirId) //..in current containing folder
        {
            std::string tmp;
            for(int i = 0; i < 11; ++i)
                tmp += driveBuffer[0x1000 + (entry * DIR_ENTRY_SIZE) + i + 1];
            
            if(strcmp(tmp.c_str(), dirName.c_str()) == 0) //match found, get ID and return it
                return driveBuffer[0x1000 + (entry * DIR_ENTRY_SIZE) + 12];
        }
    }

    return -1; //no entry found
}

void changeDir()
{
    std::string dirName;
    std::cout << "Enter directory name or enter \"..\" to go up directory: ";
    std::cin >> dirName;

    if(strcmp(dirName.c_str(), "..") == 0)
    {
        if(dirName.size() > 2)
        {
            std::cout << "Invalid entry.\n";
            return;
            //error
        }

        else
        {
            for(int entry = 0; entry < MAX_DIRECTORIES; ++entry)
            {
                //find current entry, then go to containing folder (walk up tree)
                if(currentDirId == driveBuffer[0x1000 + (entry * DIR_ENTRY_SIZE) + 12])
                {
                    currentDirId = driveBuffer[0x1000 + (entry * DIR_ENTRY_SIZE) + 13];
                    break;
                }
            }
        }
    }

    else
    {
        for(int i = 0; i < dirName.size(); ++i)
        {
            if(dirName.at(i) == '.')
            {
                std::cout << "Invalid entry.\n";
                return;
            }
        }

        int tmpDirId = getDirId(dirName);
        if(tmpDirId == -1) //error
            std::cout << "This directory does not exist.\n";
        
        else
            currentDirId = tmpDirId; //no errors, so change to new ID
    }
}

void moveFile(std::string diskName)
{

}

void pwd(std::string diskName)
{

}

void help()
{
    std::cout << "\n'touch' - Create new file\n";
    std::cout << "'dir' - Show contents of current directory\n";
    std::cout << "'get' - Extract file to host system\n";
    std::cout << "'del' - Delete a file\n";
    std::cout << "'mkdir' - Create new directory\n";
    std::cout << "'cd' - Change current working directory\n";
    std::cout << "'mv' - Move file locatioin\n";
    std::cout << "'exit' - Close file explorer\n";
    std::cout << "'help' - Displays this screen\n";
}

void commandInterpreter(std::string diskName)
{
    while(true)
    {
        std::string currentDir;
        std::string commandStr;

        std::string buildPwdStr;
        unsigned char nextUpId = currentDirId;
        do
        {
            for(int entry = 0; entry < MAX_DIRECTORIES; ++entry)
            {
                //find current entry, then go to containing folder (walk up tree)
                if(nextUpId == driveBuffer[0x1000 + (entry * DIR_ENTRY_SIZE) + 12])
                {
                    nextUpId = driveBuffer[0x1000 + (entry * DIR_ENTRY_SIZE) + 13];

                    for(int i = 0; i < 11; ++i)
                        buildPwdStr += driveBuffer[0x1000 + (entry * DIR_ENTRY_SIZE) + 1 + i];

                    //remove padding
                    bool removingPad = true;
                    int index = buildPwdStr.size();
                    int amnt = 0;
                    while(removingPad)
                    {
                        --index;
                        
                        if(buildPwdStr.at(index) != ' ')
                        {
                            buildPwdStr = buildPwdStr.substr(0, buildPwdStr.size() - amnt);
                            removingPad = false;
                        }
                        ++amnt;
                    }

                    buildPwdStr += '/';
                }
            }
        }while(nextUpId != 0);
        std::cout << buildPwdStr << std::endl;

        //std::cout << "C:/> ";
        std::cout << buildPwdStr + "/>";
        std::cin >> commandStr;

        if(strcmp("touch", commandStr.c_str()) == 0)
            writeFile(diskName);

        else if(strcmp("dir", commandStr.c_str()) == 0)
            dir(currentDirId);

        else if(strcmp("get", commandStr.c_str()) == 0)
            extractFile(currentDirId);

        else if(strcmp("del", commandStr.c_str()) == 0)
            deleteFile(diskName);

        else if(strcmp("mkdir", commandStr.c_str()) == 0)
            addDir(diskName);

        else if(strcmp("cd", commandStr.c_str()) == 0)
            changeDir();

        else if(strcmp("mv", commandStr.c_str()) == 0)
            moveFile(diskName);

        else if(strcmp("exit", commandStr.c_str()) == 0)
            return;

        else if(strcmp("help", commandStr.c_str()) == 0)
            help();

        else
            std::cout << commandStr << " is an invalid command. For a list of commands type 'help'.\n";
    }
}

int main()
{
    int command = 0;
    std::string diskName = "test.img";
    loadDiskBuffer(diskName);

    commandInterpreter(diskName);

    /*while(true)
    {
        std::cout << "1 = Add file\n2 = Dir listing\n3 = Extract file\n4 = Delete file\n5 = New Directory\n6 = Change dir\n7 = Move file\n8 = PWD\n9 = Exit\n\n";
        //std::cin.ignore();
        std::cin >> command;

        switch(command)
        {
            case 1:
                writeFile(diskName);
                break;

            case 2:
                dir(currentDirId);
                break;

            case 3:
                extractFile(currentDirId);
                break;

            case 4:
                deleteFile(diskName);
                break;

            case 5:
                addDir(diskName);
                break;

            case 6:
                changeDir();
                break;

            case 7:
                moveFile(diskName);
                break;

            case 8:
                pwd(diskName);
                break;

            case 9:
                return 0;
                break;

            default:
                break;
        }
    }*/
}