# DeliveryBot 정책 결정을 위한 Client-local Python Agent HTTP 서버를 실행한다.
import argparse
import json
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import urlparse

from agent import create_policy
from agent.contract import (
    GoalLocation,
    GridCell,
    GridMap,
    LidarObservation,
    LidarRay,
    LidarRay1D,
    LidarRay2D,
    LidarRay3D,
    PolicyError,
    RobotState,
    ScenarioDecideRequest,
    ScenarioEndRequest,
    ScenarioStartRequest,
    StartLocation,
)
from agent.state import AgentState


SERVER_NAME = "PythonAgent"
SERVER_VERSION = "0.1"


# episode 동안 유지되는 PythonAgent 상태
state = AgentState()


# user_agent.py의 create_policy()를 통해 BotPolicy 생성
policy = create_policy()


# dict 데이터를 PolicyError 객체로 변환한다.
def parse_policy_error(data: dict | None) -> PolicyError | None:
    if not data:
        return None

    return PolicyError(
        code=str(data.get("code", "UNKNOWN_ERROR")),
        message=str(data.get("message", "")),
        retryable=bool(data.get("retryable", False)),
        details=data.get("details", {}),
    )


# dict 데이터를 StartLocation 객체로 변환한다.
def parse_start_location(data: dict) -> StartLocation:
    return StartLocation(
        x=float(data.get("x", 0.0)),
        y=float(data.get("y", 0.0)),
        z=float(data.get("z", 0.0)),
        yawDegree=float(data.get("yawDegree", 0.0)),
    )


# dict 데이터를 GoalLocation 객체로 변환한다.
def parse_goal_location(data: dict) -> GoalLocation:
    return GoalLocation(
        hasGoal=bool(data.get("hasGoal", False)),
        x=float(data.get("x", 0.0)),
        y=float(data.get("y", 0.0)),
        z=float(data.get("z", 0.0)),
    )


# dict 데이터를 GridCell 객체로 변환한다.
def parse_grid_cell(data: dict) -> GridCell:
    return GridCell(
        x=int(data.get("x", 0)),
        y=int(data.get("y", 0)),
        areaType=str(data.get("areaType", "Walkable")),
        cost=float(data.get("cost", 1.0)),
        blocked=bool(data.get("blocked", False)),
        sourceCollisionProfile=str(data.get("sourceCollisionProfile", "")),
    )


# /scenario/start 요청 JSON을 ScenarioStartRequest로 변환한다.
def parse_start(data: dict) -> ScenarioStartRequest:
    grid_data = data["grid"]
    goal_data = data["goal"]
    robot_spec = data.get("robotSpec") or data.get("vehicleSpec", {})

    return ScenarioStartRequest(
        robotInstanceId=str(data["robotInstanceId"]),
        start=parse_start_location(data["start"]),
        goal=parse_goal_location(goal_data),
        grid=GridMap(
            gridSizeX=int(grid_data["gridSizeX"]),
            gridSizeY=int(grid_data["gridSizeY"]),
            cellSizeCm=float(grid_data["cellSizeCm"]),
            cellCount=int(grid_data["cellCount"]),
            originCm=parse_start_location(grid_data["originCm"]),
            cells=[
                parse_grid_cell(cell_data)
                for cell_data in grid_data.get("cells", [])
                if isinstance(cell_data, dict)
            ],
        ),
        robotSpec=robot_spec,
        driveSpec=data.get("driveSpec", {}),
        lidarSpec=data.get("lidarSpec", {}),
        artifactSpec=data.get("artifactSpec", {}),
        vehicleSpec=data.get("vehicleSpec", robot_spec),
        controlSpec=data.get("controlSpec", {}),
    )


# LiDAR actor tag 입력을 list[str] 형태로 정리한다.
def parse_actor_tags(data: dict) -> list[str]:
    actor_tags = data.get("actorTags", [])

    if not isinstance(actor_tags, list):
        return []

    return [str(actor_tag) for actor_tag in actor_tags]


# Unreal vector JSON을 cm 단위 dict로 변환한다.
def parse_vector_cm(data: object) -> dict[str, float] | None:
    if not isinstance(data, dict):
        return None

    try:
        return {
            "x": float(data.get("x", 0.0)),
            "y": float(data.get("y", 0.0)),
            "z": float(data.get("z", 0.0)),
        }
    except (TypeError, ValueError):
        return None


