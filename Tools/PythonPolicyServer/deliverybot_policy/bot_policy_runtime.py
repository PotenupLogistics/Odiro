from __future__ import annotations

from dataclasses import dataclass
from typing import Any

from deliverybot_policy.bot_policy_contract import (
    CONTEXT_GRID_MAP,
    CONTEXT_LIDAR_SCAN,
    CONTEXT_OBSERVED_OBJECTS,
    CONTEXT_ROBOT_STATE,
    MERGE_MODE_REPLACE_ALL,
    MERGE_MODE_REPLACE_KIND,
    STATUS_ERROR,
    STATUS_OK,
    Action,
    BotPolicy,
    Config,
    Context,
    ContextItem,
    ContextItemRef,
    ContextRequirement,
    Decision,
    DecisionDebug,
    DecisionRequest,
    DriveConfig,
    EpisodeSetup,
    Goal,
    InitializeResult,
    LidarConfig,
    MotionControlConfig,
    PolicyError,
    PolicySpec,
    Pose,
    SetConfigResult,
    SetContextResult,
    Vector3,
    toPlainData,
)
from deliverybot_policy.catalog import build_default_policy_spec, load_policy_catalog, normalize_policy_spec
from deliverybot_policy.registry import build_runtime_policy_response


@dataclass(slots=True)
class _StoredContextItem:
    item: ContextItem
    acceptedContextVersion: int


