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
	PM_Init(convar, gamedata);
	
	delete gamedata;
	
	// Specify a callback to fire whenever the plugin is enabled or disabled.
	PM_AddPluginStateChangedHook(OnPluginStateChanged);
	
	// Add events, detours, etc. to be hooked when the plugin enables.
	PM_AddEventHook("player_spawn", OnGameEvent_player_spawn);
	PM_AddDynamicDetourFromConf("CTFPlayer::GetLoadoutItem", OnGetLoadoutItemPre, OnGetLoadoutItemPost);
	
	// Dynamic hooks return the DynamicHook handle.
	// You can use this handle directly in DynamicHookEntity, or if you don't want to store it, look up the hook by name (see: OnClientPutInServer).
	g_hookSetModel = PM_AddDynamicHookFromConf("CBaseEntity::SetModel");
	
	PM_AddEnforcedConVar("tf_forced_holiday", "2");
}

public void OnConfigsExecuted()
{
	// This will enable the plugin if the specified convar's value is true.
	PM_TogglePluginStateIfNeeded();
}

public void OnPluginEnd()
{
	PM_Cleanup();
}

public void OnClientPutInServer(int client)
{
	// You can use the dynamic hook handle here.
	PM_DynamicHookEntity(g_hookSetModel, Hook_Pre, client, OnPlayerSetModelPre);
	
	// ...or access it by name, in case you don't want to store the hook handles yourself.
	PM_DynamicHookEntityFromConf("CBaseEntity::SetModel", Hook_Pre, client, OnPlayerSetModelPre);
	
	// Keep in mind that we just hooked the same entity twice, thus the hook will also fire twice.
}

public void OnEntityCreated(int entity, const char[] classname)
{
	if (StrEqual(classname, "prop_dynamic"))
	{
		// This will only be hooked if the plugin is enabled.
		PM_SDKHook(entity, SDKHook_SpawnPost, OnPropSpawned);
	}
}

static void OnPluginStateChanged(bool enable)
{
	LogMessage("Plugin changed state to [%s]", enable ? "enabled" : "disabled");
	
	if (enable)
	{
		// Re-create our SDKHooks
		int entity = -1;
		while ((entity = FindEntityByClassname(entity, "*")) != -1)
		{
			char classname[64];
			if (!GetEntityClassname(entity, classname, sizeof(classname)))
				continue;
			
			OnEntityCreated(entity, classname);
		}
	}
}

static void OnPropSpawned(int beam)
{
	LogMessage("prop_dynamic (%d) spawned!", beam);
}

static void OnGameEvent_player_spawn(Event event, const char[] name, bool dontBroadcast)
{
	int client = GetClientOfUserId(event.GetInt("userid"));
	LogMessage("Player %N spawned", client);
}

static MRESReturn OnGetLoadoutItemPre(int player, DHookReturn ret, DHookParam params)
{
	LogMessage("Getting Loadout item of %N", player);
	return MRES_Ignored;
}

static MRESReturn OnGetLoadoutItemPost(int player, DHookReturn ret, DHookParam params)
{
	return MRES_Ignored;
}

static MRESReturn OnPlayerSetModelPre(int client, DHookParam params)
{
	char model[PLATFORM_MAX_PATH];
	params.GetString(1, model, sizeof(model));
	
	LogMessage("Player %N changed model to %s", client, model);
	
	return MRES_Ignored;
}
