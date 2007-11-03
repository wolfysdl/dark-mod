/***************************************************************************
 *
 * PROJECT: The Dark Mod
 * $Revision: 1435 $
 * $Date: 2007-10-16 18:53:28 +0200 (Di, 16 Okt 2007) $
 * $Author: angua $
 *
 ***************************************************************************/

#include "../idlib/precompiled.h"
#pragma hdrstop

static bool init_version = FileVersionList("$Id: FleeTask.cpp 1435 2007-10-16 16:53:28Z greebo $", init_version);

#include "FleeTask.h"
#include "../Memory.h"
#include "../Library.h"

namespace ai
{

// Get the name of this task
const idStr& FleeTask::GetName() const
{
	static idStr _name(TASK_FLEE);
	return _name;
}

void FleeTask::Init(idAI* owner, Subsystem& subsystem)
{
	// Init the base class
	Task::Init(owner, subsystem);
	_fleeStartFrame = gameLocal.framenum;

	_enemy = owner->GetEnemy();
	idActor* enemy = _enemy.GetEntity();

	Memory& memory = owner->GetMemory();
	memory.fleeingDone = false;
	owner->AI_MOVE_DONE = false;
	owner->AI_RUN = true;
	_oldPosition = owner->GetPhysics()->GetOrigin();
	
	_escapeSearchLevel = 3; // 3 means FIND_FRIENDLY_GUARDED
	 _distOpt = DIST_NEAREST;
	_failureCount = 0; // This is used for _escapeLevel 1 only
}

bool FleeTask::Perform(Subsystem& subsystem)
{
	DM_LOG(LC_AI, LT_INFO).LogString("Flee Task performing.\r");

	idAI* owner = _owner.GetEntity();
	assert(owner != NULL);
	Memory& memory = owner->GetMemory();
	idActor* enemy = _enemy.GetEntity();
	assert(enemy != NULL);


	gameRenderWorld->DrawText( va("%d  %d",_escapeSearchLevel, _distOpt), owner->GetPhysics()->GetAbsBounds().GetCenter(), 
		1.0f, colorWhite, gameLocal.GetLocalPlayer()->viewAngles.ToMat3(), 1, gameLocal.msec );

	if (_failureCount > 5 || (owner->AI_MOVE_DONE && !owner->AI_DEST_UNREACHABLE))
	{
		owner->StopMove(MOVE_STATUS_DONE);
		//Fleeing is done, check if we can see the enemy
		if (owner->AI_ENEMY_VISIBLE)
		{
			if (_distOpt == DIST_NEAREST)
			{
				// Find fleepoint far away
				_distOpt = DIST_FARTHEST;
				_escapeSearchLevel = 3;
			}
			else
			{
				if (_escapeSearchLevel > 1)
				{
					_escapeSearchLevel --;
				}
			}
		}
		else
		{
			memory.fleeingDone = true;
			return true;
		}
	}

	
	if (owner->GetMoveStatus() == MOVE_STATUS_MOVING)
	{
		if (gameLocal.framenum > _fleeStartFrame && gameLocal.framenum % 5 == 0)
		{
			idVec3 newPosition = owner->GetPhysics()->GetOrigin();
			if (newPosition == _oldPosition)
			{
			//	owner->StopMove(MOVE_STATUS_DEST_UNREACHABLE);
			}
		}
	}
	else
	{
		owner->AI_RUN = true;
		if (_escapeSearchLevel >= 3)
		{
			DM_LOG(LC_AI, LT_INFO).LogString("Trying to find escape route - FIND_FRIENDLY_GUARDED.");
			// Flee to the nearest friendly guarded escape point
			owner->Flee(enemy, FIND_FRIENDLY_GUARDED, _distOpt);
			_fleeStartFrame = gameLocal.framenum;
		}

		else if (_escapeSearchLevel == 2)
		{
			// Try to find another escape route
			DM_LOG(LC_AI, LT_INFO).LogString("Trying alternate escape route - FIND_FRIENDLY.");
			// Find another escape route to ANY friendly escape point
			owner->Flee(enemy, FIND_FRIENDLY, _distOpt);
		}
		else
		{
			DM_LOG(LC_AI, LT_INFO).LogString("Searchlevel = 1, ZOMG, Panic mode, gotta run now!");

			// Get the distance to the enemy
			float enemyDistance = owner->TravelDistance(owner->GetPhysics()->GetOrigin(), enemy->GetPhysics()->GetOrigin());

			DM_LOG(LC_AI, LT_INFO).LogString("Enemy is as near as %d", enemyDistance);
			if (enemyDistance < 500)
			{
				// Increase the fleeRadius (the nearer the enemy, the more)
				// The enemy is still near, run further
				if (!owner->Flee(enemy, FIND_AAS_AREA_FAR_FROM_THREAT, 500))
				{
					// No point could be found.
					_failureCount++;
				}
			}
			else
			{
				// Fleeing is done for now
				owner->AI_MOVE_DONE = true;
				owner->AI_DEST_UNREACHABLE = false;
			}
		}
	}
	

	if (owner->AI_DEST_UNREACHABLE && _escapeSearchLevel > 1)
	{
		_escapeSearchLevel --;
	}

	return false; // not finished yet
}

void FleeTask::Save(idSaveGame* savefile) const
{
	Task::Save(savefile);

	savefile->WriteInt(_escapeSearchLevel);
	savefile->WriteInt(_failureCount);
	savefile->WriteVec3(_oldPosition);
	savefile->WriteInt(_fleeStartFrame);

	savefile->WriteInt(static_cast<int>(_distOpt));

	_enemy.Save(savefile);
}

void FleeTask::Restore(idRestoreGame* savefile)
{
	Task::Restore(savefile);

	savefile->ReadInt(_escapeSearchLevel);
	savefile->ReadInt(_failureCount);
	savefile->ReadVec3(_oldPosition);
	savefile->ReadInt(_fleeStartFrame);

	int distOptInt;
	savefile->ReadInt(distOptInt);
	_distOpt = static_cast<EscapeDistanceOption>(distOptInt);

	_enemy.Restore(savefile);
}

FleeTaskPtr FleeTask::CreateInstance()
{
	return FleeTaskPtr(new FleeTask);
}

// Register this task with the TaskLibrary
TaskLibrary::Registrar fleeTaskRegistrar(
	TASK_FLEE, // Task Name
	TaskLibrary::CreateInstanceFunc(&FleeTask::CreateInstance) // Instance creation callback
);

} // namespace ai
