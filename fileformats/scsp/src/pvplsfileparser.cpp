/* ------------------------------------------------------------------
 * Copyright (C) 1998-2009 PacketVideo
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 * -------------------------------------------------------------------
 */
#ifndef PVPLSFILEPARSER_H_INCLUDED
#include "pvplsfileparser.h"
#endif

// logging
#define LOGDEBUG(m) PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_VERBOSE, m);
#define LOGERROR(m) PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, m);
#define LOGTRACE(m) PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, m);


OSCL_EXPORT_REF PVPLSFFParser::PVPLSFFParser()
{

    iPLSInfo = NULL;
    iPLSFile = NULL;
    iLogger = PVLogger::GetLoggerObject("PVPLSFFParser");;

    LOGTRACE((0, "PVPLSFFParser::PVPLSFFParser"));
}


OSCL_EXPORT_REF PVPLSFFParser::~PVPLSFFParser()
{
    LOGTRACE((0, "PVPLSFFParser::~PVPLSFFParser"));

    cleanupParser();
    iLogger = NULL;
}


OSCL_EXPORT_REF PVPLSParserReturnCode PVPLSFFParser::IsPLSFile(PVMFDataStreamFactory& aSourceDataStreamFactory)
{
    LOGTRACE((0, "PVPLSFFParser::IsPLSFile"));

    OSCL_wHeapString<OsclMemAllocator> tmpFileName;
    Oscl_FileServer fileServ;
    PVFile pvFile;
    pvFile.SetCPM(&aSourceDataStreamFactory);

    PVPLSParserReturnCode retCode = PVPLSFF_PARSER_FILE_ERROR;

    if (!(pvFile.Open(tmpFileName.get_cstr(), Oscl_File::MODE_READ | Oscl_File::MODE_BINARY, fileServ)))
    {
        char* dataBuf = NULL;

        dataBuf = (char*)(oscl_malloc(sizeof(char) * PLSFF_MIN_DATA_SIZE_FOR_RECOGNITION));
        if (dataBuf != NULL)
        {
            int bytesRead = 0;
            bytesRead = pvFile.Read(dataBuf, sizeof(char), PLSFF_MIN_DATA_SIZE_FOR_RECOGNITION);
            if (bytesRead == PLSFF_MIN_DATA_SIZE_FOR_RECOGNITION)
            {
                // assume "[playlist]" is the first thing in the file
                const char* tagStr = "[playlist]";
                if (isTag(dataBuf, bytesRead, tagStr, oscl_strlen(tagStr)))
                {
                    // found
                    retCode = PVPLSFF_PARSER_OK;
                }
            }
            oscl_free(dataBuf);
        }
        else
        {
            retCode = PVPLSFF_PARSER_MEM_ERROR;
        }
        pvFile.Close();
    }
    return retCode;
}


