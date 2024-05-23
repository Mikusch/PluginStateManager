#pragma semicolon 1
#pragma newdecls required

#include <dhooks>

#define PLUGINMAN_MAX_EVENT_NAME_LENGTH		32
#define PLUGINMAN_MAX_DETOUR_NAME_LENGTH	64

typedef PluginToggledHook = function void(bool enabled);

enum struct PluginManagerEventData
{
	char name[PLUGINMAN_MAX_EVENT_NAME_LENGTH];
	EventHook callback;
	EventHookMode mode;
}

enum struct PluginManagerDetourData
{
	char name[PLUGINMAN_MAX_DETOUR_NAME_LENGTH];
	DynamicDetour detour;
	DHookCallback callbackPre;
	DHookCallback callbackPost;
}

static bool s_isInitialized;
static bool s_isEnabled;
static ConVar s_enableConvar;
static GameData s_gamedata;
static PrivateForward s_toggledFwd;

static ArrayList s_eventData; // ArrayList<PluginManagerEventData>
static ArrayList s_detourData; // ArrayList <PluginManagerDetourData>

methodmap PluginManager
{
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
		s_eventData = new ArrayList(sizeof(PluginManagerEventData));
		s_detourData = new ArrayList(sizeof(PluginManagerDetourData));
		
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
			PluginManagerEventData data;
			if (s_eventData.GetArray(i, data))
			{
				if (enable)
					HookEvent(data.name, data.callback, data.mode);
				else
					UnhookEvent(data.name, data.callback, data.mode);
			}
		}
		
		// Detours
		for (int i = 0; i < s_detourData.Length; i++)
		{
			PluginManagerDetourData data;
			if (s_detourData.GetArray(i, data))
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
			
			PluginManagerEventData data;
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
	 * @param name          Name of event.
	 * @param callback      An EventHook function pointer.
	 * @param mode          Optional EventHookMode determining the type of hook.
	 * @return				True on success, false otherwise.
	 */
	public static bool RemoveEventHook(const char[] name, EventHook callback, EventHookMode mode = EventHookMode_Post)
	{
		EnsureInitializedState();
		
		for (int i = 0; i < s_eventData.Length; i++)
		{
			PluginManagerEventData data;
			if (s_eventData.GetArray(i, data) && StrEqual(name, data.name) && callback == data.callback && mode == data.mode)
			{
				s_eventData.Erase(i);
				return true;
			}
		}
		
		LogError("Failed to find event '%s' in the registry", name);
		return false;
	}
	
	/**
	 * Registers a dynamic detour to get enabled when the plugin gets enabled.
	 * This internally uses DynamicDetour.FromConf, so the gamedata file must contain a "Functions" section.
	 *
	 * @param name          	Name of the function in a "Functions" section to load.
	 * @param callbackPre		Optional pre callback function.
	 * @param callbackPost		Optional post callback function.
	 * @return					True on success, false otherwise.
	 */
	public static bool AddDynamicDetour(const char[] name, DHookCallback callbackPre = INVALID_FUNCTION, DHookCallback callbackPost = INVALID_FUNCTION)
	{
		EnsureInitializedState(true);
		
		DynamicDetour hDetour = DynamicDetour.FromConf(s_gamedata, name);
		if (hDetour)
		{
			PluginManagerDetourData data;
			strcopy(data.name, sizeof(data.name), name);
			data.detour = hDetour;
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
	 * Unregisters a dynamic detour to get enabled when the plugin gets enabled.
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
			PluginManagerDetourData data;
			if (s_detourData.GetArray(i, data) && StrEqual(name, data.name) && callbackPre == data.callbackPre && callbackPost == data.callbackPost)
			{
				s_detourData.Erase(i);
				return true;
			}
		}
		
		LogError("Failed to find detour '%s' in the registry", name);
		return false;
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

//// PRIVATE

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
