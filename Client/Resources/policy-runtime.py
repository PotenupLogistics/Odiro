from __future__ import annotations

import argparse
import importlib
import importlib.util
import inspect
import json
import os
import sys
import tempfile
from dataclasses import dataclass
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
from types import ModuleType
from urllib.parse import urlparse


SERVER_NAME = "PythonAgent"
SERVER_VERSION = "0.1"
POLICY_PACKAGE_NAME = "odiro_project_policy"


@dataclass
class PolicyRuntimeContext:
    """Loaded policy package, policy object, and per-episode state."""

    policy_path: Path
    policy_module: ModuleType
    contract_module: ModuleType | None
    state: object
    policy: object


class AttributeDict(dict):
    """Dict fallback that also supports the attribute access used by policy code."""

    def __getattr__(self, name: str):
        try:
            return self[name]
        except KeyError as error:
            raise AttributeError(name) from error


runtime: PolicyRuntimeContext | None = None


def default_policy_path() -> Path:
    """Return the legacy development policy path when no project policy path is supplied."""
    env_path = os.environ.get("ODIRO_POLICY_PATH")
    if env_path:
        return Path(env_path)

    client_root = Path(__file__).resolve().parents[1]
    return client_root / "Tools" / "PythonAgent" / "agent"


def resolve_policy_path(raw_policy_path: str | None) -> Path:
    """Resolve an explicit or default policy package directory."""
    policy_path = Path(raw_policy_path).expanduser() if raw_policy_path else default_policy_path()
    return policy_path.resolve()


def import_project_policy(policy_path: Path) -> ModuleType:
    """Load policy/__init__.py as an isolated package so relative imports work."""
    entrypoint = policy_path / "__init__.py"
    if not entrypoint.is_file():
        raise FileNotFoundError(f"policy entrypoint not found: {entrypoint}")

    for module_name in list(sys.modules):
        if module_name == POLICY_PACKAGE_NAME or module_name.startswith(POLICY_PACKAGE_NAME + "."):
            del sys.modules[module_name]

    spec = importlib.util.spec_from_file_location(
        POLICY_PACKAGE_NAME,
        entrypoint,
        submodule_search_locations=[str(policy_path)],
    )
    if spec is None or spec.loader is None:
        raise RuntimeError(f"policy package cannot be loaded: {policy_path}")

    module = importlib.util.module_from_spec(spec)
    sys.modules[POLICY_PACKAGE_NAME] = module
    spec.loader.exec_module(module)
    return module


def import_optional_policy_module(name: str) -> ModuleType | None:
    """Import an optional module inside the loaded policy package."""
    try:
        return importlib.import_module(f"{POLICY_PACKAGE_NAME}.{name}")
    except ModuleNotFoundError:
        return None


def create_runtime_context(raw_policy_path: str | None) -> PolicyRuntimeContext:
    """Create the policy object and state from a project policy package."""
    policy_path = resolve_policy_path(raw_policy_path)
    policy_module = import_project_policy(policy_path)
    create_policy = getattr(policy_module, "create_policy", None)
    if not callable(create_policy):
        raise RuntimeError("policy/__init__.py must expose callable create_policy")

    contract_module = import_optional_policy_module("contract")
    state_module = import_optional_policy_module("state")
    state_type = getattr(state_module, "AgentState", None) if state_module else None
    state = state_type() if callable(state_type) else {}

    return PolicyRuntimeContext(
        policy_path=policy_path,
        policy_module=policy_module,
        contract_module=contract_module,
        state=state,
        policy=create_policy(),
    )


def contract_type(type_name: str):
    """Return a callable contract type from the policy package when available."""
    if runtime is None or runtime.contract_module is None:
        return None

    value = getattr(runtime.contract_module, type_name, None)
    return value if callable(value) else None


