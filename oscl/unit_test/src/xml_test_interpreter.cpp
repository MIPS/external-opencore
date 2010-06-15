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
#include "xml_test_interpreter.h"
#ifndef STRINGABLE_H
#include "stringable.h"
#endif

_STRING
problem_string(const test_problem& problem)
{
    _STRING Result;
    _APPEND(Result, problem.filename());
    _APPEND(Result, ":");
    _APPEND(Result, _yak_itoa(problem.line_number()));
    _APPEND(Result, ":");
    _APPEND(Result, problem.message());
    _APPEND(Result, "\n");
    return Result;
}

void
add_problems(UnitTest_XMLWriter & doc, _STRING type, const _VECTOR(test_problem, unit_test_allocator)& vect)
{
    for (_VECTOR(test_problem, unit_test_allocator)::const_iterator iter = vect.begin(); iter != vect.end(); ++iter)
    {
        doc.addAttribute("type", type);
        doc.element("failure", problem_string(*iter));
    }
}

_STRING
xml_test_interpreter::unexpected_termination_interpretation(_STRING executable_name)
{
    UnitTest_XMLWriter doc;

    doc.addAttribute("tests", 1);
    doc.addAttribute("errors", 1);
    doc.addAttribute("failures", 0);

    _STRING suitename;
    _APPEND(suitename, "PV.");
    _APPEND(suitename, executable_name);
    doc.addAttribute("name", suitename);

    int root_id = doc.start("testsuite");

    doc.addAttribute("name", "unknown");
    doc.start("testcase");

    doc.addAttribute("type", "error");
    doc.element("failure", "Test application terminated unexpectedly.  See console logs");

    doc.close();

    doc.close(root_id);

    return doc.to_str();
}

_STRING
xml_test_interpreter::interpretation(const test_result& result, _STRING executable_name, _STRING* pValue)
{
    UnitTest_XMLWriter doc;

    doc.addAttribute("tests", result.total_test_count());
    doc.addAttribute("errors", result.errors().size());
    doc.addAttribute("failures", result.failures().size());
    doc.addAttribute("time", valueToString(result.get_elapsed_time() / 1000.0));

    _STRING suitename;
    _APPEND(suitename, "PV.");
    _APPEND(suitename, executable_name);
    doc.addAttribute("name", suitename);

    int root_id = doc.start("testsuite");

    _VECTOR(test_result, unit_test_allocator)::iterator iter, end;
    end = result.subresults().end();
    for (iter = result.subresults().begin(); iter != end; ++iter)
    {
        test_result& sub_result = *iter;
        if (sub_result.get_name() == NULL)
        {
            doc.addAttribute("name", "undefined");
        }
        else
        {
            doc.addAttribute("name", sub_result.get_name());
        }
        doc.addAttribute("time", valueToString(sub_result.get_elapsed_time() / 1000.0));
        doc.start("testcase");
        add_problems(doc, "failure", sub_result.failures());
        add_problems(doc, "error", sub_result.errors());
        doc.close();
    }

    if (0 != pValue)
        doc.element("system-out", *pValue);

    doc.close(root_id);

    return doc.to_str();
}

