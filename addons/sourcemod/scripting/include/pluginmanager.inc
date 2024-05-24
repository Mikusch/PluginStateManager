#pragma semicolon 1
#pragma newdecls required

#include <dhooks>
#include <sdkhooks>

typedef PluginStateChangedCallback = function void(bool enabled);

enum struct EventData
{
	char name[32];
	EventHook callback;
	EventHookMode mode;
}

enum struct DetourData
{
	char name[64];
	DynamicDetour detour;
	DHookCallback callbackPre;
	DHookCallback callbackPost;
}

enum struct DHookData
{
	char name[64];
	DynamicHook hook;
}

enum struct DHookIdData
{
	int hookid;
	DynamicHook hook;
}

enum struct SDKHookData
{
	int ref;
	SDKHookType type;
	SDKHookCB callback;
}

enum struct ConVarData
{
	char name[64];
	char value[64];
	char initialValue[64];
	ConVar relatedConVar;
}

static bool s_isInitialized;
static bool s_isEnabled;
static ConVar s_stateConvar;
static GameData s_gamedata;
static PrivateForward s_toggledFwd;

static ArrayList s_events; // ArrayList<EventData>
static ArrayList s_detours; // ArrayList <DetourData>
static ArrayList s_dynamicHooks; // ArrayList<DHookData>
static ArrayList s_dynamicHookIds; // ArrayList<DHookIdData>
static ArrayList s_sdkHooks; // ArrayList<SDKHookData>
static ArrayList s_convars; // ArrayList<ConVarData>

//=============================================================================
// Core Features
//=============================================================================

/**
 * Initializes the plugin manager.
 * This function should be called in the OnPluginStart forward.
 *
 * @param convar		The convar used to determine whether the plugin should be enabled.
 * @param gamedata		Optional gamedata handle.
 */
stock void PM_Init(ConVar convar, GameData gamedata = null)
{
	if (s_isInitialized)
		return;
	
	s_stateConvar = convar;
	s_stateConvar.AddChangeHook(OnEnableConVarChanged);
	
	if (gamedata)
		s_gamedata = view_as<GameData>(CloneHandle(gamedata));
	
	s_toggledFwd = new PrivateForward(ET_Ignore, Param_Cell);
	s_events = new ArrayList(sizeof(EventData));
	s_detours = new ArrayList(sizeof(DetourData));
	s_dynamicHooks = new ArrayList(sizeof(DHookData));
	s_dynamicHookIds = new ArrayList(sizeof(DHookIdData));
	s_sdkHooks = new ArrayList(sizeof(SDKHookData));
	s_convars = new ArrayList(sizeof(ConVarData));
	
	s_isInitialized = true;
}

/**
 * Convenience function that disables the plugin if it is enabled. Useful to clean up anything set by the plugin manager on plugin end.
 * This function should be called in the OnPluginEnd forward.
 *
 * @error Plugin manager not initialized.
 */
stock void PM_Cleanup()
{
	ThrowIfNotInitialized();
	
	if (PM_IsEnabled())
		PM_SetPluginState(false);
}

/**
 * Convenience function that toggles the enabled state of the plugin if it isn't in that state already.
 * This function should be called in the OnConfigsExecuted forward.
 *
 * @error Plugin manager not initialized.
 */
stock void PM_TogglePluginStateIfNeeded()
{
	ThrowIfNotInitialized();
	
	if (PM_IsEnabled() != s_stateConvar.BoolValue)
	{
		PM_SetPluginState(s_stateConvar.BoolValue);
	}
}

/**
 * Sets the enabled state of the plugin.
 * Most plugins should use PM_TogglePluginStateIfNeeded to respect the convar.
 *
 * @param enable		If true, the plugin will be enabled. If false, the plugin will be disabled.
 * @error				Plugin manager not initialized.
 */
stock void PM_SetPluginState(bool enable)
{
	ThrowIfNotInitialized();
	
	if (PM_IsEnabled() == enable)
		return;
	
	s_isEnabled = enable;
	
	// Events
	for (int i = 0; i < s_events.Length; i++)
	{
		EventData data;
		if (s_events.GetArray(i, data))
		{
			ToggleEvent(data, enable);
		}
	}
	
	// Detours
	for (int i = 0; i < s_detours.Length; i++)
	{
		DetourData data;
		if (s_detours.GetArray(i, data))
		{
			ToggleDetour(data, enable);
		}
	}
	
	if (!enable)
	{
		// Dynamic Hooks
		for (int i = s_dynamicHookIds.Length - 1; i >= 0; i--)
		{
			int hookid = s_dynamicHookIds.Get(i, DHookIdData::hookid);
			DynamicHook.RemoveHook(hookid);
		}
		
		// SDKHooks
		int entity = -1;
		while ((entity = FindEntityByClassname(entity, "*")) != -1)
		{
			PM_SDKUnhook(entity);
		}
	}
	
	for (int i = 0; i < s_convars.Length; i++)
	{
		ToggleConVar(i, enable);
	}
	
	Call_StartForward(s_toggledFwd);
	Call_PushCell(enable);
	Call_Finish();
}