class RuntimeBotPolicy(BotPolicy):
    def __init__(
        self,
        policyCatalog: dict[str, Any] | None = None,
        plannerMode: str = "auto",
        dwaMode: str = "policy",
        rightOfWayMode: str = "policy",
    ) -> None:
        self.policyCatalog = policyCatalog if isinstance(policyCatalog, dict) else load_policy_catalog()
        self.policySpec = build_default_policy_spec(self.policyCatalog)
        self.plannerMode = normalizePlannerMode(plannerMode)
        self.dwaMode = normalizeDwaMode(dwaMode)
        self.rightOfWayMode = str(rightOfWayMode or "policy")

        self.config: Config | None = None
        self.configInfo: dict[str, Any] = {}
        self.contextItems: dict[tuple[str, str], _StoredContextItem] = {}
        self.gridInfo: dict[str, Any] = {}
        self.gridCellLookup: dict[tuple[int, int], dict[str, Any]] = {}
        self.gridSummary: dict[str, Any] = {}
        self.episodeInfo: dict[str, Any] = {}
        self.policyRuntimeState: dict[str, Any] = {}

        self.episodeVersion = 0
        self.configVersion = 0
        self.contextVersion = 0
        self.gridVersion = 0

    async def setConfig(self, config: Config) -> SetConfigResult:
        if not isinstance(config, Config):
            return SetConfigResult(
                status=STATUS_ERROR,
                accepted=False,
                configVersion=self.configVersion,
                error=PolicyError("INVALID_CONFIG", "config must be Config", retryable=False),
            )

        self.config = config
        self.configInfo = configToLegacyConfigInfo(config)
        legacy_policy_spec = config.parameters.get("_legacyPolicySpec")
        if isinstance(legacy_policy_spec, dict):
            self.policySpec = normalize_policy_spec(legacy_policy_spec, self.policyCatalog)
        elif config.policy is not None:
            self.policySpec = normalize_policy_spec(policySpecToLegacyDict(config.policy), self.policyCatalog)

        self.configVersion += 1
        return SetConfigResult(
            status=STATUS_OK,
            accepted=True,
            configVersion=self.configVersion,
            requiredContext=buildDefaultContextRequirements(),
        )

    async def setContext(self, context: Context) -> SetContextResult:
        if not isinstance(context, Context):
            return SetContextResult(
                status=STATUS_ERROR,
                accepted=False,
                contextVersion=self.contextVersion,
                gridVersion=self.gridVersion,
                error=PolicyError("INVALID_CONTEXT", "context must be Context", retryable=False),
            )

        if context.mergeMode == MERGE_MODE_REPLACE_ALL:
            self.contextItems.clear()
        elif context.mergeMode == MERGE_MODE_REPLACE_KIND:
            kinds_to_replace = {item.kind for item in context.items}
            self.contextItems = {
                key: stored
                for key, stored in self.contextItems.items()
                if key[0] not in kinds_to_replace
            }

        accepted_items: list[ContextItemRef] = []
        accepted_context_items: list[ContextItem] = []
        rejected_items = []

        for item in context.items:
            if not isinstance(item, ContextItem) or not item.kind or not item.name:
                rejected_items.append(
                    {
                        "name": getattr(item, "name", ""),
                        "kind": getattr(item, "kind", ""),
                        "version": int(getattr(item, "version", 0) or 0),
                        "reason": "invalid_context_item",
                    }
                )
                continue

            if item.kind == CONTEXT_GRID_MAP:
                grid_payload = item.payload if isinstance(item.payload, dict) else {}
                if not grid_payload:
                    rejected_items.append(
                        {
                            "name": item.name,
                            "kind": item.kind,
                            "version": item.version,
                            "reason": "grid_payload_missing",
                        }
                    )
                    continue
                self.gridVersion += 1
                self.gridInfo = grid_payload
                self.gridCellLookup = buildGridCellLookup(grid_payload)
                self.gridSummary = buildGridSummary(grid_payload, self.gridVersion)

            accepted_context_items.append(item)
            accepted_items.append(contextItemToRef(item, context.worldTimeSeconds))

        accepted = bool(accepted_items)
        if accepted:
            self.contextVersion += 1
            for item in accepted_context_items:
                self.contextItems[(item.kind, item.name)] = _StoredContextItem(item, self.contextVersion)

        return SetContextResult(
            status=STATUS_OK if accepted else STATUS_ERROR,
            accepted=accepted,
            contextVersion=self.contextVersion,
            gridVersion=self.gridVersion,
            acceptedItems=accepted_items,
            rejectedItems=[rejectedContextItemFromDict(item) for item in rejected_items],
            error=None
            if accepted
            else PolicyError("INVALID_CONTEXT", "no context item accepted", retryable=True),
        )

    def initialize(self, setup: EpisodeSetup) -> InitializeResult:
        if not isinstance(setup, EpisodeSetup):
            return InitializeResult(
                status=STATUS_ERROR,
                accepted=False,
                episodeVersion=self.episodeVersion,
                configVersion=self.configVersion,
                contextVersion=self.contextVersion,
                gridVersion=self.gridVersion,
                resetApplied=False,
                error=PolicyError("INVALID_CONTEXT", "setup must be EpisodeSetup", retryable=False),
            )

        self.episodeVersion += 1
        if setup.resetPolicyState:
            self.policyRuntimeState.clear()

        goal = toPlainData(setup.goal) if setup.goal is not None else {}
        self.episodeInfo = {
            "episodeId": setup.episodeId,
            "robotInstanceId": setup.robotId,
            "start": toPlainData(setup.startPose),
            "goal": goal,
            "hasGoal": bool(goal.get("hasGoal", False)) if isinstance(goal, dict) else False,
        }

        return InitializeResult(
            status=STATUS_OK,
            accepted=True,
            episodeVersion=self.episodeVersion,
            configVersion=self.configVersion,
            contextVersion=self.contextVersion,
            gridVersion=self.gridVersion,
            resetApplied=setup.resetPolicyState,
        )

    def decide(self, request: DecisionRequest) -> Decision:
        if not isinstance(request, DecisionRequest):
            return Decision(
                sequence=0,
                status=STATUS_ERROR,
                episodeVersion=self.episodeVersion,
                configVersion=self.configVersion,
                contextVersion=self.contextVersion,
                gridVersion=self.gridVersion,
                action=None,
                error=PolicyError("DECISION_FAILED", "request must be DecisionRequest", retryable=False),
            )

        observation = self.buildObservation(request)
        runtime_context = {
            "observation": observation,
            "gridInfo": self.gridInfo,
            "gridCellLookup": self.gridCellLookup,
            "gridSummary": self.gridSummary,
            "episodeInfo": self.episodeInfo,
            "configInfo": self.configInfo,
            "policyCatalog": self.policyCatalog,
            "policySpec": self.policySpec,
            "policyRuntimeState": self.policyRuntimeState,
            "plannerMode": self.plannerMode,
            "dwaMode": self.dwaMode,
            "rightOfWayMode": self.rightOfWayMode,
        }
        response = build_runtime_policy_response(runtime_context)
        response["episodeVersion"] = self.episodeVersion
        response["configVersion"] = self.configVersion
        response["contextVersion"] = self.contextVersion
        response["gridVersion"] = self.gridVersion
        return decisionFromLegacyResponse(response, self.buildUsedContextRefs(request.worldTimeSeconds))

    def buildObservation(self, request: DecisionRequest) -> dict[str, Any]:
        observation: dict[str, Any] = {
            "sequence": request.sequence,
            "worldTimeSeconds": request.worldTimeSeconds,
        }
        robot_state = self.getLatestPayload(CONTEXT_ROBOT_STATE)
        lidar_scan = self.getLatestPayload(CONTEXT_LIDAR_SCAN)
        observed_objects = self.getLatestPayload(CONTEXT_OBSERVED_OBJECTS)

        if robot_state:
            observation["robotState"] = robot_state
        if lidar_scan:
            observation["sensorSequence"] = int(lidar_scan.get("sensorSequence", 0) or 0)
            observation["lidarRays"] = lidar_scan.get("rays", lidar_scan.get("lidarRays", []))
        if observed_objects:
            observation.setdefault("sensorSequence", int(observed_objects.get("sensorSequence", 0) or 0))
            observation["observedObjects"] = observed_objects.get(
                "objects",
                observed_objects.get("observedObjects", []),
            )

        if isinstance(self.configInfo.get("driveSpec"), dict):
            observation["driveSpec"] = self.configInfo["driveSpec"]
        if isinstance(self.configInfo.get("vehicleSpec"), dict):
            observation["vehicleSpec"] = self.configInfo["vehicleSpec"]

        return observation

    def getLatestPayload(self, kind: str) -> dict[str, Any]:
        latest: _StoredContextItem | None = None
        for (item_kind, _), stored in self.contextItems.items():
            if item_kind != kind:
                continue
            if latest is None or stored.item.version > latest.item.version:
                latest = stored

        payload = latest.item.payload if latest is not None else None
        return payload if isinstance(payload, dict) else {}

    def buildUsedContextRefs(self, worldTimeSeconds: float) -> dict[str, ContextItemRef]:
        used: dict[str, ContextItemRef] = {}
        for (kind, name), stored in self.contextItems.items():
            used[name] = contextItemToRef(stored.item, worldTimeSeconds)
            used[f"{kind}:{name}"] = used[name]

        return used