OSCL_EXPORT_REF PVPLSParserReturnCode PVPLSFFParser::ParseFile(OSCL_wString& aFileName)
{
    LOGTRACE((0, "PVPLSFFParser::ParseFile"));

    Oscl_FileServer fileServer;
    fileServer.Connect();

    // If a PLS file is already open, close it first and delete the file pointer
    cleanupParser();

    // Open the file
    iPLSFile = OSCL_NEW(Oscl_File, (OSCL_FILE_BUFFER_MAX_SIZE));
    if (iPLSFile == NULL)
    {
        return PVPLSFF_PARSER_MEM_ERROR;
    }

    if (iPLSFile->Open(aFileName.get_cstr(), Oscl_File::MODE_READ | Oscl_File::MODE_BINARY, fileServer) != 0)
    {
        return PVPLSFF_PARSER_FILE_ERROR;

    }
    if (iPLSFile->Seek(0, Oscl_File::SEEKEND) != 0)
    {
        return PVPLSFF_PARSER_FILE_ERROR;
    }

    // get file size, this is a local file
    // the file size for PLS is small, the following cast to int32 is ok
    // even when large files are encountered
    int32 fileSize = (TOsclFileOffsetInt32)iPLSFile->Tell();
    if (fileSize <= 0)
    {
        return PVPLSFF_PARSER_FILE_ERROR;
    }

    int32 bufIndex = 0;
    int32 readCount = 0;
    int32 nextIndex = 0;
    int32 seekIndex = 0;
    bool foundPlaylist = false;
    bool foundNumberOfEntries = false;
    bool foundVersion = false;
    int32 numEntries = 0;
    int32 versionNum = -1;
    int32 bytesRead = 0;

    char dataBuffer[PLSFF_MAX_LINE_LENGTH];

    // look for the mandatory fields "[playlist]", "NumberOfEntires" and "Version"
    while (seekIndex < fileSize)
    {
        if (!foundPlaylist || !foundNumberOfEntries || !foundVersion)
        {
            if (bytesRead == 0)
            {
                if (iPLSFile->Seek(seekIndex, Oscl_File::SEEKSET) != 0)
                {
                    // seek failed
                    break;
                }

                bytesRead = iPLSFile->Read(&dataBuffer[0], 1, PLSFF_MAX_LINE_LENGTH);
                if (bytesRead > 0)
                {
                    readCount = seekIndex + bytesRead;
                    bufIndex = 0;
                }
                else
                {
                    // EOF
                    break;
                }
            }
            else
            {
                // next line
                bufIndex += nextIndex;
            }

            char* textStart = NULL;
            int32 textLength = 0;
            nextIndex = 0;

            bool found = getTextLine(&dataBuffer[bufIndex], bytesRead - bufIndex, &textStart, &nextIndex, &textLength);
            if (!found)
            {
                // did not find CR
                if (readCount >= fileSize)
                {
                    // no more text, done with file
                    break;
                }
                // read in the next buffer worth
                seekIndex = readCount - (bytesRead - bufIndex);
                bytesRead = 0;
                continue;
            }
            if (textLength <= 0)
            {
                // empty line
                continue;
            }
            else
            {
                if (!foundPlaylist)
                {
                    const char* tagStr = "[playlist]";
                    foundPlaylist = isTag(textStart, textLength, tagStr, oscl_strlen(tagStr));
                    if (foundPlaylist)
                    {
                        continue;
                    }
                }
                if (!foundNumberOfEntries)
                {
                    const char* tagStr = "NumberOfEntries";
                    foundNumberOfEntries = isTagReturnIntegerValue(textStart, textLength, tagStr, oscl_strlen(tagStr), NULL, &numEntries);
                    if (foundNumberOfEntries)
                    {
                        continue;
                    }
                }
                if (!foundVersion)
                {
                    const char* tagStr = "Version";
                    foundVersion = isTagReturnIntegerValue(textStart, textLength, tagStr, oscl_strlen(tagStr), NULL, &versionNum);
                    if (foundVersion)
                    {
                        continue;
                    }
                }
            }
        }
        else
        {
            // all found
            break;
        }
    } // end while

    // validate the values
    if (!foundPlaylist || !foundNumberOfEntries || !foundVersion || numEntries <= 0 || versionNum != 2)
    {
        cleanupParser();
        return PVPLSFF_PARSER_FILE_ERROR;
    }

    // instantiate a class to hold all the info in the file
    iPLSInfo = OSCL_NEW(PVPLSInfo, (numEntries, versionNum));
    if (iPLSInfo == NULL)
    {
        cleanupParser();
        return PVPLSFF_PARSER_MEM_ERROR;
    }

    // create the entries, entry numbers are 1 based
    for (int i = 0; i < numEntries; i++)
    {
        PVPLSEntry* entry = OSCL_NEW(PVPLSEntry, (i + 1));
        if (entry == NULL)
        {
            cleanupParser();
            return PVPLSFF_PARSER_MEM_ERROR;
        }
        iPLSInfo->iEntryTable.push_back(entry);
    }

    // find the entries
    nextIndex = 0;
    seekIndex = 0;
    bytesRead = 0;
    bufIndex = 0;
    readCount = 0;

    while (seekIndex < fileSize)
    {
        if (bytesRead == 0)
        {
            if (iPLSFile->Seek(seekIndex, Oscl_File::SEEKSET) != 0)
            {
                // seek failed
                break;
            }

            bytesRead = iPLSFile->Read(&dataBuffer[0], 1, PLSFF_MAX_LINE_LENGTH);
            if (bytesRead > 0)
            {
                readCount = seekIndex + bytesRead;
                bufIndex = 0;
            }
            else
            {
                // EOF
                break;
            }
        }
        else
        {
            // next line
            bufIndex += nextIndex;
        }

        char* textStart = NULL;
        int32 textLength = 0;
        nextIndex = 0;

        bool found = getTextLine(&dataBuffer[bufIndex], bytesRead - bufIndex, &textStart, &nextIndex, &textLength);
        if (!found)
        {
            // did not find CR
            if (readCount >= fileSize)
            {
                // no more text, done with file
                break;
            }
            // read in the next buffer worth
            seekIndex = readCount - (bytesRead - bufIndex);
            bytesRead = 0;
            continue;
        }
        if (textLength <= 0)
        {
            // empty line
            continue;
        }
        else
        {
            char* stringStart = NULL;
            int32 stringLen = 0;
            int32 entryNum = -1;
            int32 numLen = 0;

            const char* tagFileStr = "File";
            const char* tagTitleStr = "Title";
            const char* tagLengthStr = "Length";
            if (isTagReturnStringValue(textStart, textLength, tagFileStr, oscl_strlen(tagFileStr), &entryNum, &stringStart, &stringLen))
            {
                // found "File", check string length
                if ((stringLen > 0) && (entryNum > 0) && (entryNum <= numEntries))
                {
                    iPLSInfo->iEntryTable[entryNum - 1]->iUrl = OSCL_NEW(OSCL_wHeapString<OsclMemAllocator>, ());
                    iPLSInfo->iEntryTable[entryNum - 1]->iUrl->setrep_to_wide_char(stringStart, (uint32)stringLen, EOSCL_wStringOp_ExpandASCII, NULL);
                }
                continue;
            }
            if (isTagReturnStringValue(textStart, textLength, tagTitleStr, oscl_strlen(tagTitleStr), &entryNum, &stringStart, &stringLen))
            {
                // found "Title", check string length
                if ((stringLen > 0) && (entryNum > 0) && (entryNum <= numEntries))
                {
                    iPLSInfo->iEntryTable[entryNum - 1]->iTitle = OSCL_NEW(OSCL_wHeapString<OsclMemAllocator>, ());
                    iPLSInfo->iEntryTable[entryNum - 1]->iTitle->setrep_to_wide_char(stringStart, (uint32)stringLen, EOSCL_wStringOp_ExpandASCII, NULL);
                }
                continue;

            }
            if (isTagReturnIntegerValue(textStart, textLength, tagLengthStr, oscl_strlen(tagLengthStr), &entryNum, &numLen))
            {
                // found "Length"
                iPLSInfo->iEntryTable[entryNum - 1]->iLength = numLen;
                continue;
            }
        } // end else
    } // end while

    // done with the file
    if (iPLSFile != NULL)
    {
        iPLSFile->Close();
        OSCL_DELETE(iPLSFile);
        iPLSFile = NULL;
    }
    return PVPLSFF_PARSER_OK;
}


