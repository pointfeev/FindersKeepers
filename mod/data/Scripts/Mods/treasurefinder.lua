-- Loaded by the game as the mod's init script (scripts/mods/<modid>.lua). The TreasureFinder plugin calls
-- TreasureFinder.Probe() and reads the results back from plain globals (TreasureFinderProbe_<field>): reading a
-- global needs no table references on the C++ side.
TreasureFinder = TreasureFinder or {}
TreasureFinderLoaded = true

-- Test target: the Vidlak bandit's map (poi_vyplavenyPoklad_map) and the washed-up chest it leads to.
local MAP_CLASS = "84286824-882c-48a0-b261-e5ccea6929f6"
local CHEST_ID = 15785
local CHEST_NAME = "stash[Chest/chest29_11fe2aef-9a55-48b3-8ec5-e33b9e1d1197]"

local FIELDS = {
	"ran",
	"playerReady",
	"mapCount",
	"foundById",
	"foundByName",
	"sameEntity",
	"x",
	"y",
	"z",
	"itemCount",
	"wuid",
	"errors",
}

local function publish(p)
	for _, field in ipairs(FIELDS) do
		_G["TreasureFinderProbe_" .. field] = p[field]
	end
	-- Logged from Lua as well, so the log shows the probe running even if the plugin cannot read the results.
	local summary = "player="
		.. tostring(p.playerReady)
		.. " mapCount="
		.. tostring(p.mapCount)
		.. " byId="
		.. tostring(p.foundById)
		.. " byName="
		.. tostring(p.foundByName)
		.. " items="
		.. tostring(p.itemCount)
		.. " wuid="
		.. tostring(p.wuid)
		.. " errors="
		.. tostring(p.errors)
	if summary ~= TreasureFinder.lastSummary then
		TreasureFinder.lastSummary = summary
		System.LogAlways("[TreasureFinder] lua probe: " .. summary)
	end
end

-- Runs one game call; a failure is recorded in p.errors instead of aborting the probe, so every step reports.
local function try(p, label, fn, ...)
	local ok, result = pcall(fn, ...)
	if ok then
		return result
	end
	p.errors = (p.errors and (p.errors .. "; ") or "") .. label .. ": " .. tostring(result)
	return nil
end

local function probe(p)
	if not player or not player.inventory then
		return
	end
	p.playerReady = true
	p.mapCount = try(p, "GetCountOfClass", function()
		return player.inventory:GetCountOfClass(MAP_CLASS)
	end)

	-- Level-file entity ids may or may not survive as runtime ids, so both lookups are reported.
	local byId = try(p, "GetEntity", System.GetEntity, CHEST_ID)
	local byName = try(p, "GetEntityByName", System.GetEntityByName, CHEST_NAME)
	p.foundById = byId ~= nil
	p.foundByName = byName ~= nil
	p.sameEntity = byId ~= nil and byName ~= nil and byId.id == byName.id

	local chest = byName or byId
	if not chest then
		return
	end
	local pos = chest:GetWorldPos()
	p.x, p.y, p.z = pos.x, pos.y, pos.z
	if chest.inventory then
		p.itemCount = try(p, "GetCount", function()
			return chest.inventory:GetCount()
		end)
	end
	p.wuid = try(p, "GetMyWUID", XGenAIModule.GetMyWUID, chest)
end

function TreasureFinder.Probe()
	local p = { ran = true, playerReady = false }
	local ok, err = pcall(probe, p)
	if not ok then
		p.errors = (p.errors and (p.errors .. "; ") or "") .. "probe: " .. tostring(err)
	end
	publish(p)
end
