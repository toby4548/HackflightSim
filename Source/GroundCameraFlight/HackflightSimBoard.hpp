/*
   HackflihtSimBoard.hpp: Hackflight SimBoard class implementation for Unreal Engine

   Provides Board::outbuf() method that displays debugging text to the game display

   Copyright (C) Simon D. Levy 2017

   This file is part of HackflightSim.

   HackflightSim is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   HackflightSim is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   You should have received a copy of the GNU General Public License
   along with Hackflight.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <hackflight.hpp>
#include <boards/sim/sim.hpp>

#include "Engine/Engine.h"


#ifdef _WIN32
#include <boards/sim/windows.hpp>
#else
#include <boards/sim/linux.hpp>
#endif

static FColor TEXT_COLOR = FColor::Yellow;
static float  TEXT_SCALE = 2.f;

void hf::Board::outbuf(char * buf)
{
	if (GEngine) {

		// 0 = overwrite; 5.0f = arbitrary time to display
		GEngine->AddOnScreenDebugMessage(0, 5.0f, TEXT_COLOR, FString(buf), true, FVector2D(TEXT_SCALE,TEXT_SCALE));
	}

}


