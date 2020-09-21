// FileExcavate.cpp : Defines the entry point for the console application.
//

#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <io.h>
#include <share.h>
#include <string.h>
#include <stdlib.h>

/***************************************************************************
Funciton Prototypes;
***************************************************************************/

int SplitFile();
int JoinFiles();
int Flex(int arrgc, char *arrgv[]);
void HandleFileIOErr(char* filePath);
int deal_with_options(int arrgc, char *arrgv[]);
void PrintUsage();

/***************************************************************************
Global variables.
***************************************************************************/

const int FLEX_FPATH_SZ             = 256;
const int FLEX_MAX_FILES_TO_JOIN    = 100;
static bool g_dumpToFile            = false;
static bool g_splitFile             = false;
static bool g_joinFiles             = false;
static int g_numSplitDestFiles      = 0;
static char g_fileToSplit[FLEX_FPATH_SZ];
static char g_joinDestFile[FLEX_FPATH_SZ];
static char g_joinsSrcFile[FLEX_MAX_FILES_TO_JOIN][FLEX_FPATH_SZ];
static int g_joinSrcFileCnt         = 0;

//static const int g_blockSz          = 1048576;
static const __int64 g_blockSz        = 15000000;

static char g_byteBuff[g_blockSz + 1];

/***************************************************************************
Create an exception class to throw
***************************************************************************/
class Exception
{
};

/***************************************************************************
Main function
***************************************************************************/

int main(int argc, char* argv[])
{
    int retVal = 0;

    argc = deal_with_options(argc, argv);
    printf("\n");

    if (g_splitFile)
        retVal = SplitFile();
    else if (g_joinFiles)
        retVal = JoinFiles();
    else
        retVal = Flex(argc, argv);

	return retVal;
}

/***************************************************************************
Print usage info.
***************************************************************************/

void PrintUsage()
{
    printf("\n  Splits a file into x constituents or joins n files into one.\n"
        "  Also prints the contents of specifed byte range in specified file.\n\n"
        "      flex [-dsj] fileName [startByte [endByte] ] [destFile srcFile1 srcFile2 ...]\n\n"
        "  If startByte is not given, printing starts from the first byte.\n"
        "  If endByte is not given, 500 bytes are printed from startByte.\n"
        "  -d Dump excavated bytes to file with .excv extension.\n"
        "  -sn split fileName n times into approximately equal sizes with suffix '_x'.\n"
        "  -j join files (srcFile1 srcFile2 ...) into one destination file (destFile).\n\n");
}

/***************************************************************************
This function removes the first character from the passed string.  This
is called from deal_with_options() to remove the special purpose slash
character.
***************************************************************************/

void RemoveFirstChar(char *str)
{
    unsigned int i;
    for(i = 1; i < strlen(str); i++)
        str[i] = str[i + 1];
}

/***************************************************************************
This function checks all args to see if they begin with a hyphen.
If so the necessary flags are set.  argc and argv[] are adjusted
accordingly, set back to the condition of the option not having been
supplied at the com line (i.e. all except the first argv[] ptr are
bumped back in the array).
***************************************************************************/