def buildDefaultContextRequirements() -> list[ContextRequirement]:
    return [
        ContextRequirement("robotState", CONTEXT_ROBOT_STATE, required=True, updateMode="EveryDecision"),
        ContextRequirement("grid", CONTEXT_GRID_MAP, required=True, updateMode="OnChange"),
        ContextRequirement("lidarScan", CONTEXT_LIDAR_SCAN, required=False, maxAgeSeconds=1.0, updateMode="OnChange"),
        ContextRequirement("observedObjects", CONTEXT_OBSERVED_OBJECTS, required=False, maxAgeSeconds=1.0),
    ]


def configToLegacyConfigInfo(config: Config) -> dict[str, Any]:
    result: dict[str, Any] = {
        key: value
        for key, value in config.parameters.items()
        if not str(key).startswith("_")
    }
    if config.drive is not None:
        result["driveSpec"] = toPlainData(config.drive)
    if config.lidar is not None:
        result["lidarSpec"] = toPlainData(config.lidar)
    if config.motionControl is not None:
        result["motionControlSpec"] = toPlainData(config.motionControl)
    if config.control is not None:
        result["controlSpec"] = toPlainData(config.control)

    return result


def policySpecToLegacyDict(policySpec: PolicySpec) -> dict[str, Any]:
    return {
        "catalogId": policySpec.catalogId,
        "catalogVersion": policySpec.catalogVersion,
        "enabledPolicies": [enabledPolicyToLegacyDict(policy) for policy in policySpec.enabledPolicies],
    }


