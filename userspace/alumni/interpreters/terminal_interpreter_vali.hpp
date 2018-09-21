/* MollenOS
 *
 * Copyright 2018, Philip Meulengracht
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
 * MollenOS Terminal Implementation (Alumnious)
 * - The terminal emulator implementation for Vali. Built on manual rendering and
 *   using freetype as the font renderer.
 */
#pragma once

#include "terminal_interpreter.hpp"

class CValiTerminalInterpreter : public CTerminalInterpreter {
public:
    CValiTerminalInterpreter(CTerminal& Terminal);
    ~CValiTerminalInterpreter() = default;

    bool HandleKeyCode(unsigned int KeyCode, unsigned int Flags) override;
    void PrintCommandHeader() override;
    void UpdateWorkingDirectory();

private:
    std::string m_Profile;
    std::string m_CurrentDirectory;
};
