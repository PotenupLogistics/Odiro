# DeliveryBot 정책 결정을 위한 Client-local Python Agent HTTP 서버를 실행한다.
import argparse
import json
from http.server import BaseHTTPRequestHandler, HTTPServer

from agent import create_policy
from agent.contract import (
    GoalLocation,
    GridCell,
    GridMap,
    LidarRay,
    PolicyError,
    RobotState,
    ScenarioDecideRequest,
    ScenarioEndRequest,
    ScenarioStartRequest,
    StartLocation,
)
from agent.state import AgentState


# episode 동안 유지되는 PythonAgent 상태
state = AgentState()


# user_agent.py의 create_policy()를 통해 BotPolicy 생성
policy = create_policy()


# dict 데이터를 PolicyError 객체로 변환
def parse_policy_error(data: dict | None) -> PolicyError | None:
    if not data:
        return None

    return PolicyError(
        code=data.get("code", "UNKNOWN_ERROR"),
        message=data.get("message", ""),
        retryable=data.get("retryable", False),
        details=data.get("details", {}),
    )


# /scenario/start 요청 JSON을 ScenarioStartRequest로 변환
def parse_start(data: dict) -> ScenarioStartRequest:
    grid_data = data["grid"]

    return ScenarioStartRequest(
        experimentId=data.get("experimentId"),
        episodeId=data["episodeId"],
        robotInstanceId=data["robotInstanceId"],
        start=StartLocation(**data["start"]),
        goal=GoalLocation(**data["goal"]),
        grid=GridMap(
            gridSizeX=grid_data["gridSizeX"],
            gridSizeY=grid_data["gridSizeY"],
            cellSizeCm=grid_data["cellSizeCm"],
            cellCount=grid_data["cellCount"],
            originCm=StartLocation(**grid_data["originCm"]),
            cells=[
                GridCell(**cell_data)
                for cell_data in grid_data.get("cells", [])
            ],
        ),
        vehicleSpec=data.get("vehicleSpec", {}),
        lidarSpec=data.get("lidarSpec", {}),
        controlSpec=data.get("controlSpec", {}),
    )


# /scenario/decide 요청 JSON을 ScenarioDecideRequest로 변환
def parse_decide(data: dict) -> ScenarioDecideRequest:
    return ScenarioDecideRequest(
        sequence=data["sequence"],
        runTimeSeconds=data["runTimeSeconds"],
        robotState=RobotState(**data["robotState"]),
        lidarRays=[
            LidarRay(**ray_data)
            for ray_data in data.get("lidarRays", [])
        ],
        observedObjects=data.get("observedObjects", []),
    )


# /scenario/end 요청 JSON을 ScenarioEndRequest로 변환
def parse_end(data: dict) -> ScenarioEndRequest:
    return ScenarioEndRequest(
        experimentId=data.get("experimentId"),
        episodeId=data["episodeId"],
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


# HTTP 요청을 처리하는 handler
class PythonAgentHandler(BaseHTTPRequestHandler):

    # POST 요청 처리
    def do_POST(self) -> None:
        try:
            request_json = self.read_json_body()
            response = self.route_request(request_json)
            self.write_json_response(200, response)

        except Exception as error:
            self.write_json_response(
                500,
                {
                    "status": "error",
                    "error": {
                        "code": "SERVER_ERROR",
                        "message": str(error),
                    },
                    "debug": {
                        "reason": "server_exception",
                    },
                },
            )


    # HTTP body에서 JSON 읽기
    def read_json_body(self) -> dict:
        content_length = int(self.headers.get("Content-Length", "0"))
        raw_body = self.rfile.read(content_length)
        return json.loads(raw_body.decode("utf-8"))


    # 요청 path에 따라 BotPolicy 함수 호출
    def route_request(self, request_json: dict) -> dict:
        if self.path == "/scenario/start":
            return policy.start(parse_start(request_json), state)

        if self.path == "/scenario/decide":
            return policy.decide(parse_decide(request_json), state)

        if self.path == "/scenario/end":
            return policy.end(parse_end(request_json), state)

        return {
            "status": "error",
            "error": {
                "code": "NOT_FOUND",
                "message": self.path,
            },
            "debug": {
                "reason": "unknown_endpoint",
            },
        }


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
    parser.add_argument("--policy-mode", default="runtime")
    parser.add_argument("--verbose-runtime-log", action="store_true")
    return parser.parse_args()


# HTTP 서버 실행
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


# python server.py로 실행했을 때 서버 시작
if __name__ == "__main__":
    arguments = parse_arguments()
    run_server(
        host=arguments.host,
        port=arguments.port,
        policy_mode=arguments.policy_mode,
        verbose_runtime_log=arguments.verbose_runtime_log,
    )
