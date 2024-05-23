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

Writing "is enabled" checks into every callback can be a pain, and it's easy to forget to add them.

This include aims to make it easier to enable and disable plugins, and to save on resources.

```sourcepawn
#include <pluginmanager>

public void OnPluginStart()
{
	// Initialize the plugin manager
	PluginManager.Init(CreateConVar("sm_myplugin_enabled", "1", "Enable the plugin?"));
	
	// Register your callbacks
	PluginManager.AddEventHook("player_spawn", OnGameEvent_player_spawn);
}

public void OnConfigsExecuted()
{
	// Automatically enable or disable the plugin based on the server configuration
	PluginManager.TogglePluginIfNecessary();
}

static void OnGameEvent_player_spawn(Event event, const char[] name, bool dontBroadcast)
{
	LogMessage("Player spawned");
}
```

When `sm_myplugin_enabled` is set to `0`, everything you added will be completely unhooked, meaning you don't have to check if the plugin is enabled in each callback.

This is **completely standalone** - no need to install any plugins or extensions. Just include the file and you're good to go.

For more examples, check out the [example plugin](https://github.com/Mikusch/pluginmanager/blob/master/addons/sourcemod/scripting/pluginmanager_test.sp).