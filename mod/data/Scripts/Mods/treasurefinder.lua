-- Loaded by the game as the mod's init script (scripts/mods/<modid>.lua). The TreasureFinder plugin calls
-- TreasureFinder.Probe() and reads the results back from plain globals (TreasureFinderProbe_<field>, per target
-- TreasureFinderProbe_<field>_<n>): reading a global needs no table references on the C++ side.
TreasureFinder = TreasureFinder or {}
TreasureFinderLoaded = true

-- TreasureFinder.Maps, generated into the pak from research/treasure_maps.json.
Script.ReloadScript("Scripts/Mods/treasurefinder_maps.lua")

-- Whether a destination has been dealt with: a hole dug (Hole.lua: nDepth is 1 once dug) or a container emptied.
-- Destinations with neither (areas, triggers) stay marked while the map is carried.
local function done(entity)
	if entity.nDepth ~= nil then
		return entity.nDepth >= 1
	end
	if entity.inventory then
		return entity.inventory:GetCount() == 0
	end
	return false
end

-- The destination of one map if it is carried and its destination is in the loaded level and not dealt with yet.
-- Entity ids repeat across levels, so the name confirms the match.
local function target(m)
	if player.inventory:GetCountOfClass(m.item) == 0 then
		return nil
	end
	local entity = System.GetEntity(m.entity)
	if not entity or entity:GetName() ~= m.name or done(entity) then
		return nil
	end
	local wuid = XGenAIModule.GetMyWUID(entity)
	if not wuid then
		return nil
	end
	local pos = entity:GetWorldPos()
	return { map = m.map, label = m.label, wuid = wuid, x = pos.x, y = pos.y, z = pos.z }
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
			local ok, result = pcall(target, m)
			if not ok then
				p.errors = (p.errors and (p.errors .. "; ") or "") .. m.map .. ": " .. tostring(result)
			elseif result then
				p.targets[#p.targets + 1] = result
			end
		end
	end
	publish(p)
end
