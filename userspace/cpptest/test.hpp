/* MollenOS
 *
 * Copyright 2011 - 2018, Philip Meulengracht
 *
 * This program is free software : you can redistribute it and / or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation ? , either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * MollenOS - C/C++ Test Suite for Userspace
 *  - Runs a variety of userspace tests against the libc/libc++ to verify
 *    the stability and integrity of the operating system.
 */
#pragma once
#define __TRACE
#include <os/utils.h>
#include <string>

class OSTest {
public:
    OSTest(const std::string &SuiteName) {
        TestLog(SuiteName);
    }
    virtual int RunTests() = 0;

protected:
    void TestLog(const std::string &Message) {
        TRACE(Message.c_str());
    }
};

#define RUN_TEST_SUITE(ErrorCounter, CClass) CClass *p##CClass = new CClass(); ErrorCounter += p##CClass->RunTests();