def enabledPolicyToLegacyDict(policy: Any) -> dict[str, Any]:
    data = toPlainData(policy)
    return data if isinstance(data, dict) else {}


def buildGridCellLookup(gridInfo: dict[str, Any]) -> dict[tuple[int, int], dict[str, Any]]:
    cells = gridInfo.get("cells", [])
    safe_cells = cells if isinstance(cells, list) else []
    lookup: dict[tuple[int, int], dict[str, Any]] = {}
    for cell in safe_cells:
        if not isinstance(cell, dict):
            continue
        try:
            lookup[(int(cell.get("x", 0) or 0), int(cell.get("y", 0) or 0))] = cell
        except (TypeError, ValueError):
            continue

    return lookup


def buildGridSummary(gridInfo: dict[str, Any], gridVersion: int) -> dict[str, Any]:
    cells = gridInfo.get("cells", [])
    safe_cells = cells if isinstance(cells, list) else []
    return {
        "gridVersion": gridVersion,
        "gridSizeX": int(gridInfo.get("gridSizeX", 0) or 0),
        "gridSizeY": int(gridInfo.get("gridSizeY", 0) or 0),
        "cellSizeCm": float(gridInfo.get("cellSizeCm", 0.0) or 0.0),
        "cellCount": int(gridInfo.get("cellCount", len(safe_cells)) or len(safe_cells)),
        "walkableCount": sum(1 for cell in safe_cells if isinstance(cell, dict) and cell.get("areaType") == "Walkable"),
        "penaltyCount": sum(1 for cell in safe_cells if isinstance(cell, dict) and cell.get("areaType") == "Penalty"),
        "blockedCount": sum(
            1
            for cell in safe_cells
            if isinstance(cell, dict) and (bool(cell.get("blocked", False)) or cell.get("areaType") == "Blocked")
        ),
    }


def contextItemToRef(item: ContextItem, worldTimeSeconds: float) -> ContextItemRef:
    age_seconds = None
    if item.capturedWorldTimeSeconds is not None:
        age_seconds = max(worldTimeSeconds - item.capturedWorldTimeSeconds, 0.0)

    return ContextItemRef(
        name=item.name,
        kind=item.kind,
        version=item.version,
        capturedWorldTimeSeconds=item.capturedWorldTimeSeconds,
        ageSeconds=age_seconds,
    )


def rejectedContextItemFromDict(source: dict[str, Any]) -> Any:
    from deliverybot_policy.bot_policy_contract import RejectedContextItem

    return RejectedContextItem(
        name=str(source.get("name", "")),
        kind=str(source.get("kind", "")),
        version=int(source.get("version", 0) or 0),
        reason=str(source.get("reason", "")),
    )


def decisionFromLegacyResponse(response: dict[str, Any], usedContext: dict[str, ContextItemRef]) -> Decision:
    action_payload = response.get("action")
    action = actionFromLegacyDict(action_payload) if isinstance(action_payload, dict) else None
    debug_payload = response.get("debug", {})
    safe_debug = debug_payload if isinstance(debug_payload, dict) else {}
    return Decision(
        sequence=int(response.get("sequence", 0) or 0),
        status=str(response.get("status", STATUS_OK)),
        episodeVersion=int(response.get("episodeVersion", 0) or 0),
        configVersion=int(response.get("configVersion", 0) or 0),
        contextVersion=int(response.get("contextVersion", 0) or 0),
        gridVersion=int(response.get("gridVersion", 0) or 0),
        action=action,
        usedContext=usedContext,
        debug=DecisionDebug(
            policyName=str(safe_debug.get("policyName", "")),
            reason=str(safe_debug.get("reason", "")),
            selectedPolicyId=safe_debug.get("selectedPolicyId"),
            selectedPolicyPriority=safe_debug.get("selectedPolicyPriority"),
            candidateCount=safe_debug.get("candidateCount"),
            usedInputs=usedContext,
            values=dict(safe_debug),
        ),
        error=None if response.get("status", STATUS_OK) == STATUS_OK else PolicyError("DECISION_FAILED", "legacy response error"),
    )