def filter_contract_payload(value_type, data: dict) -> dict:
    """Drop optional fields that older project contracts do not accept."""
    try:
        signature = inspect.signature(value_type)
    except (TypeError, ValueError):
        return data

    parameters = signature.parameters
    if any(parameter.kind == inspect.Parameter.VAR_KEYWORD for parameter in parameters.values()):
        return data

    allowed_names = {
        name
        for name, parameter in parameters.items()
        if parameter.kind in (inspect.Parameter.POSITIONAL_OR_KEYWORD, inspect.Parameter.KEYWORD_ONLY)
    }
    return {name: value for name, value in data.items() if name in allowed_names}


def construct_contract(type_name: str, data: dict):
    """Construct a policy contract object, or keep a dict-like fallback."""
    value_type = contract_type(type_name)
    if value_type is None:
        return AttributeDict(data)

    try:
        return value_type(**data)
    except TypeError:
        filtered_data = filter_contract_payload(value_type, data)
        if filtered_data != data:
            return value_type(**filtered_data)
        raise


def read_object_value(value: object, name: str, default=None):
    """Read a field from either a contract object or dict fallback."""
    if isinstance(value, dict):
        return value.get(name, default)

    return getattr(value, name, default)


def safe_float(value: object, default: float = 0.0) -> float:
    """Convert JSON scalar values to float while keeping parser errors local."""
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


def safe_int(value: object, default: int = 0) -> int:
    """Convert JSON scalar values to int while keeping parser errors local."""
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


def parse_actor_tags(data: dict) -> list[str]:
    """Return actorTags as a stable string list."""
    actor_tags = data.get("actorTags", [])
    if not isinstance(actor_tags, list):
        return []

    return [str(actor_tag) for actor_tag in actor_tags]


def parse_vector_cm(data: object) -> dict[str, float] | None:
    """Parse an Unreal vector object into a cm-unit dict."""
    if not isinstance(data, dict):
        return None

    return {
        "x": safe_float(data.get("x", 0.0)),
        "y": safe_float(data.get("y", 0.0)),
        "z": safe_float(data.get("z", 0.0)),
    }


def parse_policy_error(data: dict | None):
    """Parse an optional policy error payload."""
    if not data:
        return None

    return construct_contract(
        "PolicyError",
        {
            "code": str(data.get("code", "UNKNOWN_ERROR")),
            "message": str(data.get("message", "")),
            "retryable": bool(data.get("retryable", False)),
            "details": data.get("details", {}),
        },
    )


def parse_start_location(data: dict):
    """Parse an Unreal world location and yaw."""
    return construct_contract(
        "StartLocation",
        {
            "x": safe_float(data.get("x", 0.0)),
            "y": safe_float(data.get("y", 0.0)),
            "z": safe_float(data.get("z", 0.0)),
            "yawDegree": safe_float(data.get("yawDegree", 0.0)),
        },
    )


def parse_goal_location(data: dict):
    """Parse a goal location."""
    return construct_contract(
        "GoalLocation",
        {
            "hasGoal": bool(data.get("hasGoal", False)),
            "x": safe_float(data.get("x", 0.0)),
            "y": safe_float(data.get("y", 0.0)),
            "z": safe_float(data.get("z", 0.0)),
        },
    )


def parse_grid_cell(data: dict):
    """Parse one navigation grid cell."""
    return construct_contract(
        "GridCell",
        {
            "x": safe_int(data.get("x", 0)),
            "y": safe_int(data.get("y", 0)),
            "areaType": str(data.get("areaType", "Walkable")),
            "cost": safe_float(data.get("cost", 1.0), 1.0),
            "blocked": bool(data.get("blocked", False)),
            "sourceCollisionProfile": str(data.get("sourceCollisionProfile", "")),
        },
    )


