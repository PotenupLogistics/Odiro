from __future__ import annotations

import argparse
import importlib
import importlib.util
import json
import os
import sys
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


# 실행 중인 policy package와 episode 상태
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


def construct_contract(type_name: str, data: dict):
    """Construct a policy contract object, or keep raw dict input for skeleton policies."""
    value_type = contract_type(type_name)
    return value_type(**data) if value_type else data


# dict 데이터를 PolicyError 객체로 변환
def parse_policy_error(data: dict | None) -> PolicyError | None:
    if not data:
        return None

    policy_error_type = contract_type("PolicyError")
    if policy_error_type is None:
        return data

    return policy_error_type(
        code=data.get("code", "UNKNOWN_ERROR"),
        message=data.get("message", ""),
        retryable=data.get("retryable", False),
        details=data.get("details", {}),
    )


# /scenario/start 요청 JSON을 ScenarioStartRequest로 변환
def parse_start(data: dict) -> ScenarioStartRequest:
    if contract_type("ScenarioStartRequest") is None:
        return data

    grid_data = data["grid"]
    goal_data = data["goal"]
    robot_spec = data.get("robotSpec") or data.get("vehicleSpec", {})

    return contract_type("ScenarioStartRequest")(
        robotInstanceId=data["robotInstanceId"],
        start=construct_contract("StartLocation", data["start"]),
        goal=contract_type("GoalLocation")(
            hasGoal=goal_data["hasGoal"],
            x=goal_data["x"],
            y=goal_data["y"],
            z=goal_data.get("z", 0.0),
        ),
        grid=contract_type("GridMap")(
            gridSizeX=grid_data["gridSizeX"],
            gridSizeY=grid_data["gridSizeY"],
            cellSizeCm=grid_data["cellSizeCm"],
            cellCount=grid_data["cellCount"],
            originCm=construct_contract("StartLocation", grid_data["originCm"]),
            cells=[
                construct_contract("GridCell", cell_data)
                for cell_data in grid_data.get("cells", [])
            ],
        ),
        robotSpec=robot_spec,
        driveSpec=data.get("driveSpec", {}),
        lidarSpec=data.get("lidarSpec", {}),
        vehicleSpec=data.get("vehicleSpec", robot_spec),
        controlSpec=data.get("controlSpec", {}),
    )


# /scenario/decide 요청 JSON을 ScenarioDecideRequest로 변환
def parse_decide(data: dict) -> ScenarioDecideRequest:
    if contract_type("ScenarioDecideRequest") is None:
        return data

    return contract_type("ScenarioDecideRequest")(
        sequence=data["sequence"],
        runTimeSeconds=data["runTimeSeconds"],
        robotState=construct_contract("RobotState", data["robotState"]),
        lidarRays=[
            construct_contract("LidarRay", ray_data)
            for ray_data in data.get("lidarRays", [])
        ],
        observedObjects=data.get("observedObjects", []),
    )


# /scenario/end 요청 JSON을 ScenarioEndRequest로 변환
def parse_end(data: dict) -> ScenarioEndRequest:
    if contract_type("ScenarioEndRequest") is None:
        return data

    return contract_type("ScenarioEndRequest")(
        robotInstanceId=data["robotInstanceId"],
        sequence=data["sequence"],
        status=data["status"],
        error=parse_policy_error(data.get("error")),
        metrics=data.get("metrics", {}),
        debug=data.get("debug", {}),
    )


# dict를 JSON bytes로 변환
def to_json_bytes(data: dict) -> bytes:
    return json.dumps(data, ensure_ascii=False).encode("utf-8")


# Unreal이 PythonAgent 실행/재사용 여부를 확인할 때 쓰는 가장 가벼운 응답
def build_health_response() -> dict:
    response = {
        "status": "ok",
        "server": SERVER_NAME,
        "version": SERVER_VERSION,
    }
    if runtime is not None:
        response["policyPath"] = runtime.policy_path.as_posix()
    return response


# Unreal이 보낸 envelope에서 request 영역만 꺼낸다.
def get_request_data(message: dict) -> dict:
    request_data = message.get("request")
    if isinstance(request_data, dict):
        return request_data

    return message


# Python 처리 결과를 envelope의 response 영역에 채워 반환한다.
def build_response_message(message: dict, response: dict) -> dict:
    result = dict(message)
    result["response"] = response
    return result


# 에러 응답을 envelope 형식으로 만든다.
def build_error_message(message: dict, code: str, error_message: str, reason: str) -> dict:
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


# HTTP 요청을 처리하는 handler
class PythonAgentHandler(BaseHTTPRequestHandler):

    def log_message(self, format: str, *args) -> None:
        return

    # GET 요청 처리
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


    # POST 요청 처리
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


    # query string을 제외하고 endpoint path만 가져온다.
    def get_request_path(self) -> str:
        return urlparse(self.path).path


    # HTTP body에서 JSON 읽기
    def read_json_body(self) -> dict:
        content_length = int(self.headers.get("Content-Length", "0"))
        raw_body = self.rfile.read(content_length)

        if not raw_body:
            return {}

        return json.loads(raw_body.decode("utf-8"))


    # 요청 path에 따라 request를 처리하고 response를 envelope에 채운다.
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


    # JSON 응답 쓰기
    def write_json_response(self, status_code: int, data: dict) -> None:
        body = to_json_bytes(data)

        self.send_response(status_code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


class PythonAgentServer(HTTPServer):
    allow_reuse_address = True


# 명령줄 인자를 읽어 서버 실행 옵션으로 변환
def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run the DeliveryBot Python Agent HTTP server.")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8000)
    parser.add_argument("--policy-path", default=None)
    parser.add_argument("--policy-mode", default="runtime")
    parser.add_argument("--verbose-runtime-log", action="store_true")
    return parser.parse_args()


# HTTP 서버 실행
def run_server(
    host: str = "127.0.0.1",
    port: int = 8000,
    policy_path: str | None = None,
    policy_mode: str = "runtime",
    verbose_runtime_log: bool = False,
) -> None:
    global runtime
    runtime = create_runtime_context(policy_path)
    server = PythonAgentServer((host, port), PythonAgentHandler)
    print(f"PythonAgent server listening on http://{host}:{port}")
    print(f"PythonAgent policy mode: {policy_mode}")
    print(f"PythonAgent policy path: {runtime.policy_path}")
    if verbose_runtime_log:
        print("PythonAgent verbose runtime log enabled")

    server.serve_forever()


# python server.py로 실행했을 때 서버 시작
if __name__ == "__main__":
    arguments = parse_arguments()
    run_server(
        host=arguments.host,
        port=arguments.port,
        policy_path=arguments.policy_path,
        policy_mode=arguments.policy_mode,
        verbose_runtime_log=arguments.verbose_runtime_log,
    )
