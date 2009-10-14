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
#include "http_composer.h"
#include "http_parcom_internal.h"
#include "string_keyvalue_store.h"
#include "oscl_string_utils.h"
#include "oscl_string_containers.h"

////////////////////////////////////////////////////////////////////////////////////
////// HTTPComposer implementation /////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////

OSCL_EXPORT_REF void HTTPComposer::reset(const bool aKeepAllSettingsExceptURI)
{
    // reset URI
    iURI.setPtrLen("", 0);
    iHeaderLength     = 0;
    iFirstLineLength  = 0;
    iEntityBodyLength = 0;
    if (aKeepAllSettingsExceptURI) return;

    // reset other stuff
    iMethod  = HTTP_METHOD_GET;
    iVersion = HTTP_V1_0;

    if (iKeyValueStore) iKeyValueStore->clear();
}

////////////////////////////////////////////////////////////////////////////////////
// factory method
OSCL_EXPORT_REF HTTPComposer* HTTPComposer::create()
{
    HTTPComposer *composer = OSCL_NEW(HTTPComposer, ());
    if (!composer) return NULL;
    if (!composer->construct())
    {
        OSCL_DELETE(composer);
        return NULL;
    }
    return composer;
}

////////////////////////////////////////////////////////////////////////////////////
bool HTTPComposer::construct()
{
    reset();
    if ((iKeyValueStore = StringKeyValueStore::create()) == NULL) return false;
    return true;
}

////////////////////////////////////////////////////////////////////////////////////
// destructor
OSCL_EXPORT_REF HTTPComposer::~HTTPComposer()
{
    reset();

    // delete iKeyValueStore
    if (iKeyValueStore) OSCL_DELETE(iKeyValueStore);
    iKeyValueStore = NULL;
}

////////////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void HTTPComposer::setURI(const StrPtrLen aURI)
{
    iURI = aURI;

    // get relative URI
    const char *aServerAddPtr = oscl_strstr(((char*)aURI.c_str()), "//");
    if (!aServerAddPtr) return;
    aServerAddPtr += 2;
    const char *aRelativeUriPtr = oscl_strstr(aServerAddPtr, "/");
    if (!aRelativeUriPtr) return;
    iRelativeURI = StrPtrLen(aRelativeUriPtr);
}

////////////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF bool HTTPComposer::setField(const StrCSumPtrLen &aNewFieldName, const char *aNewFieldValue, const bool aNeedReplaceOldValue)
{
    if (!iKeyValueStore) return false;

    if (aNewFieldValue)
    {
        return (iKeyValueStore->addKeyValuePair(aNewFieldName, aNewFieldValue, aNeedReplaceOldValue) ==
                StringKeyValueStore::StringKeyValueStore_Success);
    }

    // remove this field
    return iKeyValueStore->removeKeyValuePair(aNewFieldName);
}

////////////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF bool HTTPComposer::setField(const StrCSumPtrLen &aNewFieldName, const StrPtrLen *aNewFieldValue, const bool aNeedReplaceOldValue)
{
    if (!iKeyValueStore) return false;
    if (!aNewFieldValue) return iKeyValueStore->removeKeyValuePair(aNewFieldName);
    return (iKeyValueStore->addKeyValuePair(aNewFieldName, *aNewFieldValue, aNeedReplaceOldValue) ==
            StringKeyValueStore::StringKeyValueStore_Success);
}


////////////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF int32 HTTPComposer::getCurrentRequestLength(const bool usingAbsoluteURI)
{
    // sanity check
    if (!usingAbsoluteURI && iRelativeURI.length() == 0) return COMPOSE_BAD_URI;

    // since iHeaderLength is updated for each setField, we only need to calculate the request-line length
    // Request-line: Method SP Request-URI SP HTTP-Version CRLF
    iFirstLineLength = oscl_strlen(HTTPMethodString[(uint32)iMethod]); // method
    StrPtrLen *uriPtr = (usingAbsoluteURI ? (&iURI) : (&iRelativeURI));
    iFirstLineLength += uriPtr->length(); // uri
    iFirstLineLength += 8; // "HTTP/1.1" or "HTTP/1.0"
    iFirstLineLength += 4; // 2 SPs + CRLF
    iHeaderLength = iFirstLineLength;

    // header fields part
    iHeaderLength += (iKeyValueStore->getTotalKeyValueLength() +
                      iKeyValueStore->getNumberOfKeyValuePairs() * 4); // 4 => key:SPvalueCRLF, 1SP + ':' + CRLT

    iHeaderLength += 2; // final CRLF

    return (int32)(iHeaderLength + iEntityBodyLength);
}


