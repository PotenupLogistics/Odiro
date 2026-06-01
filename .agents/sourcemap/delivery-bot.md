# Delivery Bot

Covers: `Source/ProtoRobotSim/Public/DeliveryBot`, `Source/ProtoRobotSim/Private/DeliveryBot`, delivery bot structs in `Source/ProtoRobotSim/Public/Shared/Struct`

## Entry Points
- `Public/DeliveryBot/Actor`: robot actors, path points, grid bounds
- `Public/DeliveryBot/Component`: movement, drive, grid agent, path follow, avoidance, policy
- `Public/DeliveryBot/Subsystem/DeliveryBot_GridSubsystem.h`: grid cells and world subsystem
- `Public/Shared/Struct/DeliveryBot*.h`: pathing, movement, drive, queue data

## Notes
- Surface: `ADeliveryBot_SimpleMesh`, `ADeliveryBot_ChaosActor`, `UDeliveryBot_*`, `FDeliveryBot*`
- Robot movement relies on grid bounds and walkable cells for route tests