OSCL_EXPORT_REF PVPLSParserReturnCode PVPLSFFParser::GetFileInfo(PVPLSFileInfo& aInfo)
{
    LOGTRACE((0, "PVPLSFFParser::GetFileInfo"));

    if (iPLSInfo == NULL)
    {
        // file is not parsed
        return PVPLSFF_PARSER_NOT_PARSED_ERROR;
    }

    aInfo.numOfEntries = iPLSInfo->iNumEntries;
    aInfo.versionNum = iPLSInfo->iVersion;

    return PVPLSFF_PARSER_OK;
}


OSCL_EXPORT_REF PVPLSParserReturnCode PVPLSFFParser::GetEntry(PVPLSEntry& aEntry, int32 aEntryNum)
{
    LOGTRACE((0, "PVPLSFFParser::GetEntry %d", aEntryNum));

    if (iPLSInfo == NULL)
    {
        // file is not parsed
        return PVPLSFF_PARSER_NOT_PARSED_ERROR;
    }
    if (aEntryNum <= 0 || aEntryNum > iPLSInfo->iNumEntries)
    {
        // out of range
        return PVPLSFF_PARSER_MISC_ERROR;
    }

    aEntry = *iPLSInfo->iEntryTable[aEntryNum - 1];
    return PVPLSFF_PARSER_OK;
}


