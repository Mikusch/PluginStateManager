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

Writing "is enabled" checks into every callback can be a pain, and it's easy to forget to add them. Furthermore, they can be a performance hit if you have a lot of them.

This include aims to make it easier to write plugins that can be enabled and disabled at runtime. It provides a simple API to hook and unhook callbacks, and automatically handles enabling and disabling the plugin.

```sourcepawn
#include <pluginmanager>

public void OnPluginStart()
{
	// Have the plugin be managed by the sm_myplugin_enabled convar
	ConVar convar = CreateConVar("sm_myplugin_enabled", "1", "Enable the plugin?");
	PluginManager.Init(convar);
	
	// Register your callbacks
	PluginManager.AddEventHook("player_spawn", OnGameEvent_player_spawn);
	PluginManager.AddPluginToggledHook(OnPluginToggled);
}

public void OnConfigsExecuted()
{
	// Automatically enable or disable the plugin based on the server configuration
	PluginManager.TogglePluginIfNecessary();
}

// This callback will unhook itself if the plugin gets disabled via "sm_myplugin_enabled"
static void OnGameEvent_player_spawn(Event event, const char[] name, bool dontBroadcast)
{
	LogMessage("Player spawned");
}

static void OnPluginToggled(bool enabled)
{
	// Custom logic here
}
```

When `sm_myplugin_enabled` is set to `0`, everything you added will be completely unhooked, meaning you don't have to check if the plugin is enabled in each callback. Setting it back to `1` will re-enable everything. Simple, right?

This is **completely standalone** - no need to install any plugins or extensions. Just include the file and you're good to go.

For more examples, check out the [example plugin](https://github.com/Mikusch/pluginmanager/blob/master/addons/sourcemod/scripting/pluginmanager_test.sp).