def actionFromLegacyDict(action: dict[str, Any]) -> Action:
    return Action(
        steering=clampFloat(action.get("steering", 0.0), -1.0, 1.0),
        throttle=clampFloat(action.get("throttle", 0.0), 0.0, 1.0),
        brake=clampFloat(action.get("brake", 0.0), 0.0, 1.0),
        targetSpeedKmh=max(float(action.get("targetSpeedKmh", 0.0) or 0.0), 0.0),
        direction=str(action.get("direction", "Forward"))
        if str(action.get("direction", "Forward")) in {"Forward", "Reverse"}
        else "Forward",
    )


def clampFloat(value: Any, minimum: float, maximum: float) -> float:
    try:
        numeric_value = float(value)
    except (TypeError, ValueError):
        numeric_value = minimum

    return max(minimum, min(maximum, numeric_value))


def configFromLegacyPayload(payload: dict[str, Any]) -> Config:
    policy_payload = payload.get("policySpec", payload.get("policy", None))
    parameters = payload.get("parameters", {}) if isinstance(payload.get("parameters", {}), dict) else {}
    if isinstance(policy_payload, dict):
        parameters = {**parameters, "_legacyPolicySpec": policy_payload}

    return Config(
        schemaVersion=int(payload.get("schemaVersion", 1) or 1),
        configId=payload.get("configId"),
        drive=driveConfigFromDict(payload.get("driveSpec", payload.get("drive", {}))),
        lidar=lidarConfigFromDict(payload.get("lidarSpec", payload.get("lidar", {}))),
        motionControl=motionControlConfigFromDict(
            payload.get("motionControlSpec", payload.get("motionControl", {}))
        ),
        policy=policySpecFromDict(policy_payload) if isinstance(policy_payload, dict) else None,
        parameters=parameters,
    )


def driveConfigFromDict(source: Any) -> DriveConfig | None:
    if not isinstance(source, dict) or not source:
        return None
    return DriveConfig(**filterDataclassFields(DriveConfig, source))


def lidarConfigFromDict(source: Any) -> LidarConfig | None:
    if not isinstance(source, dict) or not source:
        return None
    return LidarConfig(**filterDataclassFields(LidarConfig, source))


def motionControlConfigFromDict(source: Any) -> MotionControlConfig | None:
    if not isinstance(source, dict) or not source:
        return None
    return MotionControlConfig(**filterDataclassFields(MotionControlConfig, source))


def policySpecFromDict(source: dict[str, Any]) -> PolicySpec:
    from deliverybot_policy.bot_policy_contract import EnabledPolicy

    enabled_policies = []
    for policy in source.get("enabledPolicies", []):
        if not isinstance(policy, dict):
            continue
        enabled_policies.append(EnabledPolicy(**filterDataclassFields(EnabledPolicy, policy)))

    return PolicySpec(
        catalogId=str(source.get("catalogId", "")),
        catalogVersion=int(source.get("catalogVersion", 0) or 0),
        enabledPolicies=enabled_policies,
    )


def filterDataclassFields(cls: type[Any], source: dict[str, Any]) -> dict[str, Any]:
    allowed_fields = getattr(cls, "__dataclass_fields__", {})
    return {field_name: source[field_name] for field_name in allowed_fields if field_name in source}


