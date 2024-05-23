#pragma semicolon 1
#pragma newdecls required

#include <dhooks>

#define PLUGINMAN_MAX_EVENT_NAME_LENGTH		32
#define PLUGINMAN_MAX_DETOUR_NAME_LENGTH	64

typedef PluginToggledHook = function void(bool enabled);

enum struct EventData
{
	char name[PLUGINMAN_MAX_EVENT_NAME_LENGTH];
	EventHook callback;
	EventHookMode mode;
}

enum struct DetourData
{
	char name[PLUGINMAN_MAX_DETOUR_NAME_LENGTH];
	DynamicDetour detour;
	DHookCallback callbackPre;
	DHookCallback callbackPost;
}

enum struct DHookData
{
	char name[PLUGINMAN_MAX_DETOUR_NAME_LENGTH];
	DynamicHook hook;
}

enum struct DHookIdData
{
	int hookid;
	DynamicHook hook;
}

static bool s_isInitialized;
static bool s_isEnabled;
static ConVar s_enableConvar;
static GameData s_gamedata;
static PrivateForward s_toggledFwd;

static ArrayList s_eventData; // ArrayList<EventData>
static ArrayList s_detourData; // ArrayList <DetourData>
static ArrayList s_dynamicHookData; // ArrayList<DHookData>
static ArrayList s_dynamicHookIds; // ArrayList<DHookIdData>