int deal_with_options(int arrgc, char *arrgv[])
{
    long i ,j, n;
    size_t num_opts;

    for(j = 1; j < arrgc; j++)
    {
        // If encounter a pipe symbol, '|', args handled by this app are done
        if(*arrgv[j] == '|')
        {
            // Set num args for this app to current loop counter value
            arrgc = j;
            break;
        }

        // Only process as a switch or option if begins with '-'
        if(*arrgv[j] == '-')
        {
            if(*(arrgv[j] + 1) == '/')
            {
                // Remove the backslash for option processing then go on to
                // next command line arg. Succeeding with '/' signals that
                // the hyphen is not to be interpreted as a cmd line option.
                RemoveFirstChar(arrgv[j]);
            }
            else
            {
                num_opts = strlen(arrgv[j]) - 1;
                for(i = 1; i <= (long)num_opts; i++)
                {
                    switch(*(arrgv[j] + i))
                    {
                    case 'd':
                        g_dumpToFile = true;
                        break;

                    case 's':

                        g_splitFile = true;

                        // The next chars are the number of dest files
                        g_numSplitDestFiles = atoi(arrgv[j] + i + 1);

                        // Make sure there is another arg
                        if (arrgc - j < 2)
                        {
                            // Error
                            PrintUsage();
                            arrgc = 0;
                            return -1;
                        }

                        strcpy_s(g_fileToSplit, FLEX_FPATH_SZ, arrgv[j + 1]);
                        arrgc--;

                        // Set to move to next arg
                        i = (long)num_opts + 1;
                        break;

                    case 'j':

                        g_joinFiles = true;

                        // Make sure there are at least two more args
                        if (arrgc - j < 3 || arrgc - j >= FLEX_MAX_FILES_TO_JOIN - 1)
                        {
                            // Error
                            PrintUsage();
                            arrgc = 0;
                            return -1;
                        }

                        strcpy_s(g_joinDestFile, FLEX_FPATH_SZ, arrgv[j + 1]);
                        arrgc--;

                        // Now save each source file to be joined in a loop
                        for (n = 0; arrgc >= 3; arrgc--, ++n)
                        {
                            strcpy_s(g_joinsSrcFile[n], FLEX_FPATH_SZ, arrgv[j + n + 2]);
                            g_joinSrcFileCnt++;
                            //j--;
                        }

                        break;

                    default:
                        fprintf(stderr, "  Invalid option");
                        break;

                    }
                }

                // Shift to left all args after those we just processed
                for(i = j; i < arrgc - 1; i++)
                    arrgv[i] = arrgv[i + 1];

                arrgc--;
                j = j < 2 ? 0 : j - 1;

            }
        }
    }

    return arrgc;
}

void CopyBlockByBlock(int flDescIn, int flDescOut, __int64 bytesRemaining, char* fName)
{
    unsigned int bytesToRead, bytesRead, bytesWritten;

    try
    {
        // Read g_blockSz bytes from the input file at a time and write
        // to output file.
        while (bytesRemaining > 0)
        {
            // Get block size or num left, read from infile, write to outfile
            bytesToRead = (unsigned int)(bytesRemaining < g_blockSz ? bytesRemaining : g_blockSz);
            bytesRead = _read(flDescIn, g_byteBuff, (unsigned int)bytesToRead);
            if (bytesRead != bytesToRead)
            {
                printf("bytesRead (%ld) != bytesToRead (%ld)", bytesRead, bytesToRead);
                HandleFileIOErr(fName);
                throw Exception();
            }
            bytesWritten = _write(flDescOut, g_byteBuff, (unsigned int)bytesRead);
            if (bytesWritten != bytesRead)
            {
                printf("bytesWritten (%ld) != bytesToWrite (%ld)", bytesWritten, bytesRead);
                HandleFileIOErr(fName);
                throw Exception();
            }
            bytesRemaining -= bytesRead;
            printf("    Wrote %ld bytes to output file, bytes remaining in this input file: %lld\n",
                bytesWritten, bytesRemaining);
        }
    }
    catch (Exception excp)
    {
        throw excp;
    }
}