// private functions
bool PVPLSFFParser::getTextLine(char* strBuffer, int32 strLength, char** textLineStart, int32* nextIndex, int32* textLength)
{
    LOGDEBUG((0, "PVPLSFFParser::getTextLine"));

    int32 curIndex = 0;
    int32 startIndex = -1;
    int32 length = 0;
    bool foundEnd = false;

    // include trailing spaces the first round
    // include embedded tabs
    while (curIndex < strLength)
    {
        if (strBuffer[curIndex] < 0x20)
        {
            if (startIndex == -1)
            {
                // have not found a text char yet
                curIndex++;
            }
            else
            {
                if (strBuffer[curIndex] == '\t')
                {
                    // embedded tabs are ok
                    length++;
                    curIndex++;
                }
                else
                {
                    // all those format characters
                    // found the end of text string
                    foundEnd = true;
                    break;
                }
            }
        }
        else
        {
            if (startIndex == -1)
            {
                if (strBuffer[curIndex] != 0x20)
                {
                    // do not count leading spaces
                    // found the start of text string
                    startIndex = curIndex;
                    length++;
                }
            }
            else
            {
                // count the embedded spaces
                length++;
            }
            curIndex++;
        }
    }

    if (startIndex == -1 || !foundEnd)
    {
        // no valid text or partial string
        return false;
    }

    // strip trailing spaces
    int32 tmpIndex = startIndex + length - 1;
    int32 tmpLength = length;

    while (tmpLength > 0)
    {
        if (strBuffer[tmpIndex] == 0x20)
        {
            tmpIndex--;
            tmpLength--;
        }
        else
        {
            length = tmpLength;
            break;
        }
    }

    // leaving alone the embedded spaces in the text string
    *textLineStart = &strBuffer[startIndex];
    *nextIndex = curIndex;
    *textLength = length;

    return true;
}


// return true if text line starts with ';' or '#'
bool PVPLSFFParser::isCommentLine(char* textLineStart)
{
    LOGDEBUG((0, "PVPLSFFParser::isCommentLine"));

    if (textLineStart[0] == ';' || textLineStart[0] == '#')
    {
        return true;
    }
    return false;
}


// return true if line is "tagString"
bool PVPLSFFParser::isTag(char* textLineStart, int32 textLineLength, const char* tagString, int32 tagStringLen)
{
    LOGDEBUG((0, "PVPLSFFParser::isTag"));

    if (textLineLength == tagStringLen)
    {
        int32 res = oscl_CIstrncmp(&textLineStart[0], tagString, textLineLength);
        if (res == 0)
        {
            return true;
        }
    }
    return false;
}


// return true if line is "tagString=123" or if line is "tagString1=234"
// if there is not entry number between the tagString and '=', entryNum = -1
bool PVPLSFFParser::isTagReturnIntegerValue(char* textLineStart, int32 textLineLength,
        const char* tagString, int32 tagStringLen,
        int32* entryNum, int32* intValue)
{
    LOGDEBUG((0, "PVPLSFFParser::isTagReturnIntegerValue"));

    // the text length should at least this long "tagString=0"
    int32 charCount = tagStringLen;
    if (textLineLength >= (charCount + 2))
    {
        int32 res = oscl_CIstrncmp(&textLineStart[0], tagString, charCount);
        if (res == 0)
        {
            bool foundEntryNum = false;
            int32 entryValue = 0;
            int32 numIndex = -1;
            int32 numLen = 0;

            // there may be an entry number right after the tag string
            // find the entry number
            while (charCount < textLineLength)
            {
                if (textLineStart[charCount] >= 0x30 && textLineStart[charCount] <= 0x39)
                {
                    if (numIndex == -1)
                    {
                        // found the start of number string
                        numIndex = charCount;
                    }
                    numLen++;
                }
                else
                {
                    if (numIndex != -1)
                    {
                        // found the end of number string
                        break;
                    }
                    else
                    {
                        // should be a number, but found something else
                        break;
                    }
                }
                charCount++;
            }

            if (numLen > 0)
            {
                foundEntryNum = true;
                // convert number from ascii to integer
                for (int32 i = numIndex; i < numIndex + numLen; i++)
                {
                    entryValue = (10 * entryValue) + (textLineStart[i] - 0x30);
                }
            }

            bool found = false;
            // find the '='
            while (charCount < textLineLength)
            {
                if (textLineStart[charCount] == '=')
                {
                    found = true;
                    break;
                }
                else
                {
                    charCount++;
                }
            }
            if (!found)
            {
                // can't find '='
                return false;
            }
            // find the number
            charCount++;
            bool negative = false;
            numIndex = -1;
            numLen = 0;

            while (charCount < textLineLength)
            {
                if (textLineStart[charCount] == '-' && numIndex == -1)
                {
                    // negative number
                    negative = true;
                }
                if (textLineStart[charCount] >= 0x30 && textLineStart[charCount] <= 0x39)
                {
                    if (numIndex == -1)
                    {
                        // found the start of number string
                        numIndex = charCount;
                    }
                    numLen++;
                }
                else
                {
                    if (numIndex != -1)
                    {
                        // found the end of number string
                        break;
                    }
                }
                charCount++;
            }
            // convert number from ascii to integer
            // respect the sign
            if (numLen != 0)
            {
                int32 value = 0;
                for (int32 i = numIndex; i < numIndex + numLen; i++)
                {
                    value = (10 * value) + (textLineStart[i] - 0x30);
                }
                if (negative)
                {
                    value *= -1;
                }
                *intValue = value;

                if (foundEntryNum && entryNum != NULL)
                {
                    *entryNum = entryValue;
                }
                return true;
            }
        }
    }
    return false;
}


