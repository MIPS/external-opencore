/* ------------------------------------------------------------------
 * Copyright (C) 1998-2010 PacketVideo
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
#include "unit_test_common.h"
#include "unit_test_xml_writer.h"

bool unit_test_str_check_and_escape_utf8(const char *str_buf_in, char *str_buf_out, unsigned& num_escape_bytes,
        unsigned max_out_buf_bytes, unsigned max_bytes = 0, unsigned * num_bytes_written = NULL)
{
    bool result = true, done = false, found = false;
    unsigned bytes_left, bytes_left_out;
    unsigned total_num_escape_bytes;
    const char *ptr;
    char *ptr_out;

    ptr = str_buf_in;
    ptr_out = str_buf_out;
    bytes_left = max_bytes;
    bytes_left_out = max_out_buf_bytes;
    total_num_escape_bytes = 0;

    if (ptr == NULL)
        return false;

    if (num_bytes_written) *num_bytes_written = 0;
    while (!done)
    {
        if ((*ptr == 0x0) && (max_bytes == 0)) //if need to be terminated at first null char
        {
            result = true;
            done = true;
            if (ptr_out)
            {
                if (bytes_left_out >= 1)
                {
                    *ptr_out = *ptr;
                    if (num_bytes_written) *num_bytes_written += 1;
                }
                else
                {
                    result = false;
                }
            }
            continue;
        }
        if (*ptr == '&')
        {
            found = true;
            total_num_escape_bytes += 5; //& -> &amp;
            if (ptr_out)
            {
                if (bytes_left_out >= 5)
                {
                    *ptr_out++ = '&';
                    *ptr_out++ = 'a';
                    *ptr_out++ = 'm';
                    *ptr_out++ = 'p';
                    *ptr_out++ = ';';
                    bytes_left_out -= 5;
                    if (num_bytes_written) *num_bytes_written += 5;
                }
                else
                {
                    result = false;
                    done = true;
                    continue;
                }
            }
        }
        else if (*ptr == '<')
        {
            found = true;
            total_num_escape_bytes += 4; //< -> &lt;
            if (ptr_out)
            {
                if (bytes_left_out >= 4)
                {
                    *ptr_out++ = '&';
                    *ptr_out++ = 'l';
                    *ptr_out++ = 't';
                    *ptr_out++ = ';';
                    bytes_left_out -= 4;
                    if (num_bytes_written) *num_bytes_written += 4;
                }
                else
                {
                    result = false;
                    done = true;
                    continue;
                }
            }
        }
        else if (*ptr == '>')
        {
            found = true;
            total_num_escape_bytes += 4; //> -> &gt;
            if (ptr_out)
            {
                if (bytes_left_out >= 4)
                {
                    *ptr_out++ = '&';
                    *ptr_out++ = 'g';
                    *ptr_out++ = 't';
                    *ptr_out++ = ';';
                    bytes_left_out -= 4;
                    if (num_bytes_written) *num_bytes_written += 4;
                }
                else
                {
                    result = false;
                    done = true;
                    continue;
                }
            }
        }
        else if (*ptr == '\'')
        {
            found = true;
            total_num_escape_bytes += 6; //' -> &apos;
            if (ptr_out)
            {
                if (bytes_left_out >= 6)
                {
                    *ptr_out++ = '&';
                    *ptr_out++ = 'a';
                    *ptr_out++ = 'p';
                    *ptr_out++ = 'o';
                    *ptr_out++ = 's';
                    *ptr_out++ = ';';
                    bytes_left_out -= 6;
                    if (num_bytes_written) *num_bytes_written += 6;
                }
                else
                {
                    result = false;
                    done = true;
                    continue;
                }
            }
        }
        else if (*ptr == '"')
        {
            found = true;
            total_num_escape_bytes += 6; //" -> &quot
            if (ptr_out)
            {
                if (bytes_left_out >= 6)
                {
                    *ptr_out++ = '&';
                    *ptr_out++ = 'q';
                    *ptr_out++ = 'u';
                    *ptr_out++ = 'o';
                    *ptr_out++ = 't';
                    *ptr_out++ = ';';
                    bytes_left_out -= 6;
                    if (num_bytes_written) *num_bytes_written += 6;
                }
                else
                {
                    result = false;
                    done = true;
                    continue;
                }
            }
        }
        else
        {
            if (ptr_out)
            {
                if (bytes_left_out >= 1)
                {
                    *ptr_out++ = *ptr;
                    bytes_left_out -= 1;
                    if (num_bytes_written) *num_bytes_written += 1;
                }
                else
                {
                    result = false;
                    done = true;
                    continue;
                }
            }

            total_num_escape_bytes += 1;
        }

        if (bytes_left > 0)
            bytes_left--;
        if (max_bytes && !bytes_left) //done when there is no more data
            done = true;

        ptr++;
    } //end while loop

    if (!found && (str_buf_out == 0))
        num_escape_bytes = 0;
    else
        num_escape_bytes = total_num_escape_bytes;

    return result;
}

bool unit_test_str_need_escape_xml(const char *str_buf, unsigned& num_escape_bytes, unsigned max_bytes = 0)
{
    char *str_buf_out;
    unsigned max_out_buf_bytes = 0;

    str_buf_out = NULL;
    num_escape_bytes = 0;
    return unit_test_str_check_and_escape_utf8(str_buf, str_buf_out, num_escape_bytes, max_out_buf_bytes, max_bytes);
}

int unit_test_str_escape_xml(const char *str_buf_in, char *str_buf_out, unsigned max_out_buf_bytes, unsigned max_bytes = 0, unsigned * num_bytes_written = NULL)
{
    int total_num_bytes = 0;

    if (unit_test_str_check_and_escape_utf8(str_buf_in, str_buf_out, (unsigned&)total_num_bytes, max_out_buf_bytes, max_bytes, num_bytes_written) == false)
        total_num_bytes = -total_num_bytes;


    return total_num_bytes;
}

_STRING
UnitTest_XMLWriter::escape(_STRING value)
{
    unsigned num_escape_bytes;
    if (!unit_test_str_need_escape_xml(value.c_str(), num_escape_bytes))
    {
        // Trigger an assert.
        int var = 0;
        var = 1 / var;
    }
    if (num_escape_bytes > 0)
    {
        char* buffer = new char[num_escape_bytes + 1];
        unit_test_str_escape_xml(value.c_str(), buffer, num_escape_bytes);
        buffer[num_escape_bytes] = '\0';
        value = buffer;
        delete [] buffer;
    }
    return value;
}

unsigned UnitTest_XMLWriter::start(_STRING name)
{
    if (tagOpen)
    {
        _APPEND(document, ">");
    }
    openTags.push_back(name);
    tagOpen = true;
    _APPEND(document, "<");
    _APPEND(document, name);
    // Add attributes
    while (!attributes.empty())
    {
        _APPEND(document, " ");
        _APPEND(document, attributes.back());
        attributes.pop_back();
        _APPEND(document, "=\"");
        _APPEND(document, attributes.back());
        attributes.pop_back();
        _APPEND(document, "\"");
    }
    return openTags.size() - 1;
}

void UnitTest_XMLWriter::close()
{
    if (tagOpen)
    {
        _APPEND(document, " />");
    }
    else
    {
        _APPEND(document, "</");
        _APPEND(document, openTags.back());
        _APPEND(document, ">");
    }
    tagOpen = false;
    openTags.pop_back();
}

void UnitTest_XMLWriter::addAttribute(_STRING name, _STRING value)
{
    attributes.push_back(escape(value));
    attributes.push_back(name);
}

void UnitTest_XMLWriter::addAttribute(_STRING name, int value)
{
    addAttribute(name, _yak_itoa(value));
}

void UnitTest_XMLWriter::close(unsigned id)
{
    while (openTags.size() > id)
    {
        close();
    }
}

void UnitTest_XMLWriter::element(_STRING name, _STRING value)
{
    start(name);
    _APPEND(document, ">");
    tagOpen = false;
    _APPEND(document, escape(value));
    close();
}

_STRING UnitTest_XMLWriter::to_str()
{
    return document;
}