int SplitFile()
{
    int retVal = 0;
    errno_t errRet;
    struct _stati64 statBuff;
    int i, flDescIn = 0, flDescOut = 0;
    __int64 splitFileSz, nextSplFlSz, total, firstSplitFlSz;
    char splFname[FLEX_FPATH_SZ];

    try
    {
        // Error check
        if (g_numSplitDestFiles <= 1)
        {
            printf("\n  Number of dsetination files (%d) must be greater than 1.\n",
                g_numSplitDestFiles);

            PrintUsage();
            throw Exception();
        }

        // First stat the file to determine how big it is
        errRet = _stati64(g_fileToSplit, &statBuff);

        if (errRet != 0)
        {
            HandleFileIOErr(g_fileToSplit);
            throw Exception();
        }

        printf("  Size of file to split: %lld\n", statBuff.st_size);

        // Error check
        if (g_numSplitDestFiles > statBuff.st_size)
        {
            printf("\n  Number of files to split into (%ld) is greater than orig file size (%lld).\n",
                g_numSplitDestFiles, statBuff.st_size);

            PrintUsage();
            throw Exception();
        }

        // Compute the size of the partial files
        splitFileSz = statBuff.st_size / g_numSplitDestFiles;

        total = g_numSplitDestFiles * splitFileSz;
        firstSplitFlSz = splitFileSz + (statBuff.st_size - total);

        // Error check
        if (total - splitFileSz + firstSplitFlSz != statBuff.st_size)
            throw Exception();

        printf("  Size of first destination file: %lld, size of remaining destination files: %lld\n\n",
            firstSplitFlSz, splitFileSz);

        // Open the input file for reading
        errRet = _sopen_s(&flDescIn, g_fileToSplit, _O_BINARY | _O_RANDOM | _O_RDONLY, _SH_DENYNO, _S_IREAD);

        if (errRet != 0)
        {
            HandleFileIOErr(g_fileToSplit);
            throw Exception();
        }

        // Loop once for each split file
        for (i = 0; i < g_numSplitDestFiles; ++i)
        {
            nextSplFlSz = i == 0 ? firstSplitFlSz : splitFileSz;

            // Create the next split file name then open to create
            sprintf_s(splFname, FLEX_FPATH_SZ, "%s_%d", g_fileToSplit, i + 1);

            // File must have write permission in order to be overwritten
            _chmod(splFname, _S_IREAD | _S_IWRITE);
            errRet = _sopen_s(&flDescOut, splFname, _O_WRONLY | _O_CREAT | _O_TRUNC |
                _O_BINARY | _O_RANDOM, _SH_DENYRW, _S_IREAD | _S_IWRITE);

            if (errRet != 0)
            {
                HandleFileIOErr(splFname);
                throw Exception();
            }

            printf("  Created and opened output file %s\n", splFname);

            CopyBlockByBlock(flDescIn, flDescOut, nextSplFlSz, splFname);

            // Done with this split file so close it and set the permissions to read only
            if (flDescOut != 0)
            {
                _close(flDescOut);
                flDescOut = 0;
                _chmod(splFname, _S_IREAD);
            }

            printf("  Completed writing split file %s - size: %lld\n\n", splFname, nextSplFlSz);
        }
    }
    catch (Exception)
    {
        retVal = -1;
    }

    if (flDescOut != 0)
        _close(flDescOut);

    if (flDescIn != 0)
        _close(flDescIn);

    return retVal;
}

int JoinFiles()
{
    int retVal = 0;
    errno_t errRet;
    struct _stati64 statBuff;
    int i, flDescIn = 0, flDescOut = 0;

    try
    {
        // Error check
        if (g_joinSrcFileCnt < 2)
        {
            printf("\n  Number of files to join (%ld) is less than 2.\n",
                g_joinSrcFileCnt);

            PrintUsage();
            throw Exception();
        }

        // Open the joined destination file name after making it writable
        _chmod(g_joinDestFile, _S_IREAD | _S_IWRITE);
        errRet = _sopen_s(&flDescOut, g_joinDestFile, _O_BINARY | _O_RANDOM | _O_WRONLY | _O_TRUNC | _O_CREAT, _SH_DENYNO, _S_IREAD);

        if (errRet != 0)
        {
            HandleFileIOErr(g_joinDestFile);
            throw Exception();
        }

        printf("  Created destination file: %s\n\n", g_joinDestFile);

        // Loop for each file to join
        for (i = 0; i < g_joinSrcFileCnt; ++i)
        {
            // Stat the source file to determine how big it is
            errRet = _stati64(g_joinsSrcFile[i], &statBuff);

            if (errRet != 0)
            {
                HandleFileIOErr(g_joinsSrcFile[i]);
                throw Exception();
            }

            if (statBuff.st_size < 1)
            {
                printf("  Source file zero bytes, skipping.\n");
                continue;
            }

            // Open the input source file for reading
            errRet = _sopen_s(&flDescIn, g_joinsSrcFile[i], _O_BINARY | _O_RANDOM | _O_RDONLY, _SH_DENYNO, _S_IREAD);

            if (errRet != 0)
            {
                HandleFileIOErr(g_joinsSrcFile[i]);
                throw Exception();
            }

            printf("  Opened source file %s, size: %lld\n", g_joinsSrcFile[i], statBuff.st_size);

            CopyBlockByBlock(flDescIn, flDescOut, statBuff.st_size, g_joinsSrcFile[i]);

            // Done with this source file so close it
            if (flDescIn != 0)
            {
                _close(flDescIn);
                flDescIn = 0;
            }

            printf("  Completed adding source file: %s\n\n", g_joinsSrcFile[i]);

        } // for (i = 0; i < g_numSplitDestFiles; ++i)
    }
    catch (Exception)
    {
        retVal = -1;
    }

    if (flDescOut != 0)
        _close(flDescOut);

    if (flDescIn != 0)
        _close(flDescIn);

    return retVal;
}