def episodeSetupFromLegacyPayload(payload: dict[str, Any]) -> EpisodeSetup:
    location_spec = payload.get("locationSpec", {}) if isinstance(payload.get("locationSpec", {}), dict) else {}
    start = payload.get("start", location_spec.get("startLocationCm", {}))
    goal = payload.get("goal", location_spec.get("goalLocationCm", {}))
    safe_start = start if isinstance(start, dict) else {}
    safe_goal = goal if isinstance(goal, dict) else {}
    has_goal = bool(safe_goal.get("hasGoal", bool(safe_goal)))
    if location_spec:
        has_goal = bool(location_spec.get("autoStartRoute", has_goal))

    return EpisodeSetup(
        schemaVersion=int(payload.get("schemaVersion", 1) or 1),
        episodeId=str(payload.get("episodeId", "")),
        robotId=str(payload.get("robotInstanceId", payload.get("robotId", ""))),
        startPose=poseFromDict(safe_start),
        goal=Goal(
            hasGoal=has_goal,
            x=float(safe_goal.get("x", 0.0) or 0.0),
            y=float(safe_goal.get("y", 0.0) or 0.0),
            z=float(safe_goal.get("z", 0.0) or 0.0),
            acceptanceRadiusCm=safe_goal.get("acceptanceRadiusCm"),
        ),
        resetPolicyState=bool(payload.get("resetPolicyState", True)),
    )


def poseFromDict(source: dict[str, Any]) -> Pose:
    return Pose(
        x=float(source.get("x", 0.0) or 0.0),
        y=float(source.get("y", 0.0) or 0.0),
        z=float(source.get("z", 0.0) or 0.0),
        yawDegree=float(source.get("yawDegree", 0.0) or 0.0),
    )


def contextFromLegacyGrid(gridInfo: dict[str, Any], updateId: str = "grid") -> Context:
    return Context(
        updateId=updateId,
        worldTimeSeconds=0.0,
        items=[
            ContextItem(
                kind=CONTEXT_GRID_MAP,
                name="grid",
                version=int(gridInfo.get("gridVersion", 1) or 1),
                payload=gridInfo,
            )
        ],
    )


def contextFromLegacyObservation(observation: dict[str, Any]) -> Context:
    world_time_seconds = float(observation.get("worldTimeSeconds", 0.0) or 0.0)
    sequence = int(observation.get("sequence", 0) or 0)
    sensor_sequence = int(observation.get("sensorSequence", sequence) or sequence)
    items: list[ContextItem] = []

    if isinstance(observation.get("robotState"), dict):
        items.append(
            ContextItem(
                kind=CONTEXT_ROBOT_STATE,
                name="robotState",
                version=sequence,
                capturedWorldTimeSeconds=world_time_seconds,
                payload=observation["robotState"],
            )
        )
    if isinstance(observation.get("lidarRays"), list):
        items.append(
            ContextItem(
                kind=CONTEXT_LIDAR_SCAN,
                name="lidarScan",
                version=sensor_sequence,
                capturedWorldTimeSeconds=world_time_seconds,
                payload={
                    "mode": observation.get("lidarModeType", "Unknown"),
                    "sensorSequence": sensor_sequence,
                    "rays": observation["lidarRays"],
                },
            )
        )
    if isinstance(observation.get("observedObjects"), list):
        items.append(
            ContextItem(
                kind=CONTEXT_OBSERVED_OBJECTS,
                name="observedObjects",
                version=sensor_sequence,
                capturedWorldTimeSeconds=world_time_seconds,
                payload={
                    "sensorSequence": sensor_sequence,
                    "objects": observation["observedObjects"],
                },
            )
        )

    return Context(updateId=f"observation-{sequence}", worldTimeSeconds=world_time_seconds, items=items)


def normalizePlannerMode(value: str) -> str:
    mode = str(value or "auto").strip().lower().replace("-", "_")
    return mode if mode in {"auto", "astar", "hybrid_astar"} else "auto"


def normalizeDwaMode(value: str) -> str:
    mode = str(value or "policy").strip().lower()
    return mode if mode in {"policy", "on", "off"} else "policy"