// return true if line is "tagString0=xyz"
bool PVPLSFFParser::isTagReturnStringValue(char* textLineStart, int32 textLineLength,
        const char* tagString, int32 tagStringLen,
        int32* entryNum, char** entryString, int32* entryStringLen)
{
    LOGDEBUG((0, "PVPLSFFParser::isTagReturnStringValue"));

    // the text length should at least this long "tagString0=x"
    int32 charCount = tagStringLen;
    if (textLineLength >= (charCount + 3))
    {
        int32 res = oscl_CIstrncmp(&textLineStart[0], tagString, charCount);
        if (res == 0)
        {
            bool found = false;
            int32 numIndex = -1;
            int32 numLen = 0;
            int32 strIndex = -1;
            int32 strLen = 0;

            // find the entry number
            while (charCount < textLineLength)
            {
                if (textLineStart[charCount] >= 0x30 && textLineStart[charCount] <= 0x39)
                {
                    if (numIndex == -1)
                    {
                        // found the start of number string
                        numIndex = charCount;
                    }
                    numLen++;
                }
                else
                {
                    if (numIndex != -1)
                    {
                        // found the end of number string
                        break;
                    }
                    else
                    {
                        // should be a number, but found something else
                        break;
                    }
                }
                charCount++;
            }

            // convert number from ascii to integer
            if (numLen == 0)
            {
                return false;
            }

            int32 value = 0;
            for (int32 i = numIndex; i < numIndex + numLen; i++)
            {
                value = (10 * value) + (textLineStart[i] - 0x30);
            }

            // find '='
            found = false;
            while (charCount < textLineLength)
            {
                if (textLineStart[charCount] == '=')
                {
                    found = true;
                    break;
                }
                else if (textLineStart[charCount] == ' ' || textLineStart[charCount] == '\t')
                {
                    charCount++;
                }
                else
                {
                    // shouldn't be anthing else
                    break;
                }
            }
            if (!found)
            {
                return false;
            }

            charCount++;
            found = false;
            // find the text string, ignore leading spaces and tabs
            // trailing spaces have already been removed by getNextLine()
            while (charCount < textLineLength)
            {
                if (textLineStart[charCount] == ' ' || textLineStart[charCount] == '\t')
                {
                    charCount++;
                }
                else
                {
                    // return the rest of the string
                    strIndex = charCount;
                    strLen = textLineLength - charCount;
                    break;
                }
            }

            if (strIndex != -1)
            {
                *entryNum = value;
                *entryString = &textLineStart[strIndex];
                *entryStringLen = strLen;

                return true;
            }
        }
    }
    return false;
}


// cleanup resource
void PVPLSFFParser::cleanupParser(void)
{
    LOGDEBUG((0, "PVPLSFFParser::cleanupParser"));

    if (iPLSFile != NULL)
    {
        iPLSFile->Close();
        OSCL_DELETE(iPLSFile);
        iPLSFile = NULL;
    }

    if (iPLSInfo != NULL)
    {
        OSCL_DELETE(iPLSInfo);
        iPLSInfo = NULL;
    }
}






