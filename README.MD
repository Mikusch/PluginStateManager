# Plugin State Manager

### An include for SourceMod plugins that aims to reduce boilerplate code for enabling and disabling.

**[➔ Download include](/addons/sourcemod/scripting/include/pluginstatemanager.inc)**

> [!WARNING]  
> This was created primarily for personal use.
> I've done my best to document it, but I'm not responsible if you misuse the include and cause memory leaks in your plugins.

This kind of code is quite common in callbacks:

```sourcepawn
void OnGameEvent_player_spawn(Event event, const char[] name, bool dontBroadcast)
{
    if (!g_bEnabled)
        return;
    
    // ...
}
```

I got tired of putting "is enabled" guard clauses at the top of every function, so I built a robust system that automatically unhooks and cleans everything up when the plugin is disabled.

To avoid copy-pasting the solution into every plugin I write, I turned it into an include.

## Usage

Due to the nature of include files, a few functions need to be called in SourceMod forwards. Here is a minimal example:

```sourcepawn
#include <pluginstatemanager>

public void OnPluginStart()
{
	// Initialize the plugin state manager.
	PSM_Init("sm_example_enabled");
	
	// Add events, convars, detours, etc. to be hooked when the plugin enables.
	PSM_AddEventHook("player_spawn", OnGameEvent_player_spawn);
	PSM_AddEnforcedConVar("tf_forced_holiday", "2");
	
	// ...
}

public void OnConfigsExecuted()
{
	// This will enable the plugin if the convar value of 'sm_example_enabled' is 1.
	PSM_TogglePluginState();
}

public void OnPluginEnd()
{
	// This will disable the plugin and remove any hooks that are still active.
	PSM_SetPluginState(false);
}
```

> [!NOTE]  
> This doesn't eliminate the "is enabled" guard clauses entirely.
> Forwards and command callbacks still require them, since they can't be hooked or unhooked at runtime.

For more detailed examples, check out some of my plugins utilizing the include:

* [Mann in the Machine](https://github.com/Mikusch/MannInTheMachine)
* [Deathrun Neu](https://github.com/Mikusch/deathrun)
* [Fixed Friendly Fire](https://github.com/Mikusch/friendlyfire)
