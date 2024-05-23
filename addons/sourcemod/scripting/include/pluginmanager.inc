#pragma semicolon 1
#pragma newdecls required

#define PLUGINMAN_MAX_EVENT_NAME_LENGTH	32
#define PLUGINMAN_MAX_COMMAND_LENGTH	512

typedef PluginToggledHook = function void(bool enabled);

enum struct PluginManagerEventData
{
	char name[PLUGINMAN_MAX_EVENT_NAME_LENGTH];
	EventHook callback;
	EventHookMode mode;
}

static bool s_initialized;
static bool s_isEnabled;
static ConVar s_enableConvar;
static GameData s_gamedata;
static PrivateForward s_toggledFwd;

static ArrayList s_eventData; // ArrayList<PluginManagerEventData>

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
		s_enableConvar = convar;
		s_enableConvar.AddChangeHook(OnEnableConVarChanged);
		
		s_gamedata = view_as<GameData>(CloneHandle(gamedata));
		s_toggledFwd = new PrivateForward(ET_Ignore, Param_Cell);
		s_eventData = new ArrayList(sizeof(PluginManagerEventData));
		
		s_initialized = true;
	}
	
	/**
	 * Toggles the enabled state of the plugin if it isn't in that state already.
	 * Plugins should call this in OnConfigsExecuted() to determine whether to enable the plugin on first load.
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
	 * Plugins generally want to use TogglePluginIfNecessary() to respect the convar.
	 *
	 * @param enable		If true, the plugin will be enabled. If false, the plugin will be disabled.
	 * @error				If the plugin was already in that state.
	 */
	public static void TogglePlugin(bool enable)
	{
		EnsureInitializedState();
		
		if (PluginManager.IsEnabled() == enable)
		{
			LogError("The plugin is already in that state!");
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
				{
					HookEvent(data.name, data.callback, data.mode);
					PrintToServer("Hooked!");
				}
				else
				{
					UnhookEvent(data.name, data.callback, data.mode);
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
			LogError("Event %s doesn't exist!", name);
			return false;
		}
	}
	
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
		
		return false;
	}
	
	public static bool IsInitialized()
	{
		return s_initialized;
	}
	
	public static bool IsEnabled()
	{
		return s_isEnabled;
	}
}

//// PRIVATE

static void EnsureInitializedState()
{
	if (!PluginManager.IsInitialized())
	{
		SetFailState("PluginManager is not initialized! Call PluginManager.Init() before using any of its functions!");
	}
}

static void OnEnableConVarChanged(ConVar convar, const char[] oldValue, const char[] newValue)
{
	PluginManager.TogglePluginIfNecessary();
}
