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
#include "test_result.h"

test_result::test_result()
{
    m_elapsed_time = 0;
    m_success_count = 0;
}

test_result::~test_result()
{
    delete_contents();
}

//set the name of the test
void
test_result::set_name(_STRING name)
{
    m_name = name;
}

//get the name of the test
_STRING
test_result::get_name(void) const
{
    return m_name;
}

//set the elapsed time of the test
void
test_result::set_elapsed_time(int time)
{
    m_elapsed_time = time;
}

//get the elapsed time of the test
int
test_result::get_elapsed_time(void) const
{
    return m_elapsed_time;
}

//adds an error to the result
void
test_result::add_error(const test_problem& new_error)
{
    m_errors.push_back(new_error);
}

//adds a failure
void
test_result::add_failure(const test_problem& new_failure)
{
    m_failures.push_back(new_failure);
}

//adds a success
void
test_result::add_success(void)
{
    ++m_success_count;
}

//adds another result
void
test_result::add_result(const test_result& r)
{
    //add errors and failures
    for (_VECTOR(test_problem, unit_test_allocator)::iterator iter = r.m_errors.begin(); iter != r.m_errors.end(); ++iter)
    {
        m_errors.push_back(*iter);
    }

    for (_VECTOR(test_problem, unit_test_allocator)::iterator iter1 = r.m_failures.begin(); iter1 != r.m_failures.end(); ++iter1)
    {
        m_failures.push_back(*iter1);
    }
    //add successes to accumulated success count
    m_success_count += r.m_success_count;

    // If result being added has subtests, add each subtest.
    if (r.subresults().size() > 0)
    {
        for (_VECTOR(test_result, unit_test_allocator)::iterator iter2 = r.m_subresults.begin(); iter2 != r.m_subresults.end(); ++iter2)
        {
            m_subresults.push_back(*iter2);
        }
    }
    // If no subtests, add the test as a subtest.
    else
    {
        m_subresults.push_back(r);
    }
}

//deletes the contents of the test result
void
test_result::delete_contents(void)
{
    m_errors.clear();
    m_failures.clear();
    m_subresults.clear();
    m_success_count = 0;
    m_elapsed_time = 0;
}

const _VECTOR(test_problem, unit_test_allocator)&
test_result::errors(void) const
{
    return m_errors;
}

const _VECTOR(test_problem, unit_test_allocator)&
test_result::failures(void) const
{
    return m_failures;
}

const _VECTOR(test_result, unit_test_allocator)&
test_result::subresults(void) const
{
    return m_subresults;
}

int
test_result::success_count(void) const
{
    return m_success_count;
}

int
test_result::total_test_count(void) const
{
    return m_success_count
           + m_errors.size()
           + m_failures.size();
}

