#pragma semicolon 1
#pragma newdecls required

#include <pluginmanager>

static DynamicHook g_hookSetModel;

public void OnPluginStart()
{
	GameData gamedata = new GameData("pluginmanager_test");
	ConVar convar = CreateConVar("sm_pluginmanager_test_enabled", "1", "Enable the plugin?");
	
	// Initialize the plugin manager system.
	// The passed convar handle will be used to determine whether the plugin should be enabled or disabled.
	// The passed gamedata handle will be used in certain functions (check documentation).
	PluginManager.Init(convar, gamedata);
	
	delete gamedata;
	
	// Specify a callback to fire whenever the plugin is enabled or disabled.
	PluginManager.AddPluginToggledHook(OnPluginToggled);
	
	// Add events, detours, etc. to be hooked when the plugin enables
	PluginManager.AddEventHook("player_spawn", OnGameEvent_player_spawn);
	PluginManager.AddDynamicDetour("CTFPlayer::GetLoadoutItem", OnGetLoadoutItemPre, OnGetLoadoutItemPost);
	
	// Dynamic hooks return the DynamicHook handle.
	// You can use this handle directly in DynamicHookEntity, or if you don't want to store it, look up the hook by name (see: OnClientPutInServer).
	g_hookSetModel = PluginManager.AddDynamicHook("CBaseEntity::SetModel");
	
	// You can remove them, too
	PluginManager.RemoveDynamicDetour("CTFPlayer::GetLoadoutItem", OnGetLoadoutItemPre, OnGetLoadoutItemPost);
}

public void OnClientPutInServer(int client)
{
	// You can use the dynamic hook handle here
	PluginManager.DynamicHookEntity(client, g_hookSetModel, Hook_Pre, OnSetModelPre);
	
	// ...or access it by name, in case you don't want to store the hook handles yourself
	PluginManager.DynamicHookEntityByName(client, "CBaseEntity::SetModel", Hook_Pre, OnSetModelPre);
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

static MRESReturn OnGetLoadoutItemPre(DHookParam params)
{
	return MRES_Ignored;
}

static MRESReturn OnGetLoadoutItemPost(DHookParam params)
{
	return MRES_Ignored;
}

static MRESReturn OnSetModelPre(int entity, DHookParam params)
{
	return MRES_Ignored;
}