def parse_start(data: dict):
    """Parse /scenario/start JSON into the loaded policy contract."""
    if contract_type("ScenarioStartRequest") is None:
        return data

    grid_data = data["grid"]
    goal_data = data["goal"]
    robot_spec = data.get("robotSpec") or data.get("vehicleSpec", {})

    grid = construct_contract(
        "GridMap",
        {
            "gridSizeX": safe_int(grid_data["gridSizeX"]),
            "gridSizeY": safe_int(grid_data["gridSizeY"]),
            "cellSizeCm": safe_float(grid_data["cellSizeCm"]),
            "cellCount": safe_int(grid_data["cellCount"]),
            "originCm": parse_start_location(grid_data["originCm"]),
            "cells": [
                parse_grid_cell(cell_data)
                for cell_data in grid_data.get("cells", [])
                if isinstance(cell_data, dict)
            ],
        },
    )

    return construct_contract(
        "ScenarioStartRequest",
        {
            "robotInstanceId": str(data["robotInstanceId"]),
            "start": parse_start_location(data["start"]),
            "goal": parse_goal_location(goal_data),
            "grid": grid,
            "robotSpec": robot_spec,
            "driveSpec": data.get("driveSpec", {}),
            "lidarSpec": data.get("lidarSpec", {}),
            "artifactSpec": data.get("artifactSpec", {}),
            "vehicleSpec": data.get("vehicleSpec", robot_spec),
            "controlSpec": data.get("controlSpec", {}),
        },
    )


def parse_legacy_lidar_ray(data: dict):
    """Parse a legacy 2D LiDAR ray."""
    return construct_contract(
        "LidarRay",
        {
            "hit": bool(data.get("hit", False)),
            "distanceM": safe_float(data.get("distanceM", 0.0)),
            "rayIndex": data.get("rayIndex"),
            "rayYawDegree": safe_float(data.get("rayYawDegree", data.get("yawDegree", 0.0))),
            "actorName": data.get("actorName"),
            "actorTags": parse_actor_tags(data),
        },
    )


def parse_lidar_ray_1d(data: dict):
    """Parse a typed 1D LiDAR ray."""
    return construct_contract(
        "LidarRay1D",
        {
            "hit": bool(data.get("hit", False)),
            "distanceM": safe_float(data.get("distanceM", 0.0)),
            "rayIndex": data.get("rayIndex"),
            "actorName": data.get("actorName"),
            "actorTags": parse_actor_tags(data),
        },
    )


def parse_lidar_ray_2d(data: dict):
    """Parse a typed 2D LiDAR ray."""
    return construct_contract(
        "LidarRay2D",
        {
            "hit": bool(data.get("hit", False)),
            "distanceM": safe_float(data.get("distanceM", 0.0)),
            "yawDegree": safe_float(data.get("yawDegree", data.get("rayYawDegree", 0.0))),
            "rayIndex": data.get("rayIndex"),
            "actorName": data.get("actorName"),
            "actorTags": parse_actor_tags(data),
        },
    )


def parse_lidar_ray_3d(data: dict):
    """Parse a typed 3D LiDAR ray."""
    return construct_contract(
        "LidarRay3D",
        {
            "hit": bool(data.get("hit", False)),
            "distanceM": safe_float(data.get("distanceM", 0.0)),
            "yawDegree": safe_float(data.get("yawDegree", data.get("rayYawDegree", 0.0))),
            "pitchDegree": safe_float(data.get("pitchDegree", data.get("rayPitchDegree", 0.0))),
            "rayIndex": data.get("rayIndex"),
            "actorName": data.get("actorName"),
            "actorTags": parse_actor_tags(data),
            "hitLocationCm": parse_vector_cm(data.get("hitLocationCm")),
        },
    )


def parse_lidar_ray_array(values: object, parse_ray) -> list:
    """Parse a JSON LiDAR ray array while ignoring malformed entries."""
    if not isinstance(values, list):
        return []

    return [
        parse_ray(ray_data)
        for ray_data in values
        if isinstance(ray_data, dict)
    ]