////////////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF int32 HTTPComposer::compose(OsclMemoryFragment &aComposedMessageBuffer, const bool usingAbsoluteURI, const uint32 aEntityBodyLength)
{
    HTTPMemoryFragment messageBuffer(aComposedMessageBuffer);

    // sanity check, all the list error codes will pop up here if there is an error
    int32 status = santityCheckForCompose(messageBuffer, usingAbsoluteURI, aEntityBodyLength);
    if (status != COMPOSE_SUCCESS) return status;
    iEntityBodyLength = aEntityBodyLength;

    // compose the first request/status line
    composeFirstLine(messageBuffer, usingAbsoluteURI);

    // compose the header part: general header + request header + entity header
    composeHeaders(messageBuffer);

    // if there is an enity body, then it should be in place,
    // then add NULL terminator for a string
    if (messageBuffer.getAvailableSpace() > 0)
    {
        *((char*)messageBuffer.getPtr() + iEntityBodyLength) = HTTP_CHAR_NULL;
        messageBuffer.update(1);
    }

    return COMPOSE_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////////
// supporting function for compose()
int32 HTTPComposer::santityCheckForCompose(HTTPMemoryFragment &aComposedMessageBuffer, const bool usingAbsoluteURI, const uint32 aEntityBodyLength)
{
    // check URI
    if (!usingAbsoluteURI && iRelativeURI.length() == 0) return COMPOSE_BAD_URI;

    // check input buffer size
    if (!aComposedMessageBuffer.isSpaceEnough(getCurrentRequestLength(usingAbsoluteURI) + aEntityBodyLength))
    {
        return COMPOSE_BUFFER_TOO_SMALL;
    }

    // check URI
    if (iURI.length() == 0) return COMPOSE_URI_NOT_SET;

    // check content type and content length for entity body
    if (aEntityBodyLength)
    {
        StrCSumPtrLen contentType("Content-Type");
        if (!iKeyValueStore->isKeyValueAvailable(contentType)) // no "Content-Type" key
            return COMPOSE_CONTENT_TYPE_NOT_SET_FOR_ENTITY_BODY;

        StrCSumPtrLen contentLengthKey("Content-Length");
        StrPtrLen contentLengthValue;
        if (!iKeyValueStore->getValueByKey(contentLengthKey, contentLengthValue)) // no "Content-Length" key
            return COMPOSE_CONTENT_LENGTH_NOT_SET_FOR_ENTITY_BODY;

        uint32 contentLength;
        PV_atoi(contentLengthValue.c_str(), 'd', contentLengthValue.length(), contentLength);
        if (contentLength != aEntityBodyLength) return COMPOSE_CONTENT_LENGTH_NOT_MATCH_ENTITY_BODY_LENGTH;
    }

    return COMPOSE_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////////
void HTTPComposer::composeFirstLine(HTTPMemoryFragment &aComposedMessageBuffer, const bool usingAbsoluteURI)
{
    // Request-line: Method SP Request-URI SP HTTP-Version CRLF
    char *ptr = (char *)aComposedMessageBuffer.getPtr();
    oscl_memcpy(ptr, HTTPMethodString[(uint32)iMethod], oscl_strlen(HTTPMethodString[(uint32)iMethod]));
    ptr += oscl_strlen(HTTPMethodString[iMethod]);
    *ptr++ = HTTP_CHAR_SPACE;
    StrPtrLen *uriPtr = (usingAbsoluteURI ? (&iURI) : (&iRelativeURI));
    oscl_memcpy(ptr, uriPtr->c_str(), uriPtr->length());
    ptr += uriPtr->length();
    *ptr++ = HTTP_CHAR_SPACE;

    // HTTP version
    OSCL_FastString versionString;
    if (iVersion == HTTP_V1_1)
    {
        versionString.set((OSCL_String::chartype*)_STRLIT_CHAR("HTTP/1.1"), 8);
    }
    else
    {
        versionString.set((OSCL_String::chartype*)_STRLIT_CHAR("HTTP/1.0"), 8);
    }
    oscl_memcpy(ptr, versionString.get_cstr(), 8);
    ptr += 8;
    *ptr++ = HTTP_CHAR_CR;
    *ptr++ = HTTP_CHAR_LF;

    OSCL_ASSERT(iFirstLineLength == (uint32)(ptr - (char*)aComposedMessageBuffer.getPtr()));
    aComposedMessageBuffer.update(iFirstLineLength);
}

////////////////////////////////////////////////////////////////////////////////////
class CleanupObject
{
        StrPtrLen *iKeyList;
        StrPtrLen *iValueList;

    public:
        CleanupObject(StrPtrLen *aKeyList = NULL, StrPtrLen *aValueList = NULL) :
                iKeyList(aKeyList), iValueList(aValueList)
        {
            ;
        }

        //! Use destructor to do all the clean up work
        ~CleanupObject()
        {
            if (iKeyList) OSCL_ARRAY_DELETE(iKeyList);
            if (iValueList) OSCL_ARRAY_DELETE(iValueList);
        }

        void setKeyList(StrPtrLen *aKeyList)
        {
            iKeyList = aKeyList;
        }
        void setValueList(StrPtrLen *aValueList)
        {
            iValueList = aValueList;
        }
};

bool HTTPComposer::composeHeaders(HTTPMemoryFragment &aComposedMessageBuffer)
{
    char *ptr = (char *)aComposedMessageBuffer.getPtr();
    CleanupObject cleanup;

    uint32 numKeyValuePairs = iKeyValueStore->getNumberOfKeyValuePairs();
    if (numKeyValuePairs > 0)
    {
        StrPtrLen *keyList = OSCL_ARRAY_NEW(StrPtrLen, numKeyValuePairs);
        cleanup.setKeyList(keyList);
        StrPtrLen *valueList = OSCL_ARRAY_NEW(StrPtrLen, numKeyValuePairs);
        cleanup.setValueList(valueList);
        if (!keyList || !valueList) return false; // let cleanup object to do automatic deallocation

        // get key list
        uint32 numKeys = iKeyValueStore->getCurrentKeyList(keyList);
        if (numKeys == 0)  return false;

        uint32 i, j, index = 0;
        for (i = 0, j = 0; i < numKeyValuePairs && j < numKeys; j++)
        {
            index = 0;
            while (iKeyValueStore->getValueByKey(keyList[j], valueList[i], index))
            {
                //bydefault iMaxLineSizeForMultiLineRequest value is 0x7fffffff, So control will divide header into multiple line.
                if ((keyList[j].length() + valueList[i].length()) > iMaxLineSizeForMultiLineRequest)
                {
                    //Break header field into multiple lines, if any key-value pair's length is greater then iMaxLineSizeForMultiLineRequest
                    createMultiHeaderLine(ptr, keyList[j], valueList[i]);
                }
                else
                {
                    // put key
                    oscl_memcpy(ptr, keyList[j].c_str(), keyList[j].length()); // same field key
                    ptr += keyList[j].length();
                    *ptr++ = HTTP_CHAR_COLON;
                    *ptr++ = HTTP_CHAR_SPACE;

                    // put value
                    oscl_memcpy(ptr, valueList[i].c_str(), valueList[i].length()); // same field key
                    ptr += valueList[i].length();
                    *ptr++ = HTTP_CHAR_CR;
                    *ptr++ = HTTP_CHAR_LF;
                }
                index++;
            }
            i += index;
        }
    }

    // add final CRLF
    *ptr++ = HTTP_CHAR_CR;
    *ptr++ = HTTP_CHAR_LF;

    OSCL_ASSERT(iHeaderLength == (ptr - (char*)aComposedMessageBuffer.getPtr() + aComposedMessageBuffer.getLen()));
    aComposedMessageBuffer.update(ptr);
    return true;
}

bool HTTPComposer::createMultiHeaderLine(char* &ptr, StrPtrLen &keyList, StrPtrLen &valueList)
{
    OsclMemAllocator alloc;
    int32 keyValueLength = keyList.length() + valueList.length() + 2; // 2 for COLON & SPACE
    char *ptrMultiHeader = (char *)alloc.allocate(keyValueLength);
    oscl_memcpy(ptrMultiHeader, "\0", (keyValueLength));
    char *startPtr = ptrMultiHeader;
    //copy full key-value to ptrMultiHeader buffer
    oscl_memcpy(ptrMultiHeader, keyList.c_str(), keyList.length()); // copy key field
    ptrMultiHeader += keyList.length();
    *ptrMultiHeader++ = HTTP_CHAR_COLON;
    *ptrMultiHeader++ = HTTP_CHAR_SPACE;
    oscl_memcpy(ptrMultiHeader, valueList.c_str(), valueList.length()); // copy value field
    ptrMultiHeader = startPtr;
    while (keyValueLength > 0)
    {
        uint32 copyBuffer = OSCL_MIN(keyValueLength, iMaxLineSizeForMultiLineRequest);
        oscl_memcpy(ptr, startPtr, copyBuffer);
        startPtr += copyBuffer;
        ptr += copyBuffer;
        keyValueLength -= copyBuffer;
        *ptr++ = HTTP_CHAR_CR;
        *ptr++ = HTTP_CHAR_LF;
        if (keyValueLength > 0)
        {
            *ptr++ = HTTP_CHAR_SPACE;   // add SPACE or TAB but don't add after last line
            iHeaderLength += 3;
        }
    }
    alloc.deallocate(ptrMultiHeader);
    return true;
}