# legacy 2D LiDAR ray 입력을 정책용 구조체로 변환한다.
def parse_legacy_lidar_ray(data: dict) -> LidarRay:
    return LidarRay(
        hit=bool(data.get("hit", False)),
        distanceM=float(data.get("distanceM", 0.0)),
        rayIndex=data.get("rayIndex"),
        rayYawDegree=float(data.get("rayYawDegree", data.get("yawDegree", 0.0))),
        actorName=data.get("actorName"),
        actorTags=parse_actor_tags(data),
    )


# 1D LiDAR ray 입력을 구조체로 변환한다.
def parse_lidar_ray_1d(data: dict) -> LidarRay1D:
    return LidarRay1D(
        hit=bool(data.get("hit", False)),
        distanceM=float(data.get("distanceM", 0.0)),
        rayIndex=data.get("rayIndex"),
        actorName=data.get("actorName"),
        actorTags=parse_actor_tags(data),
    )


# 2D LiDAR ray 입력을 구조체로 변환한다.
def parse_lidar_ray_2d(data: dict) -> LidarRay2D:
    return LidarRay2D(
        hit=bool(data.get("hit", False)),
        distanceM=float(data.get("distanceM", 0.0)),
        yawDegree=float(data.get("yawDegree", data.get("rayYawDegree", 0.0))),
        rayIndex=data.get("rayIndex"),
        actorName=data.get("actorName"),
        actorTags=parse_actor_tags(data),
    )


# 3D LiDAR ray 입력을 구조체로 변환한다.
def parse_lidar_ray_3d(data: dict) -> LidarRay3D:
    return LidarRay3D(
        hit=bool(data.get("hit", False)),
        distanceM=float(data.get("distanceM", 0.0)),
        yawDegree=float(data.get("yawDegree", data.get("rayYawDegree", 0.0))),
        pitchDegree=float(data.get("pitchDegree", data.get("rayPitchDegree", 0.0))),
        rayIndex=data.get("rayIndex"),
        actorName=data.get("actorName"),
        actorTags=parse_actor_tags(data),
        hitLocationCm=parse_vector_cm(data.get("hitLocationCm")),
    )


# LiDAR ray 배열을 안전하게 파싱한다.
def parse_lidar_ray_array(values: object, parse_ray) -> list:
    if not isinstance(values, list):
        return []

    return [
        parse_ray(ray_data)
        for ray_data in values
        if isinstance(ray_data, dict)
    ]


# typed LiDAR observation 입력을 구조체로 변환한다.
def parse_lidar_observation(data: dict) -> LidarObservation:
    lidar_data = data.get("lidar", {})

    if not isinstance(lidar_data, dict):
        lidar_data = {}

    return LidarObservation(
        mode=str(lidar_data.get("mode", "")),
        sensorSequence=int(lidar_data.get("sensorSequence", data.get("sensorSequence", 0))),
        sensorTimeSeconds=float(
            lidar_data.get(
                "sensorTimeSeconds",
                data.get("sensorTimeSeconds", data.get("runTimeSeconds", 0.0)),
            )
        ),
        rays1d=parse_lidar_ray_array(lidar_data.get("rays1d", []), parse_lidar_ray_1d),
        rays2d=parse_lidar_ray_array(lidar_data.get("rays2d", []), parse_lidar_ray_2d),
        rays3d=parse_lidar_ray_array(lidar_data.get("rays3d", []), parse_lidar_ray_3d),
    )


# dict 데이터를 RobotState 객체로 변환한다.
def parse_robot_state(data: dict) -> RobotState:
    collision_actor_tags = data.get("collisionActorTags", [])
    if not isinstance(collision_actor_tags, list):
        collision_actor_tags = []

    return RobotState(
        x=float(data.get("x", 0.0)),
        y=float(data.get("y", 0.0)),
        z=float(data.get("z", 0.0)),
        yawDegree=float(data.get("yawDegree", 0.0)),
        speedKmh=float(data.get("speedKmh", 0.0)),
        bColliding=bool(data.get("bColliding", False)),
        collisionActorName=str(data.get("collisionActorName", "")),
        collisionActorTags=[str(actor_tag) for actor_tag in collision_actor_tags],
    )


