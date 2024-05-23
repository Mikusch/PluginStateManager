#include <pluginmanager>

public void OnPluginStart()
{
	GameData gamedata = new GameData("cbasenpc");
	PluginManager.Init(CreateConVar("sm_myplugin_enabled", "1"), gamedata);
	delete gamedata;
	
	PluginManager.AddPluginToggledHook(OnPluginToggled);
	
	PluginManager.AddEventHook("player_spawn", OnGameEvent_player_spawn);
}

void OnPluginToggled(bool enable)
{
	LogMessage("Plugin changed state to [%s]", enable ? "enabled" : "disabled");
}

static void OnGameEvent_player_spawn(Event event, const char[] name, bool dontBroadcast)
{
	int client = GetClientOfUserId(event.GetInt("userid"));
	LogMessage("Player %N spawned", client);
}

public void OnConfigsExecuted()
{
	PluginManager.TogglePluginIfNecessary();
}