/**
 * Registers a callback function that is called when the state of the plugin changes.
 *
 * @param func		A PluginStateChangedCallback function pointer.
 * @return			True on success, false otherwise.
 * @error			Plugin manager not initialized.
 */
stock bool PM_AddPluginStateChangedHook(PluginStateChangedCallback func)
{
	ThrowIfNotInitialized();
	
	return s_toggledFwd.AddFunction(null, func);
}

//=============================================================================
// Events
//=============================================================================

/**
 * Registers an event to be hooked when the plugin gets enabled.
 *
 * @param name			Name of event.
 * @param callback		An EventHook function pointer.
 * @param mode			Optional EventHookMode determining the type of hook.
 * @error				Invalid event name or plugin manager not initialized.
 */
stock void PM_AddEventHook(const char[] name, EventHook callback, EventHookMode mode = EventHookMode_Post)
{
	ThrowIfNotInitialized();
	
	Event event = CreateEvent(name, true);
	if (event)
	{
		event.Cancel();
		
		EventData data;
		strcopy(data.name, sizeof(data.name), name);
		data.callback = callback;
		data.mode = mode;
		
		s_events.PushArray(data);
	}
	else
	{
		ThrowError("Failed to find event '%s'", name);
	}
}

/**
 * Unregisters an event to be hooked when the plugin gets enabled.
 *
 * @param name			Name of event.
 * @param callback		An EventHook function pointer.
 * @param mode			Optional EventHookMode determining the type of hook.
 * @error				Plugin manager not initialized.
 */
stock void PM_RemoveEventHook(const char[] name, EventHook callback, EventHookMode mode = EventHookMode_Post)
{
	ThrowIfNotInitialized();
	
	for (int i = 0; i < s_events.Length; i++)
	{
		EventData data;
		if (s_events.GetArray(i, data) && StrEqual(name, data.name) && callback == data.callback && mode == data.mode)
		{
			if (PM_IsEnabled())
				ToggleEvent(data, false);
			
			s_events.Erase(i);
		}
	}
}

//=============================================================================
// Detours
//=============================================================================

/**
 * Registers a dynamic detour by its name in the "Functions" section of the gamedata file.
 *
 * @param name				Name of the function in a "Functions" section to load.
 * @param callbackPre		Optional pre callback function.
 * @param callbackPost		Optional post callback function.
 * @error					Invalid gamedata, plugin manager not initialized or GameData handle not initialized.
 */
stock DynamicDetour PM_AddDynamicDetourFromConf(const char[] name, DHookCallback callbackPre = INVALID_FUNCTION, DHookCallback callbackPost = INVALID_FUNCTION)
{
	ThrowIfNotInitialized(true);
	
	DynamicDetour detour = DynamicDetour.FromConf(s_gamedata, name);
	if (detour)
	{
		PM_AddDynamicDetour(name, detour, callbackPre, callbackPost);
		return detour;
	}
	else
	{
		ThrowError("Failed to create detour setup handle '%s'", name);
		return null;
	}
}

/**
 * Registers a dynamic detour.
 *
 * @param name				Name to register the dynamic detour as.
 * @param detour			The DynamicDetour object.
 * @param callbackPre		Optional pre callback function.
 * @param callbackPost		Optional post callback function.
 * @error					Plugin manager not initialized.
 */
stock void PM_AddDynamicDetour(const char[] name, DynamicDetour detour, DHookCallback callbackPre = INVALID_FUNCTION, DHookCallback callbackPost = INVALID_FUNCTION)
{
	ThrowIfNotInitialized(true);
	
	DetourData data;
	strcopy(data.name, sizeof(data.name), name);
	data.detour = detour;
	data.callbackPre = callbackPre;
	data.callbackPost = callbackPost;
	
	s_detours.PushArray(data);
}

