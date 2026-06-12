import json
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import urlparse

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


SERVER_NAME = "PythonAgent"
SERVER_VERSION = "0.1"


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


# Unreal이 PythonAgent 실행/재사용 여부를 확인할 때 쓰는 가장 가벼운 응답
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


    # JSON 응답 쓰기
    def write_json_response(self, status_code: int, data: dict) -> None:
        body = to_json_bytes(data)

        self.send_response(status_code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


# HTTP 서버 실행
def run_server(host: str = "127.0.0.1", port: int = 8000) -> None:
    server = HTTPServer((host, port), PythonAgentHandler)
    print(f"PythonAgent server listening on http://{host}:{port}")
    server.serve_forever()


# python server.py로 실행했을 때 서버 시작
if __name__ == "__main__":
    run_server()