def parse_lidar_observation(data: dict):
    """Parse typed LiDAR observations grouped by scan dimension."""
    lidar_data = data.get("lidar", {})
    if not isinstance(lidar_data, dict):
        lidar_data = {}

    return construct_contract(
        "LidarObservation",
        {
            "mode": str(lidar_data.get("mode", "")),
            "sensorSequence": safe_int(lidar_data.get("sensorSequence", data.get("sensorSequence", 0))),
            "sensorTimeSeconds": safe_float(
                lidar_data.get(
                    "sensorTimeSeconds",
                    data.get("sensorTimeSeconds", data.get("runTimeSeconds", 0.0)),
                )
            ),
            "rays1d": parse_lidar_ray_array(lidar_data.get("rays1d", []), parse_lidar_ray_1d),
            "rays2d": parse_lidar_ray_array(lidar_data.get("rays2d", []), parse_lidar_ray_2d),
            "rays3d": parse_lidar_ray_array(lidar_data.get("rays3d", []), parse_lidar_ray_3d),
        },
    )


def parse_robot_state(data: dict):
    """Parse robot state from a decide request."""
    return construct_contract(
        "RobotState",
        {
            "x": safe_float(data.get("x", 0.0)),
            "y": safe_float(data.get("y", 0.0)),
            "z": safe_float(data.get("z", 0.0)),
            "yawDegree": safe_float(data.get("yawDegree", 0.0)),
            "speedKmh": safe_float(data.get("speedKmh", 0.0)),
            "bColliding": bool(data.get("bColliding", False)),
            "collisionActorName": str(data.get("collisionActorName", "")),
            "collisionActorTags": parse_actor_tags({"actorTags": data.get("collisionActorTags", [])}),
        },
    )


def parse_decide(data: dict):
    """Parse /scenario/decide JSON into the loaded policy contract."""
    if contract_type("ScenarioDecideRequest") is None:
        return data

    lidar = parse_lidar_observation(data)

    return construct_contract(
        "ScenarioDecideRequest",
        {
            "sequence": safe_int(data["sequence"]),
            "runTimeSeconds": safe_float(data["runTimeSeconds"]),
            "sensorSequence": safe_int(data.get("sensorSequence", read_object_value(lidar, "sensorSequence", 0))),
            "sensorTimeSeconds": safe_float(data.get("sensorTimeSeconds", read_object_value(lidar, "sensorTimeSeconds", 0.0))),
            "robotState": parse_robot_state(data["robotState"]),
            "lidar": lidar,
            "lidarRays": parse_lidar_ray_array(data.get("lidarRays", []), parse_legacy_lidar_ray),
            "observedObjects": data.get("observedObjects", []),
        },
    )


def parse_end(data: dict):
    """Parse /scenario/end JSON into the loaded policy contract."""
    if contract_type("ScenarioEndRequest") is None:
        return data

    return construct_contract(
        "ScenarioEndRequest",
        {
            "robotInstanceId": str(data["robotInstanceId"]),
            "sequence": safe_int(data["sequence"]),
            "status": str(data["status"]),
            "error": parse_policy_error(data.get("error")),
            "metrics": data.get("metrics", {}),
            "debug": data.get("debug", {}),
        },
    )


def to_json_bytes(data: dict) -> bytes:
    """Serialize a response dict to UTF-8 JSON bytes."""
    return json.dumps(data, ensure_ascii=False).encode("utf-8")


def build_health_response() -> dict:
    """Return a lightweight readiness response for Unreal health checks."""
    response = {
        "status": "ok",
        "server": SERVER_NAME,
        "version": SERVER_VERSION,
    }
    if runtime is not None:
        response["policyPath"] = runtime.policy_path.as_posix()
    return response


def get_request_data(message: dict) -> dict:
    """Extract the request payload from the Unreal envelope."""
    request_data = message.get("request")
    if isinstance(request_data, dict):
        return request_data

    return message


def build_response_message(message: dict, response: dict) -> dict:
    """Fill the response section while preserving the Unreal request envelope."""
    result = dict(message)
    result["response"] = response
    return result


