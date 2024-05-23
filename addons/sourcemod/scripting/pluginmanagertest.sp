#pragma semicolon 1
#pragma newdecls required

#include <pluginmanager>

public void OnPluginStart()
{
	GameData gamedata = new GameData("pluginmanager");
	PluginManager.Init(CreateConVar("sm_pluginmanager_enabled", "1"), gamedata);
	delete gamedata;
	
	PluginManager.AddPluginToggledHook(OnPluginToggled);
	
	// Add events, detours, etc. to be hooked when the plugin enables
	PluginManager.AddEventHook("player_spawn", OnGameEvent_player_spawn);
	PluginManager.AddDynamicDetour("CTFPlayer::GetLoadoutItem", OnGetLoadoutItem);
}

public void OnConfigsExecuted()
{
	// This will enable the plugin if the specified convar's value is true
	PluginManager.TogglePluginIfNecessary();
}

static void OnPluginToggled(bool enable)
{
	LogMessage("Plugin changed state to [%s]", enable ? "enabled" : "disabled");
}

static void OnGameEvent_player_spawn(Event event, const char[] name, bool dontBroadcast)
{
	LogMessage("Player spawned");
}

static MRESReturn OnGetLoadoutItem(DHookParam params)
{
	LogMessage("Getting player's loadout item");
	return MRES_Ignored;
}
