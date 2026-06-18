// CricketUI: the presentation layer. A pure CONSUMER of gameplay state — it reads
// the Match Engine, ball physics, bowling/batting controllers and the replay system
// and visualises them. It owns no gameplay state and never writes back into a
// simulation system. Sits at the very top of the one-directional module graph:
//
//   CricketUI -> CricketSim -> CricketGameplay -> CricketPhysics -> Engine/Core
//
// UMG provides the player-facing HUD; Common UI provides the lightweight, scalable
// widget bases (UCommonUserWidget) so the layer can grow toward a full broadcast
// presentation later without rework.

using UnrealBuildTool;

public class CricketUI : ModuleRules
{
	public CricketUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UMG",                // player-facing HUD widgets (UUserWidget/UMG)
			"CommonUI",           // UCommonUserWidget base (exposed in our public widget headers)
			"CricketPhysics",     // SI types: ball state, aero, bounce, replay, bowling/batting types
			"CricketGameplay",    // ball physics / bowling / batting / replay components
			"CricketSim"          // the Match Engine (T20 rules) + scoring types
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore"
		});
	}
}
