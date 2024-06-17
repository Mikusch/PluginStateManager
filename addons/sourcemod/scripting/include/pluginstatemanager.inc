#pragma semicolon 1
#pragma newdecls required

#include <dhooks>
#include <sdkhooks>

#define PSM_VERSION		1.0.0

typedef PluginStateChangedCallback = function void(bool enabled);

enum struct EventData
{
	char name[32];
	EventHook callback;
	EventHookMode mode;
	
	void Toggle(bool enable)
	{
		if (enable)
			HookEvent(this.name, this.callback, this.mode);
		else
			UnhookEvent(this.name, this.callback, this.mode);
	}
}

enum struct DetourData
{
	char name[64];
	DynamicDetour detour;
	DHookCallback callbackPre;
	DHookCallback callbackPost;
	
	void Toggle(bool enable)
	{
		if (this.callbackPre != INVALID_FUNCTION)
		{
			if (enable)
				this.detour.Enable(Hook_Pre, this.callbackPre);
			else
				this.detour.Disable(Hook_Pre, this.callbackPre);
		}
		
		if (this.callbackPost != INVALID_FUNCTION)
		{
			if (enable)
				this.detour.Enable(Hook_Post, this.callbackPost);
			else
				this.detour.Disable(Hook_Post, this.callbackPost);
		}
	}
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
	ConVar sourceConVar;
}

enum struct EntityOutputData
{
	char classname[64];
	char output[64];
	EntityOutput callback;
	
	void Toggle(bool enable)
	{
		if (enable)
			HookEntityOutput(this.classname, this.output, this.callback);
		else
			UnhookEntityOutput(this.classname, this.output, this.callback);
	}
}

enum struct CommandListenerData
{
	char command[64];
	CommandListener callback;
	
	void Toggle(bool enable)
	{
		if (enable)
			AddCommandListener(this.callback, this.command);
		else
			RemoveCommandListener(this.callback, this.command);
	}
}

enum struct TempEntHookData
{
	char name[64];
	TEHook hook;
	
	void Toggle(bool enable)
	{
		if (enable)
			AddTempEntHook(this.name, this.hook);
		else
			RemoveTempEntHook(this.name, this.hook);
	}
}

static bool s_isInitialized;
static bool s_isEnabled;
static char s_conVarName[64];
static ConVar s_stateConVar;
static GameData s_gamedata;
static PrivateForward s_stateChangedFwd;

static ArrayList s_events; // ArrayList<EventData>
static ArrayList s_detours; // ArrayList <DetourData>
static ArrayList s_dynamicHooks; // ArrayList<DHookData>
static ArrayList s_dynamicHookIds; // ArrayList<DHookIdData>
static ArrayList s_sdkHooks; // ArrayList<SDKHookData>
static ArrayList s_convars; // ArrayList<ConVarData>
static ArrayList s_entityOutputs; // ArrayList<EntityOutputData>
static ArrayList s_commandListeners; // ArrayList<CommandListenerData>
static ArrayList s_tempEntHooks; // ArrayList<TempEntHookData>

//=============================================================================
// Core Features
//=============================================================================

/**
 * Initializes the plugin state manager.
 *
 * This function should be called in the 'OnPluginStart' forward.
 *
 * @param convarName		The name of the convar used to determine whether the plugin should be enabled.
 * 							If the calling plugin does not create this convar, one will be automatically created.
 * @param gamedata			Optional gamedata handle.
 */
stock void PSM_Init(const char[] convarName, GameData gamedata = null)
{
	if (s_isInitialized)
		return;
	
	strcopy(s_conVarName, sizeof(s_conVarName), convarName);
	RequestFrame(InitializeStateConVar);
	
	if (gamedata)
		s_gamedata = view_as<GameData>(CloneHandle(gamedata));
	
	s_stateChangedFwd = new PrivateForward(ET_Ignore, Param_Cell);
	
	s_events = new ArrayList(sizeof(EventData));
	s_detours = new ArrayList(sizeof(DetourData));
	s_dynamicHooks = new ArrayList(sizeof(DHookData));
	s_dynamicHookIds = new ArrayList(sizeof(DHookIdData));
	s_sdkHooks = new ArrayList(sizeof(SDKHookData));
	s_convars = new ArrayList(sizeof(ConVarData));
	s_entityOutputs = new ArrayList(sizeof(EntityOutputData));
	s_commandListeners = new ArrayList(sizeof(CommandListenerData));
	s_tempEntHooks = new ArrayList(sizeof(TempEntHookData));
	
	s_isInitialized = true;
}