/**
 * Unregisters a dynamic detour.
 *
 * @param name				The name of the registered dynamic detour.
 * @param callbackPre		Optional pre callback function.
 * @param callbackPost		Optional post callback function.
 * @error					Plugin manager not initialized.
 */
stock void PM_RemoveDynamicDetour(const char[] name, DHookCallback callbackPre = INVALID_FUNCTION, DHookCallback callbackPost = INVALID_FUNCTION)
{
	ThrowIfNotInitialized();
	
	for (int i = s_detours.Length - 1; i >= 0; i--)
	{
		DetourData data;
		if (s_detours.GetArray(i, data) && StrEqual(name, data.name) && callbackPre == data.callbackPre && callbackPost == data.callbackPost)
		{
			if (PM_IsEnabled())
				ToggleDetour(data, false);
			
			s_detours.Erase(i);
		}
	}
}

//=============================================================================
// Dynamic Hooks
//=============================================================================

/**
 * Registers a dynamic hook by its name in the "Functions" section of the gamedata file.
 *
 * @param name		Name of the function in a "Functions" section to load.
 * @return			The DynamicHook on success, null otherwise.
 * @error			Invalid gamedata, plugin manager not initialized or GameData handle not initialized.
 */
stock DynamicHook PM_AddDynamicHookFromConf(const char[] name)
{
	ThrowIfNotInitialized(true);
	
	DynamicHook hook = DynamicHook.FromConf(s_gamedata, name);
	if (hook)
	{
		PM_AddDynamicHook(name, hook);
		return hook;
	}
	else
	{
		ThrowError("Failed to create hook setup handle '%s'", name);
		return null;
	}
}

/**
 * Registers a dynamic hook.
 *
 * @param name			Name to register the dynamic hook as.
 * @param detour		The DynamicHook object.
 * @error				Plugin manager not initialized.
 */
stock void PM_AddDynamicHook(const char[] name, DynamicHook hook)
{
	ThrowIfNotInitialized(true);
	
	DHookData data;
	strcopy(data.name, sizeof(data.name), name);
	data.hook = hook;
	
	s_dynamicHooks.PushArray(data);
}

/**
 * Unregisters a dynamic hook.
 *
 * @param name		The name of the registered dynamic hook.
 * @error			Plugin manager not initialized.
 */
stock void PM_RemoveDynamicHook(const char[] name)
{
	ThrowIfNotInitialized();
	
	for (int i = s_dynamicHooks.Length - 1; i >= 0; i--)
	{
		DHookData data;
		if (s_dynamicHooks.GetArray(i, data) && StrEqual(name, data.name))
		{
			// Unhook any entities that belong to this hook
			for (int j = s_dynamicHookIds.Length - 1; j >= 0; j--)
			{
				if (s_dynamicHookIds.Get(j, DHookIdData::hook) == data.hook)
					DynamicHook.RemoveHook(s_dynamicHookIds.Get(j, DHookIdData::hookid));
			}
			
			s_dynamicHooks.Erase(i);
		}
	}
}

/**
 * Finds a dynamic hook by name and hooks an entity.
 * This function does nothing if the plugin is not currently enabled.
 *
 * @param name			The name of the registered dynamic hook.
 * @param mode			The desired hook mode - pre or post. 
 * @param entity		Entity index to hook on.
 * @param callback		Callback function.
 * @return				A hookid on success, INVALID_HOOK_ID otherwise.
 * @error				Plugin manager not initialized.
 */
stock int PM_DynamicHookEntityFromConf(const char[] name, HookMode mode, int entity, DHookCallback callback)
{
	ThrowIfNotInitialized();
	
	if (!PM_IsEnabled())
		return INVALID_HOOK_ID;
	
	int index = s_dynamicHooks.FindString(name, DHookData::name);
	if (index == -1)
		ThrowError("Failed to find dynamic hook '%s'", name);
	
	DynamicHook hook = s_dynamicHooks.Get(index, DHookData::hook);
	return PM_DynamicHookEntity(hook, mode, entity, callback);
}

/**
 * Hooks an entity.
 * This function does nothing if the plugin is not currently enabled.
 *
 * @param hook			The DynamicHook handle.
 * @param mode			The desired hook mode - pre or post. 
 * @param entity		Entity index to hook on.
 * @param callback		Callback function.
 * @return				A hookid on success, INVALID_HOOK_ID otherwise.
 * @error				Plugin manager not initialized.
 */