def build_error_message(message: dict, code: str, error_message: str, reason: str) -> dict:
    """Build a stable error response envelope."""
    return build_response_message(
        message,
        {
            "status": "error",
            "error": {
                "code": code,
                "message": error_message,
            },
            "debug": {
                "reason": reason,
            },
        },
    )


def build_start_artifact_error(code: str, message: str) -> dict:
    """Build a non-retryable /scenario/start artifact initialization error."""
    return {
        "status": "error",
        "accepted": False,
        "pathStatus": "empty",
        "error": {
            "code": code,
            "message": message,
            "retryable": False,
        },
        "decision": {
            "selectedPolicy": "PolicyRuntime",
            "reason": "artifact_output_initialization_failed",
        },
        "path": {
            "pathStatus": "empty",
            "pathIndex": 0,
            "pathLength": 0,
            "targetPathIndex": 0,
            "targetWorldPoint": None,
            "pathWorldPoints": [],
        },
        "events": [],
        "captures": [],
    }


def prepare_required_point_cloud_output(request_data: dict) -> dict | None:
    """Validate and probe the required project-run Point Cloud output directory."""
    if runtime is None:
        return build_start_artifact_error(
            "POLICY_RUNTIME_NOT_READY",
            "Policy runtime is not initialized.",
        )

    lidar_spec = request_data.get("lidarSpec", {})
    if not isinstance(lidar_spec, dict):
        lidar_spec = {}
    point_cloud_options = lidar_spec.get("pointCloudOptions", {})
    if not isinstance(point_cloud_options, dict) or not bool(point_cloud_options.get("captureEnabled", False)):
        return None

    artifact_spec = request_data.get("artifactSpec", {})
    if not isinstance(artifact_spec, dict) or not bool(artifact_spec.get("required", False)):
        return None

    configuration_error_code = str(artifact_spec.get("configurationErrorCode") or "")
    if configuration_error_code:
        return build_start_artifact_error(
            configuration_error_code,
            str(
                artifact_spec.get("configurationErrorMessage")
                or "Project episode output configuration is invalid."
            ),
        )

    captures_root_text = str(artifact_spec.get("capturesRoot") or "")
    if not captures_root_text:
        return build_start_artifact_error(
            "POINT_CLOUD_OUTPUT_PATH_MISSING",
            "Required project episode output path is missing.",
        )

    captures_root = Path(captures_root_text).expanduser()
    if not captures_root.is_absolute():
        return build_start_artifact_error(
            "INVALID_EPISODE_OUTPUT_PATH",
            f"Required project episode output path must be absolute: {captures_root_text}",
        )

    resolved_captures_root = captures_root.resolve()
    expected_episodes_root = (runtime.policy_path.parent.parent / "episodes").resolve()
    try:
        episode_relative_path = resolved_captures_root.relative_to(expected_episodes_root)
    except ValueError:
        return build_start_artifact_error(
            "EPISODE_OUTPUT_OUTSIDE_RUN",
            f"Project episode output must be under {expected_episodes_root}: {resolved_captures_root}",
        )

    if len(episode_relative_path.parts) != 1:
        return build_start_artifact_error(
            "INVALID_EPISODE_OUTPUT_PATH",
            f"Project episode output must identify one episode directory: {resolved_captures_root}",
        )

    point_cloud_directory = resolved_captures_root / "lidar_point_cloud"
    probe_path: Path | None = None
    try:
        point_cloud_directory.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile(
            mode="w",
            encoding="utf-8",
            prefix=".odiro-write-probe-",
            dir=point_cloud_directory,
            delete=False,
        ) as probe_file:
            probe_file.write("ok\n")
            probe_path = Path(probe_file.name)
        probe_path.unlink()
    except OSError as error:
        try:
            if probe_path is not None:
                probe_path.unlink(missing_ok=True)
        except OSError:
            pass
        return build_start_artifact_error(
            "POINT_CLOUD_OUTPUT_UNAVAILABLE",
            f"Cannot initialize point cloud output at {resolved_captures_root}: {error}",
        )

    return None