/**
 * Convenience function that disables the plugin if it is enabled.
 *
 * This function should be called in the 'OnPluginEnd' forward.
 *
 * @error Plugin state manager not initialized.
 */
stock void PSM_Disable()
{
	ThrowIfNotInitialized();
	
	if (PSM_IsEnabled())
		PSM_SetPluginState(false);
}

/**
 * Convenience function that toggles the enabled state of the plugin if it isn't in that state already.
 *
 * This function should be called in the 'OnConfigsExecuted' forward.
 *
 * @error Plugin state manager not initialized.
 */
stock void PSM_TogglePluginState()
{
	ThrowIfNotInitialized();
	
	InitializeStateConVar();
	
	if (PSM_IsEnabled() != s_stateConVar.BoolValue)
	{
		PSM_SetPluginState(s_stateConVar.BoolValue);
	}
}

/**
 * Sets the enabled state of the plugin.
 *
 * Most plugins want to use 'PSM_TogglePluginState' to respect the convar.
 *
 * @param enable		If true, the plugin will be enabled. If false, the plugin will be disabled.
 * @error				Plugin state manager not initialized.
 */
stock void PSM_SetPluginState(bool enable)
{
	ThrowIfNotInitialized();
	
	if (PSM_IsEnabled() == enable)
		return;
	
	s_isEnabled = enable;
	
	// Events
	for (int i = 0; i < s_events.Length; i++)
	{
		EventData data;
		if (s_events.GetArray(i, data))
		{
			data.Toggle(enable);
		}
	}
	
	// Detours
	for (int i = 0; i < s_detours.Length; i++)
	{
		DetourData data;
		if (s_detours.GetArray(i, data))
		{
			data.Toggle(enable);
		}
	}
	
	if (!enable)
	{
		// Dynamic hooks
		for (int i = s_dynamicHookIds.Length - 1; i >= 0; i--)
		{
			int hookid = s_dynamicHookIds.Get(i, DHookIdData::hookid);
			DynamicHook.RemoveHook(hookid);
		}
		
		// SDKHooks
		int entity = -1;
		while ((entity = FindEntityByClassname(entity, "*")) != -1)
		{
			PSM_SDKUnhook(entity);
		}
	}
	
	// Entity outputs
	for (int i = 0; i < s_entityOutputs.Length; i++)
	{
		EntityOutputData data;
		if (s_entityOutputs.GetArray(i, data))
		{
			data.Toggle(enable);
		}
	}
	
	// Command listeners
	for (int i = 0; i < s_commandListeners.Length; i++)
	{
		CommandListenerData data;
		if (s_commandListeners.GetArray(i, data))
		{
			data.Toggle(enable);
		}
	}
	
	// ConVars
	for (int i = 0; i < s_convars.Length; i++)
	{
		ToggleConVar(i, enable);
	}
	
	// TE hooks
	for (int i = 0; i < s_tempEntHooks.Length; i++)
	{
		TempEntHookData data;
		if (s_tempEntHooks.GetArray(i, data))
		{
			data.Toggle(enable);
		}
	}
	
	if (s_stateChangedFwd.FunctionCount > 0)
	{
		Call_StartForward(s_stateChangedFwd);
		Call_PushCell(enable);
		Call_Finish();
	}
}

/**
 * Registers a callback function that is called when the state of the plugin changes.
 *
 * @param func		A PluginStateChangedCallback function pointer.
 * @return			True on success, false otherwise.
 * @error			Plugin state manager not initialized.
 */
stock bool PSM_AddPluginStateChangedHook(PluginStateChangedCallback func)
{
	ThrowIfNotInitialized();
	
	return s_stateChangedFwd.AddFunction(null, func);
}

/**
 * Returns whether the Plugin Manager is initialized.
 *
 * @return		True if initialized, false otherwise.
 */
stock bool PSM_IsInitialized()
{
	return s_isInitialized;
}

