#include "game/state/city/agentmission.h"
#include "framework/framework.h"
#include "framework/logger.h"
#include "framework/sound.h"
#include "game/state/agent.h"
#include "game/state/battle/battlecommonsamplelist.h"
#include "game/state/city/building.h"
#include "game/state/city/city.h"
#include "game/state/city/doodad.h"
#include "game/state/city/scenery.h"
#include "game/state/gamestate.h"
#include "game/state/organisation.h"
#include "game/state/rules/scenery_tile_type.h"
#include "game/state/tileview/tile.h"
#include "game/state/tileview/tileobject_doodad.h"
#include "game/state/tileview/tileobject_scenery.h"
#include "game/state/tileview/tileobject_shadow.h"
#include "library/strings_format.h"
#include <glm/glm.hpp>

namespace OpenApoc
{

AgentTileHelper::AgentTileHelper(TileMap &map, Agent &a) : map(map), a(a) {}

bool AgentTileHelper::canEnterTile(Tile *from, Tile *to, bool ignoreStaticUnits,
                                   bool ignoreAllUnits) const
{
	float nothing;
	bool none1;
	bool none2;
	return canEnterTile(from, to, false, none1, nothing, none2, ignoreStaticUnits, ignoreAllUnits);
}

float AgentTileHelper::pathOverheadAlloawnce() const { return 1.25f; }

bool AgentTileHelper::canEnterTile(Tile *from, Tile *to, bool, bool &, float &cost, bool &, bool,
                                   bool) const
{
	if (!from)
	{
		LogError("No 'from' position supplied");
		return false;
	}
	Vec3<int> fromPos = from->position;
	if (!to)
	{
		LogError("No 'to' position supplied");
		return false;
	}
	Vec3<int> toPos = to->position;
	if (fromPos == toPos)
	{
		LogError("FromPos == ToPos %s", toPos.x);
		return false;
	}
	if (!map.tileIsValid(toPos))
	{
		LogError("ToPos %s is not on the map", toPos.x);
		return false;
	}

	auto dir = toPos - fromPos;
	// Agents can only move along one axis
	if (std::abs(dir.x) + std::abs(dir.y) + std::abs(dir.z) > 1)
	{
		return false;
	}

	sp<Scenery> sceneryFrom;
	for (auto &obj : from->ownedObjects)
	{
		if (obj->getType() == TileObject::Type::Scenery)
		{
			sceneryFrom = std::static_pointer_cast<TileObjectScenery>(obj)->scenery.lock();
			break;
		}
	}
	sp<Scenery> sceneryTo;
	for (auto &obj : to->ownedObjects)
	{
		if (obj->getType() == TileObject::Type::Scenery)
		{
			sceneryTo = std::static_pointer_cast<TileObjectScenery>(obj)->scenery.lock();
			break;
		}
	}
	if (!sceneryFrom || !sceneryTo || sceneryFrom->damaged || sceneryTo->damaged ||
	    sceneryFrom->falling || sceneryTo->falling)
	{
		return false;
	}
	int forward = convertDirection(dir);
	// General passability check
	if (!isMoveAllowed(*sceneryFrom, forward) || !isMoveAllowed(*sceneryTo, convertDirection(-dir)))
	{
		return false;
	}
	// If going sideways from junction we can only go into another junction or tube
	if (forward < 4 &&
	    sceneryFrom->type->tile_type == SceneryTileType::TileType::PeopleTubeJunction)
	{
		if (sceneryTo->type->tile_type != SceneryTileType::TileType::PeopleTubeJunction &&
		    sceneryTo->type->tile_type != SceneryTileType::TileType::PeopleTube)
		{
			return false;
		}
	}
	// If going up we can't go to building unless there's a people tube junction up there
	if (forward == 4 && sceneryTo->type->tile_type == SceneryTileType::TileType::General)
	{
		bool foundJunction = false;
		auto checkedPos = sceneryTo->currentPosition;
		do
		{
			checkedPos.z++;
			if (checkedPos.z >= map.size.z)
			{
				break;
			}
			auto checkedTile = map.getTile(checkedPos);
			sp<Scenery> checkedScenery;
			for (auto &obj : checkedTile->ownedObjects)
			{
				if (obj->getType() == TileObject::Type::Scenery)
				{
					checkedScenery =
					    std::static_pointer_cast<TileObjectScenery>(obj)->scenery.lock();
					break;
				}
			}
			if (checkedScenery &&
			    checkedScenery->type->tile_type == SceneryTileType::TileType::PeopleTubeJunction)
			{
				foundJunction = true;
			}
		} while (!foundJunction);
		if (!foundJunction)
		{
			return false;
		}
	}

	cost = glm::length(Vec3<float>{fromPos} - Vec3<float>{toPos});
	return true;
}

float AgentTileHelper::getDistance(Vec3<float> from, Vec3<float> to) const
{
	return glm::length(to - from);
}

float AgentTileHelper::getDistance(Vec3<float> from, Vec3<float> toStart, Vec3<float> toEnd) const
{
	auto diffStart = toStart - from;
	auto diffEnd = toEnd - from - Vec3<float>{1.0f, 1.0f, 1.0f};
	auto xDiff = from.x >= toStart.x && from.x < toEnd.x ? 0.0f : std::min(std::abs(diffStart.x),
	                                                                       std::abs(diffEnd.x));
	auto yDiff = from.y >= toStart.y && from.y < toEnd.y ? 0.0f : std::min(std::abs(diffStart.y),
	                                                                       std::abs(diffEnd.y));
	auto zDiff = from.z >= toStart.z && from.z < toEnd.z ? 0.0f : std::min(std::abs(diffStart.z),
	                                                                       std::abs(diffEnd.z));
	return sqrtf(xDiff * xDiff + yDiff * yDiff + zDiff * zDiff);
}

int AgentTileHelper::convertDirection(Vec3<int> dir) const
{
	// No sanity checks, assuming only one coord is non-zero
	if (dir.y == -1)
	{
		return 0;
	}
	if (dir.x == 1)
	{
		return 1;
	}
	if (dir.y == 1)
	{
		return 2;
	}
	if (dir.x == -1)
	{
		return 3;
	}
	if (dir.z == 1)
	{
		return 4;
	}
	if (dir.z == -1)
	{
		return 5;
	}
	LogError("Impossible to reach here? convertDirection for 0,0,0?");
	return -1;
}

float AgentTileHelper::adjustCost(Vec3<int> nextPosition, int z) const
{
	// Quite unlikely that we ever need to dig
	if (nextPosition.z < MIN_REASONABLE_HEIGHT_AGENT && z == -1)
	{
		return -50.0f;
	}
	return 0;
}

bool AgentTileHelper::isMoveAllowed(Scenery &scenery, int dir) const
{
	switch (scenery.type->tile_type)
	{
		// Can traverse people's tube only according to flags
		case SceneryTileType::TileType::PeopleTube:
			return scenery.type->tube[dir];
		// Can traverse junction according to flags or up/down
		case SceneryTileType::TileType::PeopleTubeJunction:
			return scenery.type->tube[dir] || dir == 4 || dir == 5;
		// Can traverse roads and general only if they are part of a building
		case SceneryTileType::TileType::General:
		case SceneryTileType::TileType::Road:
			return (bool)scenery.building;
		// Cannot traverse walls ever
		case SceneryTileType::TileType::CityWall:
			return false;
	}
	LogError("Unhandled situiation in isMoveAllowed, can't reach here?");
	return false;
}

AgentMission *AgentMission::gotoBuilding(GameState &, Agent &, StateRef<Building> target,
                                         bool allowTeleporter)
{
	auto *mission = new AgentMission();
	mission->type = MissionType::GotoBuilding;
	mission->targetBuilding = target;
	mission->allowTeleporter = allowTeleporter;
	return mission;
}

AgentMission *AgentMission::snooze(GameState &, Agent &, unsigned int snoozeTicks)
{
	auto *mission = new AgentMission();
	mission->type = MissionType::Snooze;
	mission->timeToSnooze = snoozeTicks;
	return mission;
}

AgentMission *AgentMission::restartNextMission(GameState &, Agent &)
{
	auto *mission = new AgentMission();
	mission->type = MissionType::RestartNextMission;
	return mission;
}

AgentMission *AgentMission::awaitPickup(GameState &, Agent &)
{
	auto *mission = new AgentMission();
	mission->type = MissionType::AwaitPickup;
	return mission;
}

AgentMission *AgentMission::teleport(GameState &state, Agent &a, StateRef<Building> b)
{
	auto *mission = new AgentMission();
	mission->type = MissionType::Teleport;
	mission->targetBuilding = b;
	return mission;
}

bool AgentMission::teleportCheck(GameState &state, Agent &a)
{
	if (allowTeleporter && a.canTeleport())
	{
		auto *teleportMission = AgentMission::teleport(state, a, targetBuilding);
		a.missions.emplace_front(teleportMission);
		teleportMission->start(state, a);
		return true;
	}
	return false;
}

bool AgentMission::getNextDestination(GameState &state, Agent &a, Vec3<float> &destPos)
{
	if (cancelled)
	{
		return false;
	}
	switch (this->type)
	{
		case MissionType::GotoBuilding:
		{
			return advanceAlongPath(state, a, destPos);
		}
		case MissionType::Snooze:
		case MissionType::RestartNextMission:
		case MissionType::AwaitPickup:
		{
			return false;
		}
		default:
			LogWarning("TODO: Implement getNextDestination");
			return false;
	}
	return false;
}

void AgentMission::update(GameState &state, Agent &a, unsigned int ticks, bool finished)
{
	finished = finished || isFinishedInternal(state, a);
	switch (this->type)
	{
		case MissionType::GotoBuilding:
		{
			if (finished)
			{
				return;
			}
			if (this->currentPlannedPath.empty())
			{
				if ((Vec3<int>)a.position == targetBuilding->crewQuarters)
				{
					a.enterBuilding(state, targetBuilding);
				}
				else
				{
					LogWarning("Try order a taxi pickup first!");
					setPathTo(state, a, targetBuilding);
				}
			}
			return;
		}
		case MissionType::AwaitPickup:
			LogError("Check if pickup coming or order pickup");
			return;
		case MissionType::RestartNextMission:
			return;
		case MissionType::Snooze:
		{
			if (ticks >= this->timeToSnooze)
				this->timeToSnooze = 0;
			else
				this->timeToSnooze -= ticks;
			return;
		}
		default:
			LogWarning("TODO: Implement update");
			return;
	}
}

bool AgentMission::isFinished(GameState &state, Agent &a, bool callUpdateIfFinished)
{
	if (isFinishedInternal(state, a))
	{
		if (callUpdateIfFinished)
		{
			update(state, a, 0, true);
		}
		return true;
	}
	return false;
}

bool AgentMission::isFinishedInternal(GameState &, Agent &a)
{
	if (cancelled)
	{
		return true;
	}
	switch (this->type)
	{
		case MissionType::GotoBuilding:
			return this->targetBuilding == a.currentBuilding;
		case MissionType::Snooze:
			return this->timeToSnooze == 0;
		case MissionType::AwaitPickup:
			LogError("Implement awaitpickup isfinished");
			return true;
		case MissionType::RestartNextMission:
		case MissionType::Teleport:
			return true;
		default:
			LogWarning("TODO: Implement isFinishedInternal");
			return false;
	}
}

void AgentMission::start(GameState &state, Agent &a)
{
	switch (this->type)
	{
		case MissionType::GotoBuilding:
		{
			if (teleportCheck(state, a))
			{
				return;
			}
			if (targetBuilding->bounds.within(Vec2<int>{a.position.x, a.position.y}))
			{
				a.enterBuilding(state, targetBuilding);
			}
			else if (currentPlannedPath.empty())
			{
				this->setPathTo(state, a, this->targetBuilding);
			}
		}
		case MissionType::Teleport:
		{
			if (!a.canTeleport())
			{
				return;
			}
			a.enterBuilding(state, targetBuilding);
			if (state.battle_common_sample_list->teleport)
			{
				fw().soundBackend->playSample(state.battle_common_sample_list->teleport,
				                              a.position);
			}
			return;
		}
		case MissionType::RestartNextMission:
		case MissionType::Snooze:
			// No setup
			return;
		default:
			LogError("TODO: Implement start");
			return;
	}
}

void AgentMission::setPathTo(GameState &state, Agent &a, StateRef<Building> b)
{
	this->currentPlannedPath.clear();
	auto &map = *a.city->map;
	auto path = map.findShortestPath(a.position, b->crewQuarters, 1000, AgentTileHelper{map, a});
	if (path.empty())
	{
		cancelled = true;
		return;
	}

	// Always start with the current position
	this->currentPlannedPath.push_back(a.position);
	for (auto &p : path)
	{
		this->currentPlannedPath.push_back(p);
	}
}

bool AgentMission::advanceAlongPath(GameState &state, Agent &a, Vec3<float> &destPos)
{
	// Add {0.5,0.5,0.5} to make it route to the center of the tile
	static const Vec3<float> offset{0.5f, 0.5f, 0.5f};
	static const Vec3<float> offsetLand{0.5f, 0.5f, 0.0f};

	if (currentPlannedPath.empty())
	{
		return false;
	}
	currentPlannedPath.pop_front();
	if (currentPlannedPath.empty())
	{
		return false;
	}
	auto pos = currentPlannedPath.front();

	// See if we can actually go there
	auto tFrom = a.city->map->getTile(a.position);
	auto tTo = tFrom->map.getTile(pos);
	if (tFrom->position != pos &&
	    (std::abs(tFrom->position.x - pos.x) > 1 || std::abs(tFrom->position.y - pos.y) > 1 ||
	     std::abs(tFrom->position.z - pos.z) > 1 ||
	     !AgentTileHelper{tFrom->map, a}.canEnterTile(tFrom, tTo)))
	{
		// Next tile became impassable, pick a new path
		currentPlannedPath.clear();
		a.missions.emplace_front(restartNextMission(state, a));
		a.missions.front()->start(state, a);
		return false;
	}

	// See if we can make a shortcut
	// When ordering move to vehidle already on the move, we can have a situation
	// where going directly to 2nd step in the path is faster than going to the first
	// In this case, we should skip unnesecary steps
	auto it = ++currentPlannedPath.begin();
	// Start with position after next
	// If next position has a node and we can go directly to that node
	// Then update current position and iterator
	while (it != currentPlannedPath.end() &&
	       (tFrom->position == *it ||
	        (std::abs(tFrom->position.x - it->x) <= 1 && std::abs(tFrom->position.y - it->y) <= 1 &&
	         std::abs(tFrom->position.z - it->z) <= 1 &&
	         AgentTileHelper{tFrom->map, a}.canEnterTile(tFrom, tFrom->map.getTile(*it)))))
	{
		currentPlannedPath.pop_front();
		pos = currentPlannedPath.front();
		tTo = tFrom->map.getTile(pos);
		it = ++currentPlannedPath.begin();
	}

	destPos = Vec3<float>{pos.x, pos.y, pos.z} + offset;
	return true;
}

UString AgentMission::getName()
{
	static const std::map<AgentMission::MissionType, UString> TypeMap = {
	    {MissionType::GotoBuilding, "GotoBuilding"},
	    {MissionType::Snooze, "Snooze"},
	    {MissionType::AwaitPickup, "AwaitPickup"},
	    {MissionType::RestartNextMission, "RestartNextMission"},
	    {MissionType::Teleport, "Teleport"},
	};
	UString name = "UNKNOWN";
	const auto it = TypeMap.find(this->type);
	if (it != TypeMap.end())
		name = it->second;
	switch (this->type)
	{
		case MissionType::GotoBuilding:
			name += " " + this->targetBuilding.id;
			break;
		case MissionType::Snooze:
			name += format(" for %u ticks", this->timeToSnooze);
			break;
		case MissionType::Teleport:
			name += " " + this->targetBuilding.id;
			break;
		case MissionType::AwaitPickup:
		case MissionType::RestartNextMission:
			break;
	}
	return name;
}

} // namespace OpenApoc