stock int PM_DynamicHookEntity(DynamicHook hook, HookMode mode, int entity, DHookCallback callback)
{
	ThrowIfNotInitialized();
	
	if (!PM_IsEnabled())
		return INVALID_HOOK_ID;
	
	int hookid = hook.HookEntity(Hook_Pre, entity, callback, OnDynamicHookRemoved);
	if (hookid != INVALID_HOOK_ID)
	{
		DHookIdData data;
		data.hookid = hookid;
		data.hook = hook;
		
		s_dynamicHookIds.PushArray(data);
	}
	
	return hookid;
}

/**
 * Hooks the gamerules object.
 * This function does nothing if the plugin is not currently enabled.
 *
 * @param hook			The DynamicHook handle.
 * @param mode			The desired hook mode - pre or post. 
 * @param callback		Callback function.
 * @return				A hookid on success, INVALID_HOOK_ID otherwise.
 * @error				Plugin manager not initialized.
 */
stock int PM_DynamicHookGameRules(DynamicHook hook, HookMode mode, DHookCallback callback)
{
	ThrowIfNotInitialized();
	
	if (!PM_IsEnabled())
		return INVALID_HOOK_ID;
	
	int hookid = hook.HookGamerules(mode, callback, OnDynamicHookRemoved);
	if (hookid != INVALID_HOOK_ID)
	{
		DHookIdData data;
		data.hookid = hookid;
		data.hook = hook;
		
		s_dynamicHookIds.PushArray(data);
	}
	
	return hookid;
}

//=============================================================================
// SDKHooks
//=============================================================================

/**
 * Hooks an entity.
 * This function does nothing if the plugin is not currently enabled.
 *
 * @param entity		Entity index.
 * @param type			Type of function to hook.
 * @param callback		Function to call when hook is called.
 * @return				True on success, false otherwise.
 * @error				Plugin manager not initialized.
 */
stock bool PM_SDKHook(int entity, SDKHookType type, SDKHookCB callback)
{
	ThrowIfNotInitialized();
	
	if (!PM_IsEnabled())
		return false;
	
	SDKHookData data;
	data.ref = IsValidEdict(entity) ? EntIndexToEntRef(entity) : entity;
	data.type = type;
	data.callback = callback;
	
	s_sdkHooks.PushArray(data);
	
	SDKHook(entity, type, callback);
	return true;
}

/**
 * Unhooks an entity.
 *
 * @param entity		Entity index.
 * @error				Plugin manager not initialized.
 */
stock void PM_SDKUnhook(int entity)
{
	ThrowIfNotInitialized();
	
	int ref = IsValidEdict(entity) ? EntIndexToEntRef(entity) : entity;
	
	for (int i = s_sdkHooks.Length - 1; i >= 0; i--)
	{
		SDKHookData data;
		if (s_sdkHooks.GetArray(i, data) && ref == data.ref)
		{
			SDKUnhook(data.ref, data.type, data.callback);
			s_sdkHooks.Erase(i);
		}
	}
}

//=============================================================================
// ConVars
//=============================================================================

/**
 * Registers an "enforced" convar, meaning that its value will be enforced even if it is changed by configs or other plugins.
 *
 * @param name		The name of the convar.
 * @param value		The value to enforce.
 * @return			True on success, false otherwise.
 * @error			Invalid convar name or plugin manager not initialized.
 */
stock bool PM_AddEnforcedConVar(const char[] name, const char[] value)
{
	ThrowIfNotInitialized();
	
	ConVar convar = FindConVar(name);
	if (!convar)
	{
		ThrowError("Failed to find convar '%s'", name);
		return false;
	}
	
	ConVarData data;
	strcopy(data.name, sizeof(data.name), name);
	strcopy(data.value, sizeof(data.value), value);
	
	s_convars.PushArray(data);
	return true;
}

/**
 * Registers a "synced" convar, meaning that its value will always be kept in sync with another convar.
 *
 * @param name		The name of the convar.
 * @param value		The value to enforce.
 * @return			True on success, false otherwise.
 * @error			Invalid convar name or plugin manager not initialized.
 */
stock bool PM_AddSyncedConVar(const char[] name, ConVar relatedConVar)
{
	ThrowIfNotInitialized();
	
	ConVar convar = FindConVar(name);
	if (!convar)
	{
		ThrowError("Failed to find convar '%s'", name);
		return false;
	}
	
	ConVarData data;
	strcopy(data.name, sizeof(data.name), name);
	data.relatedConVar = relatedConVar;
	
	s_convars.PushArray(data);
	return true;
}

