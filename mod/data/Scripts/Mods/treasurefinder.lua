-- Loaded by the game as the mod's init script (scripts/mods/<modid>.lua). The TreasureFinder plugin calls
-- TreasureFinder.Probe() and reads the results back from plain globals (TreasureFinderProbe_<field>, per target
-- TreasureFinderProbe_<field>_<n>): reading a global needs no table references on the C++ side.
TreasureFinder = TreasureFinder or {}
TreasureFinderLoaded = true

-- TreasureFinder.Maps, generated into the pak from research/treasure_maps.json.
Script.ReloadScript("Scripts/Mods/treasurefinder_maps.lua")

-- Whether a destination has been dealt with: a hole dug (Hole.lua: nDepth is 1 once dug) or a container emptied.
-- Destinations with neither (loose items in item slots, areas, triggers) stay marked while the map is carried.
local function done(entity)
	if entity.nDepth ~= nil then
		return entity.nDepth >= 1
	end
	if entity.inventory then
		return entity.inventory:GetCount() == 0
	end
	return false
end

-- Whether the player has a map: carries its item, or, for a map carved into a wall, has examined it (its one-shot
-- quest trigger stops being enabled once used; it is also off before the open world unlocks).
local function hasMap(m)
	if m.item then
		return player.inventory:GetCountOfClass(m.item) > 0
	end
	local trigger = System.GetEntityByName(m.seen)
	return trigger ~= nil and not trigger.interactionTrigger:IsPressEnabled()
end

-- One place a map leads to, if it is in the loaded level and not dealt with yet. Entity ids repeat across levels, so
-- the name confirms the match.
local function place(m, t)
	local entity = System.GetEntity(t.entity)
	if not entity or entity:GetName() ~= t.name or done(entity) then
		return nil
	end
	local wuid = XGenAIModule.GetMyWUID(entity)
	if not wuid then
		return nil
	end
	local pos = entity:GetWorldPos()
	return { map = m.map, label = m.label, wuid = wuid, x = pos.x, y = pos.y, z = pos.z }
end

-- Adds every open place of a map the player has to `targets`.
local function addTargets(m, targets)
	if not hasMap(m) then
		return
	end
	for _, t in ipairs(m.targets) do
		local found = place(m, t)
		if found then
			targets[#targets + 1] = found
		end
	end
end

local function publish(p)
	TreasureFinderProbe_ran = true
	TreasureFinderProbe_playerReady = p.playerReady
	TreasureFinderProbe_count = #p.targets
	TreasureFinderProbe_errors = p.errors
	local names = {}
	for n, t in ipairs(p.targets) do
		_G["TreasureFinderProbe_map_" .. n] = t.map
		_G["TreasureFinderProbe_label_" .. n] = t.label
		_G["TreasureFinderProbe_wuid_" .. n] = t.wuid
		_G["TreasureFinderProbe_x_" .. n] = t.x
		_G["TreasureFinderProbe_y_" .. n] = t.y
		_G["TreasureFinderProbe_z_" .. n] = t.z
		names[n] = t.map
	end
	-- Logged from Lua as well, so the log shows the probe running even if the plugin cannot read the results.
	local summary = "player="
		.. tostring(p.playerReady)
		.. " targets="
		.. table.concat(names, ",")
		.. " errors="
		.. tostring(p.errors)
	if summary ~= TreasureFinder.lastSummary then
		TreasureFinder.lastSummary = summary
		System.LogAlways("[TreasureFinder] lua probe: " .. summary)
	end
end

function TreasureFinder.Probe()
	local p = { playerReady = false, targets = {} }
	if player and player.inventory then
		p.playerReady = true
		-- One failing map is recorded in p.errors and does not stop the others.
		for _, m in ipairs(TreasureFinder.Maps) do
			local ok, err = pcall(addTargets, m, p.targets)
			if not ok then
				p.errors = (p.errors and (p.errors .. "; ") or "") .. m.map .. ": " .. tostring(err)
			end
		end
	end
	publish(p)
end
