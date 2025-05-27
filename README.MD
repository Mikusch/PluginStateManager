# Plugin State Manager

### An include for SourceMod plugins that aims to reduce boilerplate code for enabling and disabling.

#### [➔ Download include](/addons/sourcemod/scripting/include/pluginstatemanager.inc)

> [!WARNING]  
> This was made primarily for personal use. It may not be suitable for all plugins, and may change at any moment.

This kind of code is quite common:

```sourcepawn
void OnGameEvent_player_spawn(Event event, const char[] name, bool dontBroadcast)
{
    if (!g_bEnabled)
        return;
    
    // ...
}
```

I grew tired of putting "is enabled" guard clauses at the top of every function, so I wrote a robust system that will automatically unhook and clean up everything if the plugin gets disabled.

To avoid copy-pasting my solution into every plugin I wrote, I turned it into an include.

## Usage

Due to the nature of include files, a few functions need to be called in SourceMod forwards. Here is a minimal example:

```sourcepawn
#include <pluginstatemanager>

public void OnPluginStart()
{
	// Initialize the plugin state manager.
	PSM_Init("sm_myplugin_enabled");
	
	// Add events, detours, etc. to be hooked when the plugin enables.
	PSM_AddEventHook("player_spawn", OnGameEvent_player_spawn);
	PSM_AddEnforcedConVar("tf_forced_holiday", "2");
}

public void OnConfigsExecuted()
{
	// This will enable the plugin if the value of sm_myplugin_enabled is 1.
	PSM_TogglePluginState();
}

public void OnPluginEnd()
{
	// This will disable the plugin and remove any hooks that are still active.
	PSM_SetPluginState(false);
}
```

For a more detailed example, see the [example plugin](https://github.com/Mikusch/PluginStateManager/blob/master/addons/sourcemod/scripting/psm_test.sp).