class PythonAgentHandler(BaseHTTPRequestHandler):
    """HTTP adapter for the loaded project policy."""

    def log_message(self, format: str, *args) -> None:
        return

    def do_GET(self) -> None:
        request_path = self.get_request_path()

        if request_path == "/health":
            self.write_json_response(200, build_health_response())
            return

        self.write_json_response(
            404,
            {
                "status": "error",
                "error": {
                    "code": "NOT_FOUND",
                    "message": request_path,
                },
                "debug": {
                    "reason": "unknown_endpoint",
                },
            },
        )

    def do_POST(self) -> None:
        request_json = {}

        try:
            request_json = self.read_json_body()
            response = self.route_request(request_json)
            self.write_json_response(200, response)

        except Exception as error:
            self.write_json_response(
                500,
                build_error_message(
                    request_json,
                    "SERVER_ERROR",
                    str(error),
                    "server_exception",
                ),
            )

    def get_request_path(self) -> str:
        return urlparse(self.path).path

    def read_json_body(self) -> dict:
        content_length = int(self.headers.get("Content-Length", "0"))
        raw_body = self.rfile.read(content_length)

        if not raw_body:
            return {}

        return json.loads(raw_body.decode("utf-8"))

    def route_request(self, request_json: dict) -> dict:
        if runtime is None:
            return build_error_message(
                request_json,
                "POLICY_RUNTIME_NOT_READY",
                "Policy runtime is not initialized.",
                "runtime_not_ready",
            )

        request_path = self.get_request_path()
        request_data = get_request_data(request_json)

        if request_path == "/scenario/start":
            artifact_error = prepare_required_point_cloud_output(request_data)
            if artifact_error is not None:
                return build_response_message(request_json, artifact_error)
            response = runtime.policy.start(parse_start(request_data), runtime.state)
            return build_response_message(request_json, response)

        if request_path == "/scenario/decide":
            response = runtime.policy.decide(parse_decide(request_data), runtime.state)
            return build_response_message(request_json, response)

        if request_path == "/scenario/end":
            response = runtime.policy.end(parse_end(request_data), runtime.state)
            return build_response_message(request_json, response)

        return build_error_message(
            request_json,
            "NOT_FOUND",
            request_path,
            "unknown_endpoint",
        )

    def write_json_response(self, status_code: int, data: dict) -> None:
        body = to_json_bytes(data)

        self.send_response(status_code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


class PythonAgentServer(HTTPServer):
    allow_reuse_address = True


def parse_arguments() -> argparse.Namespace:
    """Parse command line options for the policy runtime server."""
    parser = argparse.ArgumentParser(description="Run the DeliveryBot Python Agent HTTP server.")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8000)
    parser.add_argument("--policy-path", default=None)
    parser.add_argument("--policy-mode", default="runtime")
    parser.add_argument("--verbose-runtime-log", action="store_true")
    return parser.parse_args()


def run_server(
    host: str = "127.0.0.1",
    port: int = 8000,
    policy_path: str | None = None,
    policy_mode: str = "runtime",
    verbose_runtime_log: bool = False,
) -> None:
    """Load the project policy package and serve Unreal policy requests."""
    global runtime
    runtime = create_runtime_context(policy_path)
    server = PythonAgentServer((host, port), PythonAgentHandler)
    print(f"PythonAgent server listening on http://{host}:{port}")
    print(f"PythonAgent policy mode: {policy_mode}")
    print(f"PythonAgent policy path: {runtime.policy_path}")
    if verbose_runtime_log:
        print("PythonAgent verbose runtime log enabled")

    server.serve_forever()


if __name__ == "__main__":
    arguments = parse_arguments()
    run_server(
        host=arguments.host,
        port=arguments.port,
        policy_path=arguments.policy_path,
        policy_mode=arguments.policy_mode,
        verbose_runtime_log=arguments.verbose_runtime_log,
    )
