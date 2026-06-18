// CricketAudio: the reactive sound layer. A pure CONSUMER of gameplay/physics
// events — it listens to the simulation and plays sound; it never writes back into
// a simulation system and never influences an outcome. Sits at the top of the
// one-directional module graph alongside CricketUI:
//
//   CricketAudio -> CricketSim -> CricketGameplay -> CricketPhysics -> Engine/Core
//
// All sound playback uses engine-portable audio (UGameplayStatics / UAudioComponent),
// so the layer builds and runs identically on macOS (Metal) and Windows.

using UnrealBuildTool;

public class CricketAudio : ModuleRules
{
	public CricketAudio(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",             // audio: USoundBase, UAudioComponent, UGameplayStatics
			"CricketPhysics",     // SI report types: bat impact, bounce, release, fielding
			"CricketGameplay",    // ball physics / bowling / batting / fielder / replay components
			"CricketSim"          // the Match Engine (delegates: ball applied, wicket, over, state)
		});
	}
}