# /scenario/decide 요청 JSON을 ScenarioDecideRequest로 변환한다.
def parse_decide(data: dict) -> ScenarioDecideRequest:
    lidar = parse_lidar_observation(data)

    return ScenarioDecideRequest(
        sequence=int(data["sequence"]),
        runTimeSeconds=float(data["runTimeSeconds"]),
        sensorSequence=int(data.get("sensorSequence", lidar.sensorSequence)),
        sensorTimeSeconds=float(data.get("sensorTimeSeconds", lidar.sensorTimeSeconds)),
        robotState=parse_robot_state(data["robotState"]),
        lidar=lidar,
        lidarRays=parse_lidar_ray_array(data.get("lidarRays", []), parse_legacy_lidar_ray),
        observedObjects=data.get("observedObjects", []),
    )


# /scenario/end 요청 JSON을 ScenarioEndRequest로 변환한다.
def parse_end(data: dict) -> ScenarioEndRequest:
    return ScenarioEndRequest(
        robotInstanceId=str(data["robotInstanceId"]),
        sequence=int(data["sequence"]),
        status=str(data["status"]),
        error=parse_policy_error(data.get("error")),
        metrics=data.get("metrics", {}),
        debug=data.get("debug", {}),
    )


# dict를 JSON bytes로 변환한다.
def to_json_bytes(data: dict) -> bytes:
    return json.dumps(data, ensure_ascii=False).encode("utf-8")


# Unreal이 PythonAgent 실행/재사용 여부를 확인할 때 쓰는 가벼운 응답
def build_health_response() -> dict:
    return {
        "status": "ok",
        "server": SERVER_NAME,
        "version": SERVER_VERSION,
    }


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
            "decision": {
                "selectedPolicy": "PythonAgentServer",
                "reason": reason,
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
        },
    )


# HTTP 요청을 처리하는 handler
class PythonAgentHandler(BaseHTTPRequestHandler):

    # BaseHTTPRequestHandler 기본 로그 출력을 막는다.
    def log_message(self, format: str, *args) -> None:
        return

    # GET 요청을 처리한다.
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
                "decision": {
                    "selectedPolicy": "PythonAgentServer",
                    "reason": "unknown_endpoint",
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
            },
        )

    # POST 요청을 처리한다.
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

    # HTTP body에서 JSON을 읽는다.
    def read_json_body(self) -> dict:
        content_length = int(self.headers.get("Content-Length", "0"))
        raw_body = self.rfile.read(content_length)

        if not raw_body:
            return {}

        return json.loads(raw_body.decode("utf-8"))

    # 요청 path에 따라 request를 처리하고 response를 envelope에 채운다.
    def route_request(self, request_json: dict) -> dict:
        request_path = self.get_request_path()
        request_data = get_request_data(request_json)

        if request_path == "/scenario/start":
            response = policy.start(parse_start(request_data), state)
            return build_response_message(request_json, response)

        if request_path == "/scenario/decide":
            response = policy.decide(parse_decide(request_data), state)
            return build_response_message(request_json, response)

        if request_path == "/scenario/end":
            response = policy.end(parse_end(request_data), state)
            return build_response_message(request_json, response)

        return build_error_message(
            request_json,
            "NOT_FOUND",
            request_path,
            "unknown_endpoint",
        )

    # JSON 응답을 쓴다.
    def write_json_response(self, status_code: int, data: dict) -> None:
        body = to_json_bytes(data)

        self.send_response(status_code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


# PythonAgent 전용 HTTPServer 설정
class PythonAgentServer(HTTPServer):
    allow_reuse_address = True


# 명령줄 인자를 서버 실행 옵션으로 변환한다.
def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run the DeliveryBot Python Agent HTTP server.")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8000)
    parser.add_argument("--policy-mode", default="runtime")
    parser.add_argument("--verbose-runtime-log", action="store_true")
    return parser.parse_args()


# HTTP 서버를 실행한다.
def run_server(
    host: str = "127.0.0.1",
    port: int = 8000,
    policy_mode: str = "runtime",
    verbose_runtime_log: bool = False,
) -> None:
    server = PythonAgentServer((host, port), PythonAgentHandler)
    print(f"PythonAgent server listening on http://{host}:{port}")
    print(f"PythonAgent policy mode: {policy_mode}")
    if verbose_runtime_log:
        print("PythonAgent verbose runtime log enabled")

    server.serve_forever()


# python server.py로 실행했을 때 서버를 시작한다.
if __name__ == "__main__":
    arguments = parse_arguments()
    run_server(
        host=arguments.host,
        port=arguments.port,
        policy_mode=arguments.policy_mode,
        verbose_runtime_log=arguments.verbose_runtime_log,
    )
