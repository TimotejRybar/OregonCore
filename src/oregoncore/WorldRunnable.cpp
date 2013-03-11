/*
 * Copyright (C) 2010-2012 OregonCore <http://www.oregoncore.com/>
 * Copyright (C) 2008-2012 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2012 MaNGOS <http://getmangos.com/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "WorldSocketMgr.h"
#include "Common.h"
#include "World.h"
#include "WorldRunnable.h"
#include "Timer.h"
#include "ObjectAccessor.h"
#include "MapManager.h"
#include "BattleGroundMgr.h"
#include "Database/DatabaseEnv.h"

#define WORLD_SLEEP_CONST 50

// Heartbeat for the World
void WorldRunnable::run()
{
    // Init new SQL thread for the world database
    WorldDatabase.ThreadStart(); // let thread do safe mySQL requests (one connection call enough)
    sWorld.InitResultQueue();

    uint32 real_curr_time = 0;
    uint32 real_prev_tTime = getMSTime();
    uint32 prev_sleep_time = 0; // used for balanced full tick time length near WORLD_SLEEP_CONST

    // While we have not World::m_stopEvent, update the world
    while (!World::IsStopped())
    {
        ++World::m_worldLoopCounter;
        real_curr_time = getMSTime();

        uint32 diff = getMSTimeDiff(real_prev_tTime, real_curr_time);

        sWorld.Update(diff);
        real_prev_tTime = real_curr_time;

        // diff (D0) include time of previous sleep (d0) + tick time (t0)
        // We want that next d1 + t1 == WORLD_SLEEP_CONST
        // We can't know next t1 and then can use (t0 + d1) == WORLD_SLEEP_CONST requirement
        // d1 = WORLD_SLEEP_CONST - t0 = WORLD_SLEEP_CONST - (D0 - d0) = WORLD_SLEEP_CONST + d0 - D0
        if (diff <= WORLD_SLEEP_CONST + prev_sleep_time)
        {
            prev_sleep_time = WORLD_SLEEP_CONST + prev_sleep_time-diff;
            ACE_Based::Thread::Sleep(prev_sleep_time);
        }
        else
            prev_sleep_time = 0;
    }

    sWorld.KickAll(); // Save and kick all players
    sWorld.UpdateSessions(1); // Real players unload required UpdateSessions call

    // Unload battleground templates before different singletons destroyed
    sBattleGroundMgr.DeleteAlllBattleGrounds();

    sWorldSocketMgr->StopNetwork();

    MapManager::Instance().UnloadAll(); // Unload all grids (including locked in memory)

    // End the database thread
    WorldDatabase.ThreadEnd(); // Free mySQL thread resources
}