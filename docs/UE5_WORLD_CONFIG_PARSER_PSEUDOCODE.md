# UE5 World Config Parser Pseudocode

## 1. 목적

UE5 쪽 `worldConfig` parser를 구현할 때 참고할 의사코드다. 이 문서는 실제 UE5 C++ 또는 Blueprint 코드가 아니며, AI Backend repo에는 UE5 코드를 생성하지 않는다.

## 2. Pseudocode

```text
ParseHandoffResponse(response):
    ValidateHandoffSuccess(response)
    config = ExtractWorldConfig(response)
    SpawnMap(config.map)
    SpawnRobot(config.robot)
    SpawnGoal(config.robot.goal)
    SpawnObstacles(config.obstacles)
    SpawnPedestrians(config.pedestrians)
    ApplyRuntime(config.runtime)
    LogScenarioIds(config.worldId, config.scenarioId)

ValidateHandoffSuccess(response):
    if response.success != true:
        stop and log response.error
    if response.worldConfig is null:
        stop and log missing worldConfig
    if response.validation.contractValidationPassed != true:
        stop and log validation failure

ExtractWorldConfig(response):
    return response.worldConfig

SpawnMap(map):
    create sidewalk/map mesh using map.lengthCm and map.sidewalkWidthCm
    apply surface material from map.surfaceCondition
    apply slope using map.slopeDegree

SpawnRobot(robot):
    spawn DeliveryRobot actor at robot.spawn
    attach policyId to controller/debug state

SpawnGoal(goal):
    spawn Goal marker actor at goal

SpawnObstacles(obstacles):
    for obstacle in obstacles:
        if obstacle.type == "Kickboard":
            spawn Kickboard obstacle actor at obstacle.position
        else:
            spawn generic obstacle actor at obstacle.position
        store blockingRatio for debug metric

SpawnPedestrians(pedestrians):
    for pedestrian in pedestrians:
        spawn Pedestrian actor at pedestrian.spawn
        set target to pedestrian.goal
        set movement speed from pedestrian.speedKmh
        if pedestrian.behavior == "Crossing":
            enable crossing movement behavior

ApplyRuntime(runtime):
    set max simulation duration from runtime.maxDurationSec
    configure replay capture from runtime.captureReplay
    configure event log from runtime.emitEventLog
```

## 3. 주의

좌표는 UE5 world coordinate 기준이고, 거리는 cm 단위를 사용한다.
# EpisodeSpec parser pseudocode

```text
load EpisodeSpec JSON
assert schema == "episode_actor_spawn_mvp"
assert units.distance == "m"
create ground regions from ground_model.regions
spawn robot from actors.robot.transform
assign robot route.goal_m
create paths from paths[]
spawn pedestrian actors and attach path_id
spawn static obstacles by prop_id
apply properties.semantic_type for temporary semantic mapping
```