/**
 * Returns whether the Plugin Manager is initialized.
 *
 * @return		True if initialized, false otherwise.
 * @error		Plugin manager not initialized.
 */
stock bool PM_IsInitialized()
{
	return s_isInitialized;
}

/**
 * Returns whether the plugin is currently enabled.
 *
 * @return		True if enabled, false otherwise.
 * @error		Plugin manager not initialized.
 */
stock bool PM_IsEnabled()
{
	return s_isEnabled;
}

//*******************************************************************************************************************************************************

static void ThrowIfNotInitialized(bool requireGameData = false)
{
	if (!PM_IsInitialized())
	{
		ThrowError("The plugin manager is not initialized! Call 'PM_Init' to use this function!");
	}
	
	if (requireGameData && !s_gamedata)
	{
		ThrowError("GameData handle is not initialized! Pass a valid GameData handle to 'PM_Init' to use this function!");
	}
}

static void OnEnableConVarChanged(ConVar convar, const char[] oldValue, const char[] newValue)
{
	PM_TogglePluginStateIfNeeded();
}

static void OnDynamicHookRemoved(int hookid)
{
	int index = s_dynamicHookIds.FindValue(hookid, DHookIdData::hookid);
	if (index != -1)
		s_dynamicHookIds.Erase(index);
}

static void ToggleDetour(DetourData data, bool enable)
{
	if (data.callbackPre != INVALID_FUNCTION)
	{
		if (enable)
			data.detour.Enable(Hook_Pre, data.callbackPre);
		else
			data.detour.Disable(Hook_Pre, data.callbackPre);
	}
	
	if (data.callbackPost != INVALID_FUNCTION)
	{
		if (enable)
			data.detour.Enable(Hook_Post, data.callbackPost);
		else
			data.detour.Disable(Hook_Post, data.callbackPost);
	}
}

static void ToggleEvent(EventData data, bool enable)
{
	if (enable)
		HookEvent(data.name, data.callback, data.mode);
	else
		UnhookEvent(data.name, data.callback, data.mode);
}

static void ToggleConVar(int index, bool enable)
{
	ConVarData data;
	if (!s_convars.GetArray(index, data))
		return;
	
	ConVar convar = FindConVar(data.name);
	if (!convar)
		return;
	
	if (enable)
	{
		// Store the current value so we can reset the convar on disable
		convar.GetString(data.initialValue, sizeof(data.initialValue));
		
		// Copy the value from the setting convar if it isn't set
		if (!data.value[0] && data.relatedConVar)
		{
			char value[64];
			data.relatedConVar.GetString(value, sizeof(value));
			strcopy(data.value, sizeof(data.value), value);
			
			data.relatedConVar.AddChangeHook(OnRelatedConVarChanged);
		}
		
		s_convars.SetArray(index, data);
		
		// Update the current value
		convar.SetString(data.value);
		convar.AddChangeHook(OnConVarChanged);
	}
	else
	{
		// Restore the old convar value
		convar.RemoveChangeHook(OnConVarChanged);
		convar.SetString(data.initialValue);
		
		if (data.relatedConVar)
		{
			PrintToServer("test");
			data.value[0] = EOS;
			data.relatedConVar.RemoveChangeHook(OnRelatedConVarChanged);
			
			s_convars.SetArray(index, data);
		}
	}
}

static void OnConVarChanged(ConVar convar, const char[] oldValue, const char[] newValue)
{
	char name[64];
	convar.GetName(name, sizeof(name));
	
	int index = s_convars.FindString(name);
	if (index == -1)
		return;
	
	ConVarData data;
	if (s_convars.GetArray(index, data))
	{
		if (!StrEqual(newValue, data.value))
		{
			// Update value to reset
			strcopy(data.initialValue, sizeof(data.initialValue), newValue);
			s_convars.SetArray(index, data);
			
			// Restore our desired value
			convar.SetString(oldValue);
		}
	}
}

static void OnRelatedConVarChanged(ConVar convar, const char[] oldValue, const char[] newValue)
{
	int index = s_convars.FindValue(convar, ConVarData::relatedConVar);
	if (index == -1)
		return;
	
	ConVarData data;
	if (s_convars.GetArray(index, data))
	{
		ConVar actualConVar = FindConVar(data.name);
		if (!actualConVar)
			return;
		
		actualConVar.RemoveChangeHook(OnConVarChanged);
		actualConVar.SetString(newValue);
		actualConVar.AddChangeHook(OnConVarChanged);
	}
}