/**
 * Returns whether the plugin is currently enabled.
 *
 * @return		True if enabled, false otherwise.
 */
stock bool PSM_IsEnabled()
{
	return PSM_IsInitialized() && s_isEnabled;
}

//=============================================================================
// Events
//=============================================================================

/**
 * Registers an event hook.
 *
 * @param name			Name of event.
 * @param callback		An EventHook function pointer.
 * @param mode			Optional EventHookMode determining the type of hook.
 * @error				Invalid event name or plugin state manager not initialized.
 */
stock void PSM_AddEventHook(const char[] name, EventHook callback, EventHookMode mode = EventHookMode_Post)
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
 * Unregisters an event hook.
 *
 * @param name			Name of event.
 * @param callback		An EventHook function pointer.
 * @param mode			Optional EventHookMode determining the type of hook.
 * @error				Plugin state manager not initialized.
 */
stock void PSM_RemoveEventHook(const char[] name, EventHook callback, EventHookMode mode = EventHookMode_Post)
{
	ThrowIfNotInitialized();
	
	for (int i = 0; i < s_events.Length; i++)
	{
		EventData data;
		if (s_events.GetArray(i, data) && StrEqual(name, data.name) && callback == data.callback && mode == data.mode)
		{
			if (PSM_IsEnabled())
				data.Toggle(false);
			
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
 * @error					Invalid gamedata, plugin state manager not initialized or GameData handle not initialized.
 */
stock DynamicDetour PSM_AddDynamicDetourFromConf(const char[] name, DHookCallback callbackPre = INVALID_FUNCTION, DHookCallback callbackPost = INVALID_FUNCTION)
{
	ThrowIfNotInitialized(true);
	
	DynamicDetour detour = DynamicDetour.FromConf(s_gamedata, name);
	if (detour)
	{
		PSM_AddDynamicDetour(name, detour, callbackPre, callbackPost);
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
 * @error					Plugin state manager not initialized.
 */
stock void PSM_AddDynamicDetour(const char[] name, DynamicDetour detour, DHookCallback callbackPre = INVALID_FUNCTION, DHookCallback callbackPost = INVALID_FUNCTION)
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
 * @error					Plugin state manager not initialized.
 */
stock void PSM_RemoveDynamicDetour(const char[] name, DHookCallback callbackPre = INVALID_FUNCTION, DHookCallback callbackPost = INVALID_FUNCTION)
{
	ThrowIfNotInitialized();
	
	for (int i = s_detours.Length - 1; i >= 0; i--)
	{
		DetourData data;
		if (s_detours.GetArray(i, data) && StrEqual(name, data.name) && callbackPre == data.callbackPre && callbackPost == data.callbackPost)
		{
			if (PSM_IsEnabled())
				data.Toggle(false);
			
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
 * @error			Invalid gamedata, plugin state manager not initialized or GameData handle not initialized.
 */
stock DynamicHook PSM_AddDynamicHookFromConf(const char[] name)
{
	ThrowIfNotInitialized(true);
	
	DynamicHook hook = DynamicHook.FromConf(s_gamedata, name);
	if (hook)
	{
		PSM_AddDynamicHook(name, hook);
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
 * @param hook			The DynamicHook object.
 * @error				Plugin state manager not initialized.
 */
stock void PSM_AddDynamicHook(const char[] name, DynamicHook hook)
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
 * @error			Plugin state manager not initialized.
 */
stock void PSM_RemoveDynamicHook(const char[] name)
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
 *
 * This function does nothing if the plugin is not currently enabled.
 *
 * @param name			The name of the registered dynamic hook.
 * @param mode			The desired hook mode - pre or post. 
 * @param entity		Entity index to hook on.
 * @param callback		Callback function.
 * @return				A hookid on success, INVALID_HOOK_ID otherwise.
 * @error				Plugin state manager not initialized.
 */
stock int PSM_DynamicHookEntityByName(const char[] name, HookMode mode, int entity, DHookCallback callback)
{
	ThrowIfNotInitialized();
	
	if (!PSM_IsEnabled())
		return INVALID_HOOK_ID;
	
	int index = s_dynamicHooks.FindString(name, DHookData::name);
	if (index == -1)
		ThrowError("Failed to find dynamic hook '%s'", name);
	
	DynamicHook hook = s_dynamicHooks.Get(index, DHookData::hook);
	return PSM_DynamicHookEntity(hook, mode, entity, callback);
}

/**
 * Hooks an entity.
 *
 * This function does nothing if the plugin is not currently enabled.
 *
 * @param hook			The DynamicHook handle.
 * @param mode			The desired hook mode - pre or post. 
 * @param entity		Entity index to hook on.
 * @param callback		Callback function.
 * @return				A hookid on success, INVALID_HOOK_ID otherwise.
 * @error				Plugin state manager not initialized.
 */
stock int PSM_DynamicHookEntity(DynamicHook hook, HookMode mode, int entity, DHookCallback callback)
{
	ThrowIfNotInitialized();
	
	if (!PSM_IsEnabled())
		return INVALID_HOOK_ID;
	
	int hookid = hook.HookEntity(mode, entity, callback, OnDynamicHookRemoved);
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
 * Finds a dynamic hook by name and hooks the gamerules object.
 *
 * This function does nothing if the plugin is not currently enabled.
 *
 * @param name			The name of the registered dynamic hook.
 * @param mode			The desired hook mode - pre or post. 
 * @param callback		Callback function.
 * @return				A hookid on success, INVALID_HOOK_ID otherwise.
 * @error				Plugin state manager not initialized.
 */
stock int PSM_DynamicHookGameRulesByName(const char[] name, HookMode mode, DHookCallback callback)
{
	ThrowIfNotInitialized();
	
	if (!PSM_IsEnabled())
		return INVALID_HOOK_ID;
	
	int index = s_dynamicHooks.FindString(name, DHookData::name);
	if (index == -1)
		ThrowError("Failed to find dynamic hook '%s'", name);
	
	DynamicHook hook = s_dynamicHooks.Get(index, DHookData::hook);
	return PSM_DynamicHookGameRules(hook, mode, callback);
}

/**
 * Hooks the gamerules object.
 *
 * This function does nothing if the plugin is not currently enabled.
 *
 * @param hook			The DynamicHook handle.
 * @param mode			The desired hook mode - pre or post. 
 * @param callback		Callback function.
 * @return				A hookid on success, INVALID_HOOK_ID otherwise.
 * @error				Plugin state manager not initialized.
 */
stock int PSM_DynamicHookGameRules(DynamicHook hook, HookMode mode, DHookCallback callback)
{
	ThrowIfNotInitialized();
	
	if (!PSM_IsEnabled())
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
 *
 * This function does nothing if the plugin is not currently enabled.
 *
 * @param entity		Entity index.
 * @param type			Type of function to hook.
 * @param callback		Function to call when hook is called.
 * @return				True on success, false otherwise.
 * @error				Plugin state manager not initialized.
 */
stock bool PSM_SDKHook(int entity, SDKHookType type, SDKHookCB callback)
{
	ThrowIfNotInitialized();
	
	if (!PSM_IsEnabled())
		return false;
	
	SDKHookData data;
	data.ref = EntIndexToEntRefSafe(entity);
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
 * @error				Plugin state manager not initialized.
 */
stock void PSM_SDKUnhook(int entity)
{
	ThrowIfNotInitialized();
	
	int ref = EntIndexToEntRefSafe(entity);
	
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
 * @error			Invalid convar name or plugin state manager not initialized.
 */
stock void PSM_AddEnforcedConVar(const char[] name, const char[] value)
{
	ThrowIfNotInitialized();
	
	ConVar convar = FindConVar(name);
	if (!convar)
	{
		ThrowError("Failed to find convar '%s'", name);
	}
	
	ConVarData data;
	strcopy(data.name, sizeof(data.name), name);
	strcopy(data.value, sizeof(data.value), value);
	
	s_convars.PushArray(data);
}

/**
 * Registers a "synced" convar, meaning that its value will always be kept in sync with another convar.
 *
 * @param targetName		The name of the convar to keep in sync.
 * @param relatedName		The name of the source convar.
 * @error					Invalid convar name or plugin state manager not initialized.
 */
stock void PSM_AddSyncedConVar(const char[] targetName, const char[] sourceName)
{
	ThrowIfNotInitialized();
	
	ConVar convar = FindConVar(targetName);
	if (!convar)
	{
		ThrowError("Failed to find convar '%s'", targetName);
	}
	
	ConVar sourceConVar = FindConVar(sourceName);
	if (!sourceConVar)
	{
		ThrowError("Failed to find convar '%s'", sourceName);
	}
	
	ConVarData data;
	strcopy(data.name, sizeof(data.name), targetName);
	data.sourceConVar = sourceConVar;
	
	s_convars.PushArray(data);
}

//=============================================================================
// Miscellaneous
//=============================================================================

/**
 * Registers an entity output hook.
 *
 * @param classname		The classname to hook.
 * @param output		The output name to hook.
 * @param callback		An EntityOutput function pointer.
 * @error				Plugin state manager not initialized.
 */
stock void PSM_AddEntityOutputHook(const char[] classname, const char[] output, EntityOutput callback)
{
	ThrowIfNotInitialized();
	
	EntityOutputData data;
	strcopy(data.classname, sizeof(data.classname), classname);
	strcopy(data.output, sizeof(data.output), output);
	data.callback = callback;
	
	s_entityOutputs.PushArray(data);
}

/**
 * Registers a command listener.
 *
 * @param callback		A CommandListener function pointer.
 * @param command		Command, or if not specified, a global listener.
 * 						The command is case insensitive.
 * @error				Plugin state manager not initialized.
 */
stock void PSM_AddCommandListener(CommandListener callback, const char[] command = "")
{
	ThrowIfNotInitialized();
	
	CommandListenerData data;
	strcopy(data.command, sizeof(data.command), command);
	data.callback = callback;
	
	s_commandListeners.PushArray(data);
}

/**
 * Registers a temp entity hook.
 *
 * @param te_name		TE name to hook.
 * @param hook			Function to use as a hook.
 * @error				Plugin state manager not initialized.
 */
stock void PSM_AddTempEntHook(const char[] te_name, TEHook hook)
{
	ThrowIfNotInitialized();
	
	TempEntHookData data;
	strcopy(data.name, sizeof(data.name), te_name);
	data.hook = hook;
	
	s_tempEntHooks.PushArray(data);
}

//*******************************************************************************************************************************************************

static void ThrowIfNotInitialized(bool requireGameData = false)
{
	if (!PSM_IsInitialized())
	{
		ThrowError("The plugin state manager is not initialized! Call 'PSM_Init' to use this function!");
	}
	
	if (requireGameData && !s_gamedata)
	{
		ThrowError("GameData handle is not initialized! Pass a valid GameData handle to 'PSM_Init' to use this function!");
	}
}

static void InitializeStateConVar()
{
	if (!s_stateConVar)
	{
		s_stateConVar = FindConVar(s_conVarName);
		if (!s_stateConVar)
		{
			s_stateConVar = CreateConVar(s_conVarName, "Enable the plugin?", "1");
			LogMessage("ConVar '%s' has been automatically created.", s_conVarName);
		}
		
		s_stateConVar.AddChangeHook(OnEnableConVarChanged);
	}
}

static void OnEnableConVarChanged(ConVar convar, const char[] oldValue, const char[] newValue)
{
	PSM_TogglePluginState();
}

static void OnDynamicHookRemoved(int hookid)
{
	int index = s_dynamicHookIds.FindValue(hookid, DHookIdData::hookid);
	if (index != -1)
		s_dynamicHookIds.Erase(index);
}

static int EntIndexToEntRefSafe(int entity)
{
	return IsValidEdict(entity) ? EntIndexToEntRef(entity) : entity;
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
		if (!data.value[0] && data.sourceConVar)
		{
			char value[64];
			data.sourceConVar.GetString(value, sizeof(value));
			strcopy(data.value, sizeof(data.value), value);
			
			data.sourceConVar.AddChangeHook(OnSourceConVarChanged);
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
		
		if (data.sourceConVar)
		{
			data.value[0] = '\0';
			data.sourceConVar.RemoveChangeHook(OnSourceConVarChanged);
			
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

static void OnSourceConVarChanged(ConVar convar, const char[] oldValue, const char[] newValue)
{
	int index = s_convars.FindValue(convar, ConVarData::sourceConVar);
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
