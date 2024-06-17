#pragma semicolon 1
#pragma newdecls required

#include <pluginstatemanager>

static DynamicHook g_hookSetModel;

public void OnPluginStart()
{
	GameData gamedata = new GameData("pluginstatemanager_test");
	
	// Initialize the plugin state manager system.
	// The passed convar name will be used as the trigger to enable/disable the plugin. If the plugin does not create it, one will be created automatically.
	// The passed gamedata handle will be used in certain functions (check documentation).
	PSM_Init("sm_pluginstatemanager_enabled", gamedata);
	delete gamedata;
	
	// You can create the convar before or after calling PSM_Init(), as long as it happens in the same frame.
	CreateConVar("sm_pluginstatemanager_enabled", "1", "Enable the plugin?");
	CreateConVar("sm_pluginstatemanager_waitingforplayers_time", "60", "This convar will be kept in sync with mp_waitingforplayers_time.");
	
	// Specify a callback to fire whenever the plugin is enabled or disabled.
	// Custom logic for enabling/disabling can go there.
	PSM_AddPluginStateChangedHook(OnPluginStateChanged);
	
	// Add events, detours, etc. to be hooked when the plugin enables.
	PSM_AddEventHook("player_spawn", OnGameEvent_player_spawn);
	PSM_AddDynamicDetourFromConf("CTFPlayer::GetLoadoutItem", OnGetLoadoutItemPre, OnGetLoadoutItemPost);
	
	// Dynamic hooks return the DynamicHook handle.
	// You can use this handle directly in DynamicHookEntity, or if you don't want to store it, look up the hook by name (see: OnClientPutInServer).
	g_hookSetModel = PSM_AddDynamicHookFromConf("CBaseEntity::SetModel");
	
	// This forces tf_forced_holiday to always be 2 while the plugin is enabled.
	PSM_AddEnforcedConVar("tf_forced_holiday", "2");
	
	// This forces sm_pluginstatemanager_waitingforplayers_time to always keep the same value as mp_waitingforplayers_time.
	PSM_AddSyncedConVar("sm_pluginstatemanager_waitingforplayers_time", "mp_waitingforplayers_time");
	
	RegConsoleCmd("sm_test", OnTestCommand);
}

public void OnConfigsExecuted()
{
	// This will enable the plugin if sm_pluginstatemanager_enabled is true.
	PSM_TogglePluginState();
}

public void OnPluginEnd()
{
	// This will clean up any hooks that are still active.
	PSM_SetPluginState(false);
}

public void OnClientPutInServer(int client)
{
	// You can use the dynamic hook handle here.
	PSM_DynamicHookEntity(g_hookSetModel, Hook_Pre, client, OnPlayerSetModelPre);
	
	// ...or access it by name, in case you don't want to store the hook handles yourself.
	// (keep in mind that we just hooked the same entity twice)
	PSM_DynamicHookEntityByName("CBaseEntity::SetModel", Hook_Pre, client, OnPlayerSetModelPre);
}

public void OnEntityCreated(int entity, const char[] classname)
{
	if (StrEqual(classname, "prop_dynamic"))
	{
		// This will only be hooked if the plugin is enabled.
		PSM_SDKHook(entity, SDKHook_SpawnPost, OnPropSpawned);
	}
}

static void OnPluginStateChanged(bool enable)
{
	LogMessage("Plugin changed state to [%s]", enable ? "enabled" : "disabled");
	
	if (enable)
	{
		// Re-create our SDKHooks.
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

static Action OnTestCommand(int client, int args)
{
	// Not every single guard clause can be eliminated, but you can check the current plugin state.
	if (!PSM_IsEnabled())
		return Plugin_Continue;
	
	LogMessage("Test!");
	return Plugin_Handled;
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