methodmap PluginManager
{
	//=============================================================================
	// Core Features
	//=============================================================================
	
	/**
	 * Initializes the plugin manager.
	 *
	 * @param convar		The convar used to determine whether the plugin should be enabled.
	 * @param gamedata		Optional gamedata handle.
	 */
	public static void Init(ConVar convar, GameData gamedata = null)
	{
		if (s_isInitialized)
		{
			LogError("PluginManager can only be initialized once per plugin!");
			return;
		}
		
		s_enableConvar = convar;
		s_enableConvar.AddChangeHook(OnEnableConVarChanged);
		
		if (gamedata)
			s_gamedata = view_as<GameData>(CloneHandle(gamedata));
		
		s_toggledFwd = new PrivateForward(ET_Ignore, Param_Cell);
		s_eventData = new ArrayList(sizeof(EventData));
		s_detourData = new ArrayList(sizeof(DetourData));
		s_dynamicHookData = new ArrayList(sizeof(DHookData));
		s_dynamicHookIds = new ArrayList(sizeof(DHookIdData));
		
		s_isInitialized = true;
	}
	
	/**
	 * Convenience function that toggles the enabled state of the plugin if it isn't in that state already.
	 * Plugins should call this function in a OnConfigsExecuted forward to determine whether to enable themselves on first load.
	 */
	public static void TogglePluginIfNecessary()
	{
		EnsureInitializedState();
		
		if (PluginManager.IsEnabled() != s_enableConvar.BoolValue)
		{
			PluginManager.TogglePlugin(s_enableConvar.BoolValue);
		}
	}
	
	/**
	 * Sets the enabled state of the plugin.
	 * Plugins generally want to use PluginManager.TogglePluginIfNecessary to respect the convar.
	 *
	 * @param enable		If true, the plugin will be enabled. If false, the plugin will be disabled.
	 * @error				If the plugin was already in that state.
	 */
	public static void TogglePlugin(bool enable)
	{
		EnsureInitializedState();
		
		if (PluginManager.IsEnabled() == enable)
		{
			LogError("Attempted to set plugin into state it already is in!");
			return;
		}
		
		s_isEnabled = enable;
		
		// Events
		for (int i = 0; i < s_eventData.Length; i++)
		{
			EventData data;
			if (s_eventData.GetArray(i, data))
			{
				ToggleEvent(data, enable);
			}
		}
		
		// Detours
		for (int i = 0; i < s_detourData.Length; i++)
		{
			DetourData data;
			if (s_detourData.GetArray(i, data))
			{
				ToggleDetour(data, enable);
			}
		}
		
		// Dynamic Hooks
		if (!enable)
		{
			for (int i = s_dynamicHookIds.Length - 1; i >= 0; i--)
			{
				int hookid = s_dynamicHookIds.Get(i, DHookIdData::hookid);
				DynamicHook.RemoveHook(hookid);
			}
		}
		
		Call_StartForward(s_toggledFwd);
		Call_PushCell(enable);
		Call_Finish();
	}
	
	/**
	 * Adds a callback that is triggered when the enabled state of the plugin changes.
	 *
	 * @param func		Function callback.
	 * @return			True on success, false otherwise.
	 */
	public static bool AddPluginToggledHook(PluginToggledHook hook)
	{
		EnsureInitializedState();
		
		return s_toggledFwd.AddFunction(null, hook);
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
	 * @return				True on success, false otherwise.
	 * @error				Invalid event name.
	 */
	public static bool AddEventHook(const char[] name, EventHook callback, EventHookMode mode = EventHookMode_Post)
	{
		EnsureInitializedState();
		
		Event event = CreateEvent(name, true);
		if (event)
		{
			event.Cancel();
			
			EventData data;
			strcopy(data.name, sizeof(data.name), name);
			data.callback = callback;
			data.mode = mode;
			
			s_eventData.PushArray(data);
			return true;
		}
		else
		{
			LogError("Failed to find event '%s'", name);
			return false;
		}
	}
	
	/**
	 * Unregisters an event to be hooked when the plugin gets enabled.
	 *
	 * @param name			Name of event.
	 * @param callback		An EventHook function pointer.
	 * @param mode			Optional EventHookMode determining the type of hook.
	 * @return				True on success, false otherwise.
	 */
	public static bool RemoveEventHook(const char[] name, EventHook callback, EventHookMode mode = EventHookMode_Post)
	{
		EnsureInitializedState();
		
		for (int i = 0; i < s_eventData.Length; i++)
		{
			EventData data;
			if (s_eventData.GetArray(i, data) && StrEqual(name, data.name) && callback == data.callback && mode == data.mode)
			{
				if (PluginManager.IsEnabled())
					ToggleEvent(data, false);
				
				s_eventData.Erase(i);
				return true;
			}
		}
		
		LogError("Failed to find event '%s' in the registry", name);
		return false;
	}
	
	//=============================================================================
	// Detours
	//=============================================================================
	
	/**
	 * Registers a dynamic detour.
	 * This internally uses DynamicDetour.FromConf, so the name must be present in the "Functions" section of the gamedata file.
	 *
	 * @param name          	Name of the function in a "Functions" section to load.
	 * @param callbackPre		Optional pre callback function.
	 * @param callbackPost		Optional post callback function.
	 * @return					True on success, false otherwise.
	 */
	public static bool AddDynamicDetour(const char[] name, DHookCallback callbackPre = INVALID_FUNCTION, DHookCallback callbackPost = INVALID_FUNCTION)
	{
		EnsureInitializedState(true);
		
		DynamicDetour detour = DynamicDetour.FromConf(s_gamedata, name);
		if (detour)
		{
			DetourData data;
			strcopy(data.name, sizeof(data.name), name);
			data.detour = detour;
			data.callbackPre = callbackPre;
			data.callbackPost = callbackPost;
			
			s_detourData.PushArray(data);
			return true;
		}
		else
		{
			LogError("Failed to create detour setup handle '%s'", name);
			return false;
		}
	}
	
	/**
	 * Unregisters a dynamic detour.
	 *
	 * @param name          	Name of the function in a "Functions" section to load.
	 * @param callbackPre		Optional pre callback function.
	 * @param callbackPost		Optional post callback function.
	 * @return					True on success, false otherwise.
	 */
	public static bool RemoveDynamicDetour(const char[] name, DHookCallback callbackPre = INVALID_FUNCTION, DHookCallback callbackPost = INVALID_FUNCTION)
	{
		EnsureInitializedState();
		
		for (int i = 0; i < s_detourData.Length; i++)
		{
			DetourData data;
			if (s_detourData.GetArray(i, data) && StrEqual(name, data.name) && callbackPre == data.callbackPre && callbackPost == data.callbackPost)
			{
				if (PluginManager.IsEnabled())
					ToggleDetour(data, false);
				
				s_detourData.Erase(i);
				return true;
			}
		}
		
		LogError("Failed to find detour '%s' in the registry", name);
		return false;
	}
	
	//=============================================================================
	// Dynamic Hooks
	//=============================================================================
	
	/**
	 * Registers a dynamic hook.
	 * This internally uses DynamicHook.FromConf, so the name must be present in the "Functions" section of the gamedata file.
	 *
	 * @param name		Name of the function in a "Functions" section to load.
	 * @return			The DynamicHook on success, null otherwise.
	 */
	public static DynamicHook AddDynamicHook(const char[] name)
	{
		EnsureInitializedState(true);
		
		DynamicHook hook = DynamicHook.FromConf(s_gamedata, name);
		if (hook)
		{
			DHookData data;
			strcopy(data.name, sizeof(data.name), name);
			data.hook = hook;
			
			s_dynamicHookData.PushArray(data);
			return hook;
		}
		else
		{
			LogError("Failed to create hook setup handle '%s'", name);
			return null;
		}
	}
	
	/**
	 * Unregisters a dynamic hook.
	 *
	 * @param name          	The name of the registered dynamic hook.
	 * @return					True on success, false otherwise.
	 */
	public static bool RemoveDynamicHook(const char[] name)
	{
		EnsureInitializedState();
		
		for (int i = 0; i < s_dynamicHookData.Length; i++)
		{
			DHookData data;
			if (s_dynamicHookData.GetArray(i, data) && StrEqual(name, data.name))
			{
				// Unhook any entities that belong to this hook
				for (int j = s_dynamicHookIds.Length - 1; j >= 0; j--)
				{
					if (s_dynamicHookIds.Get(j, DHookIdData::hook) == data.hook)
						DynamicHook.RemoveHook(s_dynamicHookIds.Get(j, DHookIdData::hookid));
				}
				
				s_dynamicHookData.Erase(i);
				return true;
			}
		}
		
		LogError("Failed to find dynamic hook '%s' in the registry", name);
		return false;
	}
	
	/**
	 * Finds a dynamic hook by name and hooks the provided entity.
	 *
	 * @param entity		Entity index to hook on.
	 * @param name			The name of the registered dynamic hook.
	 * @param mode			The desired hook mode - pre or post. 
	 * @param callback		Callback function.
	 * @return				A hookid on success, INVALID_HOOK_ID otherwise.
	 */
	public static int DynamicHookEntityByName(int entity, const char[] name, HookMode mode, DHookCallback callback)
	{
		EnsureInitializedState();
		
		int index = s_dynamicHookData.FindString(name, DHookData::name);
		if (index == -1)
		{
			LogError("Failed to find dynamic hook for '%s'", name);
			return false;
		}
		
		DynamicHook hook = s_dynamicHookData.Get(index, DHookData::hook);
		return PluginManager.DynamicHookEntity(entity, hook, mode, callback);
	}
	
	/**
	 * Hooks the provided entity.
	 *
	 * @param entity		Entity index to hook on.
	 * @param hook			The DynamicHook handle.
	 * @param mode			The desired hook mode - pre or post. 
	 * @param callback		Callback function.
	 * @return				A hookid on success, INVALID_HOOK_ID otherwise.
	 */
	public static int DynamicHookEntity(int entity, DynamicHook hook, HookMode mode, DHookCallback callback)
	{
		EnsureInitializedState();
		
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
	 *
	 * @param hook			The DynamicHook handle.
	 * @param mode			The desired hook mode - pre or post. 
	 * @param callback		Callback function.
	 * @return				A hookid on success, INVALID_HOOK_ID otherwise.
	 */
	public static int DynamicHookGameRules(DynamicHook hook, HookMode mode, DHookCallback callback)
	{
		EnsureInitializedState();
		
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
	
	public static bool IsInitialized()
	{
		return s_isInitialized;
	}
	
	public static bool IsEnabled()
	{
		return s_isEnabled;
	}
}

//*******************************************************************************************************************************************************

static void EnsureInitializedState(bool requireGameData = false)
{
	if (!PluginManager.IsInitialized())
	{
		SetFailState("PluginManager is not initialized! Call 'PluginManager.Init' to use this function!");
	}
	
	if (requireGameData && !s_gamedata)
	{
		SetFailState("GameData handle is not initialized! Pass a valid GameData handle to 'PluginManager.Init' to use this function!");
	}
}

static void OnEnableConVarChanged(ConVar convar, const char[] oldValue, const char[] newValue)
{
	PluginManager.TogglePluginIfNecessary();
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
