# Plugin State Manager

### An include for SourceMod plugins that aims to reduce boilerplate code for enabling and disabling.

> [!NOTE]  
> This include was mostly made for my own personal use, so it may be a bit rough around the edges.

Many plugin developers are familiar with the good old "is enabled" guard clause at the top of every single function:

```sourcepawn
void OnGameEvent_player_spawn(Event event, const char[] name, bool dontBroadcast)
{
    if (!g_bEnabled)
        return;
    
    // ...
}
```

This approach has several flaws, since it is not only is very annoying to write, but also very easy to forget. Most importantly, it wastes resources since hooks are still called even if the plugin is disabled.

I ended up writing a pretty robust system for enabling and disabling plugins, which I've copied into pretty much all my plugins.

## Usage

Certain functions need to be called in SourceMod forwards to properly use this include.

You generally want to call `PSM_Init` in `OnPluginStart`, `PSM_TogglePluginState` in `OnConfigsExecuted`, and `PSM_Disable` in `OnPluginEnd`. For a full example utilizing most of the features, see [here](https://github.com/Mikusch/PluginStateManager/blob/master/addons/sourcemod/scripting/pluginstatemanager_test.sp).

```sourcepawn
#include <pluginstatemanager>

public void OnPluginStart()
{
	CreateConVar("sm_myplugin_enabled", "1", "Enable the plugin?");
	
	// Initialize the state manager.
	PSM_Init("sm_myplugin_enabled");
	
	// Add events, detours, etc. to be hooked when the plugin enables.
	PSM_AddEventHook("player_spawn", OnGameEvent_player_spawn);
	PSM_AddEnforcedConVar("tf_forced_holiday", "2");
}

public void OnConfigsExecuted()
{
	// This will enable the plugin if sm_myplugin_enabled is true.
	PSM_TogglePluginState();
}

public void OnPluginEnd()
{
	// This will disable the plugin and remove any hooks that are still active.
	PSM_Disable();
}
```