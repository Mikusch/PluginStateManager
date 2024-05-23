# Plugin Manager

### An include for SourceMod plugins that allows for easy plugin enabling and disabling.

SourceMod developers are all too familiar with this kind of code:

```sourcepawn
void OnGameEvent_player_spawn(Event event, const char[] name, bool dontBroadcast)
{
    if (!g_bEnabled)
        return;
    
    // ...
}
```

As plugin developers, we have to make sure that our disabled plugins don't actually do anything, and the easiest way to accomplish that is to check for the enabled state in every callback we create. This not only ends up being annoying and error-prone, but also wastes CPU cycles, as the hooks still get called by SourceMod.

A while ago I wrote a pretty robust system for enabling and disabling a plugin, making sure that no trace is left behind when a plugin is disabled. I ended up copy-pasting it into every plugin and grew pretty tired of that, so I opted to  make it an include.

```sourcepawn
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
```

Since this is an include, all the resources allocated will belong to your plugin.

When `sm_myplugin_enabled` is set to `0`, everything you added will be completely unhooked, meaning you don't have to check if the plugin is enabled in each callback.