int Flex(int arrgc, char *arrgv[])
{
    int retVal = 0;
    struct _stati64 statBuff;
    errno_t errRet;
    char* filePath;
    char dumpFilePath[_MAX_PATH];
    unsigned int bytesRead, bytesWritten;
    __int64 seekRet, startByte, endByte, bytesRemaining, bytesToRead;
    int flDescIn = 0, flDescOut = 0;

    try
    {
        if (arrgc < 2)
        {
            // Print usage
            PrintUsage();
            throw Exception();
        }

       // Set the start and end bytes of the file to read and print
        startByte = arrgc > 2 ? atoi(arrgv[2]) : 0;
        endByte = arrgc > 3 ? atoi(arrgv[3]) : startByte + 500;

        if (startByte < 0 || endByte < 0)
        {
            printf("  startByte (%lld) or endByte (%lld) is negative, possibly due to 4 byte integer overflow.\n", startByte, endByte);
            PrintUsage();
            throw Exception();
        }

        if (endByte < startByte)
        {
            printf("  endByte (%lld) should not be less than startByte (%lld).\n", endByte, startByte);
            PrintUsage();
            throw Exception();
        }

        // Get file path from command line as first arg
        filePath = arrgv[1];

        errRet = _stati64(filePath, &statBuff);

        if (errRet != 0)
        {
            HandleFileIOErr(filePath);
            throw Exception();
        }

        printf("  File size: %lld\n", statBuff.st_size);

        // If size is negative, it is too large to fit into 4 byte int
        if (statBuff.st_size >= 0 && endByte > statBuff.st_size)
            endByte = (int)statBuff.st_size;

        if (endByte <= startByte)
            endByte = startByte;

        // Open the file
        errRet = _sopen_s(&flDescIn, filePath, _O_BINARY | _O_RANDOM | _O_RDONLY, _SH_DENYNO, _S_IREAD);

        if (errRet != 0)
        {
            HandleFileIOErr(filePath);
            throw Exception();
        }

        // Seek to specified start position
        seekRet = _lseeki64(flDescIn, startByte, SEEK_SET);

        if (g_dumpToFile)
        {
            // Create a dump file fname, same as specified file, but add .excv
            sprintf_s(dumpFilePath, _MAX_PATH, "%s.excv", filePath);

            // Open the file
            errRet = _sopen_s(&flDescOut, dumpFilePath, _O_BINARY | _O_RANDOM | _O_CREAT | _O_WRONLY, _SH_DENYNO, _S_IWRITE);

            if (errRet != 0)
            {
                HandleFileIOErr(filePath);
                throw Exception();
            }
        }

        // Init params for loop
        bytesRemaining  = endByte - startByte + 1;
        printf("\n  Print %lld byte(s), from byte locations %lld - %lld\n\n", bytesRemaining, startByte, endByte);

        while (bytesRemaining > 0)
        {
            // Get block size or num left
            bytesToRead = bytesRemaining < g_blockSz ? bytesRemaining : g_blockSz;
            bytesRead = _read(flDescIn, g_byteBuff, (unsigned int)bytesToRead);
            bytesRemaining -= bytesRead;

            if (g_dumpToFile)
                bytesWritten = _write(flDescOut, g_byteBuff, bytesRead);
            else
            {
                g_byteBuff[bytesRead] = 0;
                printf("%s", g_byteBuff);
            }

        }

        printf("\n");
    }
    catch (Exception)
    {
        retVal = -1;
    }

    if (flDescOut != 0)
        _close(flDescOut);

    if (flDescIn != 0)
        _close(flDescIn);

    return retVal;
}

void HandleFileIOErr(char* filePath)
{
    int errVal;
    const int errBuffSz = 512;
    char errBuf[errBuffSz];

    // Print error and get out
    _get_errno(&errVal);
    strerror_s(errBuf, errBuffSz, errVal);
    printf("  File I/O error: %s\n  errVal: %d, errStr: %s\n",
        filePath, errVal, errBuf);

    PrintUsage();
}
