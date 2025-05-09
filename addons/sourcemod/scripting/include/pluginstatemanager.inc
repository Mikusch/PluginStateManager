#pragma semicolon 1
#pragma newdecls required

#include <dhooks>
#include <sdkhooks>
#tryinclude <sourcescramble>

#define PSM_VERSION	1.2.0

typedef PluginStateChangedCallback = function void(bool enabled);
typedef ShouldEnableCallback = function bool();

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

enum struct DynamicDetourData
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

enum struct DynamicHookData
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

enum struct EnforcedConVarData
{
	char name[64];
	char value[64];
	char initialValue[64];
}

enum struct SyncedConVarData
{
	char name[64];
	char initialValue[64];
	ConVar sourceConVar;
}

enum struct ConVarChangeHookData
{
	ConVar convar;
	ConVarChanged callback;
	
	void Toggle(bool enable)
	{
		if (enable)
			this.convar.AddChangeHook(this.callback);
		else
			this.convar.RemoveChangeHook(this.callback);
	}
}

enum struct EntityOutputHookData
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

enum struct UserMessageHookData
{
	UserMsg msg_id;
	MsgHook hook;
	bool intercept;
	MsgPostHook post;
	
	void Toggle(bool enable)
	{
		if (enable)
			HookUserMessage(this.msg_id, this.hook, this.intercept, this.post);
		else
			UnhookUserMessage(this.msg_id, this.hook, this.intercept);
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

enum struct MultiTargetFilterData
{
	char pattern[64];
	MultiTargetFilter filter;
	char phrase[64];
	bool phraseIsML;
	
	void Toggle(bool enable)
	{
		if (enable)
			AddMultiTargetFilter(this.pattern, this.filter, this.phrase, this.phraseIsML);
		else
			RemoveMultiTargetFilter(this.pattern, this.filter);
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

enum struct NormalSHookData
{
	NormalSHook hook;
	
	void Toggle(bool enable)
	{
		if (enable)
			AddNormalSoundHook(this.hook);
		else
			RemoveNormalSoundHook(this.hook);
	}
}

#if defined __sourcescramble_ext_included
enum struct MemoryPatchData
{
	char name[64];
	MemoryPatch patch;
	
	void Toggle(bool enable)
	{
		if (enable)
			this.patch.Enable();
		else
			this.patch.Disable();
	}
}
#endif

static bool s_isInitialized;
static bool s_isEnabled;
static char s_conVarName[64];
static ConVar s_stateConVar;
static GameData s_gamedata;
static PrivateForward s_stateChangedFwd;
static PrivateForward s_shouldEnableFwd;

static ArrayList s_events; // ArrayList<EventData>
static ArrayList s_dynamicDetours; // ArrayList <DynamicDetourData>
static ArrayList s_dynamicHooks; // ArrayList<DynamicHookData>
static ArrayList s_dynamicHookIds; // ArrayList<DHookIdData>
static ArrayList s_sdkHooks; // ArrayList<SDKHookData>
static ArrayList s_enforcedConVars; // ArrayList<EnforcedConVarData>
static ArrayList s_syncedConVars;   // ArrayList<SyncedConVarData>
static ArrayList s_conVarChangeHooks; // ArrayList<ConVarChangeHookData>
static ArrayList s_entityOutputHooks; // ArrayList<EntityOutputHookData>
static ArrayList s_userMessageHooks; // ArrayList<UserMessageHookData>
static ArrayList s_commandListeners; // ArrayList<CommandListenerData>
static ArrayList s_multiTargetFilters; // ArrayList<MultiTargetFilterData>
static ArrayList s_tempEntHooks; // ArrayList<TempEntHookData>
static ArrayList s_normalSHooks; // ArrayList<NormalSHookData>
#if defined __sourcescramble_ext_included
static ArrayList s_memoryPatches; // ArrayList<MemoryPatchData>
#endif

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
	
	if (gamedata)
		s_gamedata = view_as<GameData>(CloneHandle(gamedata));
	
	s_stateChangedFwd = new PrivateForward(ET_Ignore, Param_Cell);
	s_shouldEnableFwd = new PrivateForward(ET_Single);
	
	s_events = new ArrayList(sizeof(EventData));
	s_dynamicDetours = new ArrayList(sizeof(DynamicDetourData));
	s_dynamicHooks = new ArrayList(sizeof(DynamicHookData));
	s_dynamicHookIds = new ArrayList(sizeof(DHookIdData));
	s_sdkHooks = new ArrayList(sizeof(SDKHookData));
	s_enforcedConVars = new ArrayList(sizeof(EnforcedConVarData));
	s_syncedConVars = new ArrayList(sizeof(SyncedConVarData));
	s_conVarChangeHooks = new ArrayList(sizeof(ConVarChangeHookData));
	s_entityOutputHooks = new ArrayList(sizeof(EntityOutputHookData));
	s_userMessageHooks = new ArrayList(sizeof(UserMessageHookData));
	s_commandListeners = new ArrayList(sizeof(CommandListenerData));
	s_multiTargetFilters = new ArrayList(sizeof(MultiTargetFilterData));
	s_tempEntHooks = new ArrayList(sizeof(TempEntHookData));
	s_normalSHooks = new ArrayList(sizeof(NormalSHookData));
#if defined __sourcescramble_ext_included
	s_memoryPatches = new ArrayList(sizeof(MemoryPatchData));
#endif
	
	s_isInitialized = true;
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

	bool shouldEnable = s_stateConVar.BoolValue && PSM_ShouldEnable();

	if (PSM_IsEnabled() != shouldEnable)
	{
		PSM_SetPluginState(shouldEnable);
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
	for (int i = 0; i < s_dynamicDetours.Length; i++)
	{
		DynamicDetourData data;
		if (s_dynamicDetours.GetArray(i, data))
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
	
	// Entity output hooks
	for (int i = 0; i < s_entityOutputHooks.Length; i++)
	{
		EntityOutputHookData data;
		if (s_entityOutputHooks.GetArray(i, data))
		{
			data.Toggle(enable);
		}
	}
	
	// User message hooks
	for (int i = 0; i < s_userMessageHooks.Length; i++)
	{
		UserMessageHookData data;
		if (s_userMessageHooks.GetArray(i, data))
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
	
	// Multi-target filters
	for (int i = 0; i < s_multiTargetFilters.Length; i++)
	{
		MultiTargetFilterData data;
		if (s_multiTargetFilters.GetArray(i, data))
		{
			data.Toggle(enable);
		}
	}
	
	// Enforced ConVars
	for (int i = 0; i < s_enforcedConVars.Length; i++)
	{
		ToggleEnforcedConVar(i, enable);
	}

	// Synced ConVars
	for (int i = 0; i < s_syncedConVars.Length; i++)
	{
		ToggleSyncedConVar(i, enable);
	}
	
	// ConVar change hooks
	for (int i = 0; i < s_conVarChangeHooks.Length; i++)
	{
		ConVarChangeHookData data;
		if (s_conVarChangeHooks.GetArray(i, data))
		{
			data.Toggle(enable);
		}
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
	
	// Normal sound hooks
	for (int i = 0; i < s_normalSHooks.Length; i++)
	{
		NormalSHookData data;
		if (s_normalSHooks.GetArray(i, data))
		{
			data.Toggle(enable);
		}
	}
	
#if defined __sourcescramble_ext_included
	// Memory patches
	for (int i = 0; i < s_memoryPatches.Length; i++)
	{
		MemoryPatchData data;
		if (s_memoryPatches.GetArray(i, data))
		{
			data.Toggle(enable);
		}
	}
#endif
	
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
 * @param callback	A PluginStateChangedCallback function pointer.
 * @return			True on success, false otherwise.
 * @error			Plugin state manager not initialized.
 */
stock bool PSM_AddPluginStateChangedHook(PluginStateChangedCallback callback)
{
	ThrowIfNotInitialized();
	
	return s_stateChangedFwd.AddFunction(null, callback);
}

/**
 * Registers a callback function to determine whether `PSM_TogglePluginState` should enable the plugin.
 *
 * @param callback	A ShouldEnableCallback function pointer.
 * @return			True on success, false otherwise.
 * @error			Plugin state manager not initialized.
 */
stock bool PSM_AddShouldEnableCallback(ShouldEnableCallback callback)
{
	ThrowIfNotInitialized();

	return s_shouldEnableFwd.AddFunction(null, callback);
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

/**
 * Returns the GameData handle set by 'PSM_Init'.
 *
 * @return		GameData handle, or null.
 */
stock GameData PSM_GetGameData()
{
	return s_gamedata;
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
		
		if (PSM_IsEnabled())
			data.Toggle(true);
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
 * @error					Plugin state manager or GameData handle not initialized.
 */
stock void PSM_AddDynamicDetour(const char[] name, DynamicDetour detour, DHookCallback callbackPre = INVALID_FUNCTION, DHookCallback callbackPost = INVALID_FUNCTION)
{
	ThrowIfNotInitialized();
	
	DynamicDetourData data;
	strcopy(data.name, sizeof(data.name), name);
	data.detour = detour;
	data.callbackPre = callbackPre;
	data.callbackPost = callbackPost;
	
	s_dynamicDetours.PushArray(data);
	
	if (PSM_IsEnabled())
		data.Toggle(true);
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
	
	for (int i = s_dynamicDetours.Length - 1; i >= 0; i--)
	{
		DynamicDetourData data;
		if (s_dynamicDetours.GetArray(i, data) && StrEqual(name, data.name) && callbackPre == data.callbackPre && callbackPost == data.callbackPost)
		{
			if (PSM_IsEnabled())
				data.Toggle(false);
			
			s_dynamicDetours.Erase(i);
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
 * @error			Invalid gamedata or plugin state manager not initialized.
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
	ThrowIfNotInitialized();
	
	DynamicHookData data;
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
		DynamicHookData data;
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
stock int PSM_DHookEntityByName(const char[] name, HookMode mode, int entity, DHookCallback callback)
{
	ThrowIfNotInitialized();
	
	if (!PSM_IsEnabled())
		return INVALID_HOOK_ID;
	
	int index = s_dynamicHooks.FindString(name);
	if (index == -1)
		ThrowError("Failed to find dynamic hook '%s'", name);
	
	DynamicHook hook = s_dynamicHooks.Get(index, DynamicHookData::hook);
	return PSM_DHookEntity(hook, mode, entity, callback);
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
stock int PSM_DHookEntity(DynamicHook hook, HookMode mode, int entity, DHookCallback callback)
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
stock int PSM_DHookGameRulesByName(const char[] name, HookMode mode, DHookCallback callback)
{
	ThrowIfNotInitialized();
	
	if (!PSM_IsEnabled())
		return INVALID_HOOK_ID;
	
	int index = s_dynamicHooks.FindString(name);
	if (index == -1)
		ThrowError("Failed to find dynamic hook '%s'", name);
	
	DynamicHook hook = s_dynamicHooks.Get(index, DynamicHookData::hook);
	return PSM_DHookGameRules(hook, mode, callback);
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
stock int PSM_DHookGameRules(DynamicHook hook, HookMode mode, DHookCallback callback)
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
		ThrowError("Failed to find convar '%s'", name);

	EnforcedConVarData data;
	strcopy(data.name, sizeof(data.name), name);
	strcopy(data.value, sizeof(data.value), value);

	int index = s_enforcedConVars.PushArray(data);

	if (PSM_IsEnabled())
		ToggleEnforcedConVar(index, true);
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
		ThrowError("Failed to find convar '%s'", targetName);

	ConVar source = FindConVar(sourceName);
	if (!source)
		ThrowError("Failed to find convar '%s'", sourceName);

	SyncedConVarData data;
	strcopy(data.name, sizeof(data.name), targetName);
	data.sourceConVar = source;

	int index = s_syncedConVars.PushArray(data);

	if (PSM_IsEnabled())
		ToggleSyncedConVar(index, true);
}

/**
 * Registers a hook for when a console variable's value is changed.
 *
 * @param convar	The convar.
 * @param callback	An OnConVarChanged function pointer.
 * @error			Plugin state manager not initialized.
 */
stock void PSM_AddConVarChangeHook(ConVar convar, ConVarChanged callback)
{
	ThrowIfNotInitialized();
	
	ConVarChangeHookData data;
	data.convar = convar;
	data.callback = callback;
	
	s_conVarChangeHooks.PushArray(data);
	
	if (PSM_IsEnabled())
		data.Toggle(true);
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
	
	EntityOutputHookData data;
	strcopy(data.classname, sizeof(data.classname), classname);
	strcopy(data.output, sizeof(data.output), output);
	data.callback = callback;
	
	s_entityOutputHooks.PushArray(data);
	
	if (PSM_IsEnabled())
		data.Toggle(true);
}

/**
 * Registers a user message hook.
 *
 * @param msg_id		Message index.
 * @param hook			Function to use as a hook.
 * @param intercept		If intercept is true, message will be fully intercepted,
 * 						allowing the user to block the message.  Otherwise,
 * 						the hook is normal and ignores the return value.
 * @param post			Notification function.
 * @error				Plugin state manager not initialized or invalid message index.
 */
stock void PSM_AddUserMessageHook(UserMsg msg_id, MsgHook hook, bool intercept = false, MsgPostHook post = INVALID_FUNCTION)
{
	ThrowIfNotInitialized();
	
	UserMessageHookData data;
	data.msg_id = msg_id;
	data.hook = hook;
	data.intercept = intercept;
	data.post = post;
	
	s_userMessageHooks.PushArray(data);
	
	if (PSM_IsEnabled())
		data.Toggle(true);
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
	
	if (PSM_IsEnabled())
		data.Toggle(true);
}

/**
 * Registers a multi-target filter.
 *
 * @param pattern		Pattern to match (case sensitive).
 * @param filter		Filter function.
 * @param phrase		Descriptive phrase to display on successful match.
 * @param phraseIsML	True if phrase is multi-lingual, false otherwise.
 * @error				Plugin state manager not initialized.
 */
stock void PSM_AddMultiTargetFilter(const char[] pattern, MultiTargetFilter filter, const char[] phrase, bool phraseIsML)
{
	ThrowIfNotInitialized();
	
	MultiTargetFilterData data;
	strcopy(data.pattern, sizeof(data.pattern), pattern);
	data.filter = filter;
	strcopy(data.phrase, sizeof(data.phrase), phrase);
	data.phraseIsML = phraseIsML;
	
	s_multiTargetFilters.PushArray(data);
	
	if (PSM_IsEnabled())
		data.Toggle(true);
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
	
	if (PSM_IsEnabled())
		data.Toggle(true);
}

/**
 * Registers a normal sound hook.
 *
 * @param hook			Function to use as a hook.
 * @error				Plugin state manager not initialized.
 */
stock void PSM_AddNormalSoundHook(NormalSHook hook)
{
	ThrowIfNotInitialized();
	
	NormalSHookData data;
	data.hook = hook;
	
	s_normalSHooks.PushArray(data);
	
	if (PSM_IsEnabled())
		data.Toggle(true);
}

//=============================================================================
// Source Scramble
//=============================================================================

#if defined __sourcescramble_ext_included
/**
 * Registers a memory patch from game config.
 *
 * @param name			Name of the patch in a "MemPatches" section to load.
 * @error				Invalid gamedata or plugin state manager not initialized.
 */
stock MemoryPatch PSM_AddMemoryPatchFromConf(const char[] name)
{
	ThrowIfNotInitialized(true);
	
	MemoryPatch patch = MemoryPatch.CreateFromConf(s_gamedata, name);
	if (patch.Validate())
	{
		MemoryPatchData data;
		strcopy(data.name, sizeof(data.name), name);
		data.patch = patch;
		
		s_memoryPatches.PushArray(data);
		
		if (PSM_IsEnabled())
			data.Toggle(true);
		
		return patch;
	}
	else
	{
		ThrowError("Failed to validate memory patch '%s'", name);
		return null;
	}
}
#endif

//*******************************************************************************************************************************************************

static void ThrowIfNotInitialized(bool requireGameData = false)
{
	if (!PSM_IsInitialized())
		ThrowError("The plugin state manager has not yet been initialized! You must call 'PSM_Init' to use this functionality.");
	
	if (requireGameData && !s_gamedata)
		ThrowError("GameData handle is not initialized! You must pass a valid handle to 'PSM_Init' to use this functionality.");
}

static void InitializeStateConVar()
{
	if (s_stateConVar)
		return;
	
	s_stateConVar = FindConVar(s_conVarName);
	if (!s_stateConVar)
	{
		s_stateConVar = CreateConVar(s_conVarName, "1", "Enable the plugin?");
		LogMessage("ConVar '%s' has been automatically created.", s_conVarName);
	}
	
	s_stateConVar.AddChangeHook(OnEnableConVarChanged);
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

static bool PSM_ShouldEnable()
{
	if (s_shouldEnableFwd.FunctionCount > 0)
	{
		bool value = true;

		Call_StartForward(s_shouldEnableFwd);
		if (Call_Finish(value) != SP_ERROR_NONE || !value)
			return false;
	}

	return true;
}

static int EntIndexToEntRefSafe(int entity)
{
	return IsValidEdict(entity) ? EntIndexToEntRef(entity) : entity;
}

static void ToggleEnforcedConVar(int index, bool enable)
{
	EnforcedConVarData data;
	if (!s_enforcedConVars.GetArray(index, data))
		return;

	ConVar convar = FindConVar(data.name);
	if (!convar)
		return;

	if (enable)
	{
		convar.GetString(data.initialValue, sizeof(data.initialValue));
		s_enforcedConVars.SetArray(index, data);

		convar.SetString(data.value);
		convar.AddChangeHook(OnEnforcedConVarChanged);
	}
	else
	{
		convar.RemoveChangeHook(OnEnforcedConVarChanged);
		convar.SetString(data.initialValue);
	}
}

static void ToggleSyncedConVar(int index, bool enable)
{
	SyncedConVarData data;
	if (!s_syncedConVars.GetArray(index, data))
		return;

	ConVar target = FindConVar(data.name);
	if (!target)
		return;

	if (enable)
	{
		target.GetString(data.initialValue, sizeof(data.initialValue));

		char value[64];
		data.sourceConVar.GetString(value, sizeof(value));
		target.SetString(value);

		data.sourceConVar.AddChangeHook(OnSourceConVarChanged);
		s_syncedConVars.SetArray(index, data);

		target.AddChangeHook(OnSyncedConVarChanged);
	}
	else
	{
		target.RemoveChangeHook(OnSyncedConVarChanged);
		target.SetString(data.initialValue);

		data.sourceConVar.RemoveChangeHook(OnSourceConVarChanged);
	}
}

static void OnEnforcedConVarChanged(ConVar convar, const char[] oldValue, const char[] newValue)
{
	char name[64];
	convar.GetName(name, sizeof(name));

	int index = s_enforcedConVars.FindString(name);
	if (index == -1)
		return;

	EnforcedConVarData data;
	if (s_enforcedConVars.GetArray(index, data))
	{
		if (!StrEqual(newValue, data.value))
		{
			strcopy(data.initialValue, sizeof(data.initialValue), newValue);
			s_enforcedConVars.SetArray(index, data);

			convar.SetString(data.value);
		}
	}
}

static void OnSyncedConVarChanged(ConVar convar, const char[] oldValue, const char[] newValue)
{
	char name[64];
	convar.GetName(name, sizeof(name));

	int index = s_syncedConVars.FindString(name);
	if (index == -1)
		return;

	SyncedConVarData data;
	if (s_syncedConVars.GetArray(index, data))
	{
		char expected[64];
		data.sourceConVar.GetString(expected, sizeof(expected));

		if (!StrEqual(newValue, expected))
		{
			strcopy(data.initialValue, sizeof(data.initialValue), newValue);
			s_syncedConVars.SetArray(index, data);

			convar.SetString(expected);
		}
	}
}

static void OnSourceConVarChanged(ConVar convar, const char[] oldValue, const char[] newValue)
{
	int index = s_syncedConVars.FindValue(convar, SyncedConVarData::sourceConVar);
	if (index == -1)
		return;

	SyncedConVarData data;
	if (s_syncedConVars.GetArray(index, data))
	{
		ConVar target = FindConVar(data.name);
		if (!target)
			return;

		target.RemoveChangeHook(OnSyncedConVarChanged);
		target.SetString(newValue);
		target.AddChangeHook(OnSyncedConVarChanged);
	}
}
