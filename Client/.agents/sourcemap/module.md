# Runtime Module

Covers: `ProtoRobotSim.uproject`, `Source/*.Target.cs`, `Source/ProtoRobotSim/ProtoRobotSim.Build.cs`

## Entry Points
- `ProtoRobotSim.uproject`: UE 5.7, runtime module, enabled plugins
- `Source/ProtoRobotSim.Target.cs`: game target
- `Source/ProtoRobotSimEditor.Target.cs`: editor target
- `Source/ProtoRobotSim/ProtoRobotSim.Build.cs`: dependencies

## Notes
- Module: `ProtoRobotSim`
- Public deps: `Core`, `CoreUObject`, `Engine`, `InputCore`, `EnhancedInput`, `ChaosVehicles`, `Json`, `JsonUtilities`
