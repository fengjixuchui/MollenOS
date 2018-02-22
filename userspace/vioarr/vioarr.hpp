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
 * MollenOS - Vioarr Window Compositor System
 *  - The window compositor system and general window manager for
 *    MollenOS.
 */
#pragma once

/* Includes
 * - Library */
#include <cstdlib>
#include <queue>
#include <thread>

/* Includes
 * - System */
#include "graphics/displays/display_framebuffer.hpp"
#include "screens/screen_login.hpp"
#include "screens/screen.hpp"

#include "input/input_handler.hpp"
#include "utils/log_manager.hpp"

class CVioarrEvent {
public:
    enum EVioarrEventType {
        EventScreenChange,
    };
    CVioarrEvent(EVioarrEventType Type) {
        _Type = Type;
    }
    ~CVioarrEvent() { }
    EVioarrEventType GetType() { return _Type; }
private:
    EVioarrEventType _Type;
};

class CVioarrScreenChangeEvent : public CVioarrEvent {
public:
    CVioarrScreenChangeEvent(CScreenManager::EScreenType Type) 
        : CVioarrEvent(CVioarrEvent::EventScreenChange) {
        _ScreenType = Type;
    }
    CScreenManager::EScreenType GetType() { return _ScreenType; }
private:
    CScreenManager::EScreenType _ScreenType;
};

class VioarrCompositor {
public:
	static VioarrCompositor& GetInstance() {
		// Guaranteed to be destroyed.
		// Is instantiated on first use
		static VioarrCompositor _Instance;
		return _Instance;
	}
private:
	VioarrCompositor() {}                     // Constructor? (the {} brackets) are needed here.

public:
	VioarrCompositor(VioarrCompositor const&) = delete;
	void operator=(VioarrCompositor const&) = delete;

    // Run
    // The main program loop
    int Run() {

        // Initialize state
        _IsRunning      = true;

        // Create the display
        sLog.Info("Creating display");
        _Display = new CDisplayOsMesa();
        if (!_Display->Initialize()) {
            delete _Display;
            return -2;
        }

        // Spawn message handler
        sLog.Info("Spawning message handler");
        SpawnMessageHandler();

        // Load the background texture

        // Create the available screens
        sLog.Info("Creating available screens");
        _ScreenManager = new CScreenManager();
        _ScreenManager->RegisterScreen(new CLoginScreen(_Display), CScreenManager::ScreenLogin);

        // Set available screen
        _ScreenManager->SetActiveScreen(CScreenManager::ScreenLogin, true);

        // Enter event loop
        while (_IsRunning) {
            CVioarrEvent *Event = nullptr;
            {
                std::unique_lock<std::mutex> _eventlock(_EventMutex);
                while (_EventQueue.empty()) _EventSignal.wait(_eventlock);
                Event = _EventQueue.front();
                _EventQueue.pop();
            }
            switch (Event->GetType()) {
                case CVioarrEvent::EventScreenChange: {
                    CVioarrScreenChangeEvent *ScreenChangeEvent = (CVioarrScreenChangeEvent*)Event;
                    _ScreenManager->SetActiveScreen(ScreenChangeEvent->GetType(), true);
                } break;
            }
            delete Event;
        }
        return 0;
    }

    // Queues a new event up
    void QueueEvent(CVioarrEvent *Event) {
        std::unique_lock<std::mutex> _eventlock(_EventMutex);
        _EventQueue.push(Event);
        _EventSignal.notify_one();
    }

private:
    // Functions
    void SpawnMessageHandler();

    // Resources
    CScreenManager*             _ScreenManager;
    CDisplay*                   _Display;
    std::thread*                _MessageThread;
    std::condition_variable     _EventSignal;
    std::queue<CVioarrEvent*>   _EventQueue;
    std::mutex                  _EventMutex;

    // State tracking
    bool                        _IsRunning;
};

// Shorthand for the vioarr
#define sVioarr VioarrCompositor::GetInstance()
