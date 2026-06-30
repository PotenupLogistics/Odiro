# DeliveryBot LiDAR 3D ray를 Python 내부 point cloud view와 capture artifact로 변환한다.
from dataclasses import dataclass, field
import json
import math
from pathlib import Path
from typing import Any

from .contract import LidarRay3D, ScenarioDecideRequest, ScenarioStartRequest


# Point cloud capture manifest schema 버전.
POINT_CLOUD_SCHEMA_VERSION = 2

# XYZ 파일에 기록하는 최종 좌표계.
POINT_CLOUD_COORDINATE_FRAME = "unreal_world_map_local"

# LiDAR ray를 해석할 때 사용하는 입력 좌표계.
POINT_CLOUD_SOURCE_COORDINATE_FRAME = "robot_local"

# XYZ 파일에 기록하는 위치 단위.
POINT_CLOUD_UNIT = "centimeter"

# Unreal LiDAR Point Cloud 플러그인에 넘기는 ASCII 포맷 이름.
POINT_CLOUD_POINT_FORMAT = "xyzrgb_ascii"

# XYZRGB ASCII 파일의 컬럼 순서.
POINT_CLOUD_POINT_COLUMNS = ["x_cm", "y_cm", "z_cm", "red", "green", "blue"]

# Unreal Point Cloud import에서 보기 좋게 변환한 좌표축 의미.
POINT_CLOUD_AXIS = {
    "x": "unreal_world_x_minus_capture_origin_x",
    "y": "negative_unreal_world_y_minus_capture_origin_y",
    "z": "unreal_world_z",
}

# Unreal world Y축을 import review 화면의 좌우 방향에 맞춘다.
POINT_CLOUD_IMPORT_Y_SIGN = -1.0

# 여러 frame을 한 번에 검토하기 위한 누적 point cloud 파일명.
POINT_CLOUD_ACCUMULATED_FILE_NAME = "map_accumulated.xyz"

# Unreal world에 그대로 겹쳐 확인하기 위한 누적 point cloud 파일명.
POINT_CLOUD_WORLD_ACCUMULATED_FILE_NAME = "world_accumulated.xyz"

# capture 결과를 사람이 빠르게 검증하기 위한 summary 파일명.
POINT_CLOUD_SUMMARY_FILE_NAME = "capture_summary.json"

# 개별 sensor frame debug 파일을 root import 파일과 분리해 저장하는 폴더명.
POINT_CLOUD_FRAME_DIRECTORY_NAME = "frames"

# Point classification별 표시 색상.
POINT_CLOUD_CLASSIFICATION_COLORS = {
    "ground": (120, 120, 120),
    "wall": (80, 180, 255),
    "obstacle": (255, 80, 60),
    "unknown": (160, 120, 255),
}


# Point cloud 생성과 capture 저장 조건.
@dataclass
class LidarPointCloudOptions:
    profile: str = "basic"                       # observation profile 이름
    bCaptureEnabled: bool = False                # 파일 capture 사용 여부
    captureEveryNSensorFrames: int = 10          # 저장할 sensor frame 간격
    rangeLimitM: float | None = None             # point 변환 최대 거리 제한
    bIncludeGroundPoints: bool = True            # tag 없는 ground point 포함 여부
    maxPoints: int = 4096                        # frame당 저장할 최대 point 수


# Point cloud capture 저장 상태와 경로를 episode 동안 소유한다.
@dataclass
class LidarPointCloudRecorder:
    options: LidarPointCloudOptions = field(default_factory=LidarPointCloudOptions) # 현재 capture 옵션
    capturesRoot: Path | None = None                                                # 실제 capture 저장 루트
    capturesRootRelative: str = "captures"                                          # response에 남길 상대 capture 루트
    lastCapturedSensorSequence: int = -1                                             # 마지막으로 저장한 sensor sequence
    captureOriginCm: dict[str, float] = field(default_factory=dict)                  # import review용 map-local 원점
    captureSummary: dict[str, Any] = field(default_factory=dict)                      # capture 검증용 누적 summary 상태

    # /scenario/start 설정으로 point cloud capture 상태를 초기화하고 저장 경로 오류를 반환한다.
    def configure_from_start(self, request: ScenarioStartRequest) -> dict[str, Any] | None:
        self.options = build_lidar_point_cloud_options(request.lidarSpec)
        self.lastCapturedSensorSequence = -1
        self.capturesRoot = None
        self.captureOriginCm = _get_capture_origin_cm(request)
        self.captureSummary = {}

        artifact_spec = request.artifactSpec or {}
        captures_root = str(artifact_spec.get("capturesRoot") or "")
        self.capturesRootRelative = str(artifact_spec.get("capturesRootRelative") or "captures")
        b_output_required = bool(artifact_spec.get("required", False))

        if not self.options.bCaptureEnabled:
            return None

        configuration_error_code = str(artifact_spec.get("configurationErrorCode") or "")
        if b_output_required and configuration_error_code:
            return {
                "code": configuration_error_code,
                "message": str(
                    artifact_spec.get("configurationErrorMessage")
                    or "Project episode output configuration is invalid."
                ),
                "retryable": False,
            }

        if not captures_root:
            if b_output_required:
                return {
                    "code": "POINT_CLOUD_OUTPUT_PATH_MISSING",
                    "message": "Required project episode output path is missing.",
                    "retryable": False,
                }
            return None

        try:
            self.capturesRoot = Path(captures_root) / "lidar_point_cloud"
            self.capturesRoot.mkdir(parents=True, exist_ok=True)
            self._reset_capture_summary()
            self._reset_accumulated_map()
            self._write_manifest()
            self._write_capture_summary()
        except OSError as error:
            self.capturesRoot = None
            if b_output_required:
                return {
                    "code": "POINT_CLOUD_OUTPUT_UNAVAILABLE",
                    "message": f"Cannot initialize point cloud output at {captures_root}: {error}",
                    "retryable": False,
                }

        return None

    # 현재 decide frame을 point cloud capture 파일로 저장하고 stable response 참조를 반환한다.
    def capture_decide(self, request: ScenarioDecideRequest) -> list[dict[str, Any]]:
        if not self._should_capture(request):
            return []

        frame = build_lidar_point_cloud_frame(request, self.options, self.captureOriginCm)
        if not frame["points"]:
            return []

        try:
            file_path = self._write_xyzrgb_frame(frame)
            self._append_frame_index(frame, file_path)
            self._update_capture_summary(frame, file_path)
            self._write_capture_summary()
            capture_path = self._build_capture_reference_path(file_path)
        except OSError:
            return []

        self.lastCapturedSensorSequence = request.sensorSequence

        return [
            {
                "captureType": "lidar_point_cloud",
                "sensorId": "deliverybot_lidar",
                "sensorSequence": request.sensorSequence,
                "sensorTimeSeconds": request.sensorTimeSeconds,
                "runTimeSeconds": request.runTimeSeconds,
                "format": "xyzrgb_ascii",
                "path": capture_path,
            }
        ]

    # capture 옵션과 sensor sequence 기준으로 저장 여부를 판단한다.
    def _should_capture(self, request: ScenarioDecideRequest) -> bool:
        if self.capturesRoot is None:
            return False

        if self.options.profile == "basic":
            return False

        if request.sensorSequence == self.lastCapturedSensorSequence:
            return False

        every_n = max(1, self.options.captureEveryNSensorFrames)
        return request.sensorSequence % every_n == 0

    # point cloud frame을 xyzrgb ASCII 파일로 저장한다.
    def _write_xyzrgb_frame(self, frame: dict[str, Any]) -> Path:
        if self.capturesRoot is None:
            raise OSError("captures root is not configured")

        frame_directory = self.capturesRoot / POINT_CLOUD_FRAME_DIRECTORY_NAME
        frame_directory.mkdir(parents=True, exist_ok=True)

        file_path = frame_directory / f"frame_{frame['sensorSequence']:06d}.xyz"
        lines = _build_xyzrgb_lines(frame["points"])
        world_lines = _build_world_xyzrgb_lines(frame["points"])

        file_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
        self._append_accumulated_map(lines)
        self._append_world_accumulated_map(world_lines)
        return file_path

    # 누적 point cloud 파일들을 episode 시작 시 비운다.
    def _reset_accumulated_map(self) -> None:
        if self.capturesRoot is None:
            return

        accumulated_path = self.capturesRoot / POINT_CLOUD_ACCUMULATED_FILE_NAME
        world_accumulated_path = self.capturesRoot / POINT_CLOUD_WORLD_ACCUMULATED_FILE_NAME
        frame_index_path = self.capturesRoot / "frames.jsonl"
        accumulated_path.write_text("", encoding="utf-8")
        world_accumulated_path.write_text("", encoding="utf-8")
        frame_index_path.write_text("", encoding="utf-8")

    # capture summary 상태를 빈 capture 기준으로 초기화한다.
    def _reset_capture_summary(self) -> None:
        self.captureSummary = {
            "schema": "lidar_point_cloud_capture_summary",
            "version": POINT_CLOUD_SCHEMA_VERSION,
            "captureType": "lidar_point_cloud",
            "coordinateFrame": POINT_CLOUD_COORDINATE_FRAME,
            "sourceCoordinateFrame": POINT_CLOUD_SOURCE_COORDINATE_FRAME,
            "pointUnit": POINT_CLOUD_UNIT,
            "pointFormat": POINT_CLOUD_POINT_FORMAT,
            "mainReviewFile": POINT_CLOUD_ACCUMULATED_FILE_NAME,
            "debugWorldFile": POINT_CLOUD_WORLD_ACCUMULATED_FILE_NAME,
            "summaryFile": POINT_CLOUD_SUMMARY_FILE_NAME,
            "frameDirectory": POINT_CLOUD_FRAME_DIRECTORY_NAME,
            "frameFilesAreDebugOnly": True,
            "captureOriginCm": self.captureOriginCm,
            "importYAxisSign": POINT_CLOUD_IMPORT_Y_SIGN,
            "frameCount": 0,
            "totalPointCount": 0,
            "groundPointCount": 0,
            "wallPointCount": 0,
            "obstaclePointCount": 0,
            "unknownPointCount": 0,
            "firstSensorSequence": None,
            "lastSensorSequence": None,
            "firstSensorTimeSeconds": None,
            "lastSensorTimeSeconds": None,
            "firstRunTimeSeconds": None,
            "lastRunTimeSeconds": None,
            "boundsCm": None,
            "worldBoundsCm": None,
            "latestFramePath": "",
        }

    # 현재 frame point를 누적 point cloud 파일에 추가한다.
    def _append_accumulated_map(self, lines: list[str]) -> None:
        if self.capturesRoot is None or not lines:
            return

        accumulated_path = self.capturesRoot / POINT_CLOUD_ACCUMULATED_FILE_NAME
        with accumulated_path.open("a", encoding="utf-8") as accumulated_file:
            accumulated_file.write("\n".join(lines) + "\n")

    # 현재 frame world point를 월드 오버레이용 누적 point cloud 파일에 추가한다.
    def _append_world_accumulated_map(self, lines: list[str]) -> None:
        if self.capturesRoot is None or not lines:
            return

        accumulated_path = self.capturesRoot / POINT_CLOUD_WORLD_ACCUMULATED_FILE_NAME
        with accumulated_path.open("a", encoding="utf-8") as accumulated_file:
            accumulated_file.write("\n".join(lines) + "\n")

    # 저장한 point cloud frame 정보를 frames.jsonl에 기록한다.
    def _append_frame_index(self, frame: dict[str, Any], file_path: Path) -> None:
        if self.capturesRoot is None:
            return

        points = frame["points"]
        frame_record = {
            "captureType": "lidar_point_cloud",
            "sensorId": "deliverybot_lidar",
            "sensorSequence": frame["sensorSequence"],
            "sensorTimeSeconds": frame["sensorTimeSeconds"],
            "runTimeSeconds": frame["runTimeSeconds"],
            "coordinateFrame": frame["coordinateFrame"],
            "sourceCoordinateFrame": frame["sourceCoordinateFrame"],
            "pointUnit": POINT_CLOUD_UNIT,
            "pointCount": len(points),
            "groundPointCount": sum(1 for point in points if point["classification"] == "ground"),
            "wallPointCount": sum(1 for point in points if point["classification"] == "wall"),
            "obstaclePointCount": sum(1 for point in points if point["classification"] == "obstacle"),
            "unknownPointCount": sum(
                1 for point in points if point["classification"] not in ("ground", "wall", "obstacle")
            ),
            "format": POINT_CLOUD_POINT_FORMAT,
            "captureOriginCm": self.captureOriginCm,
            "importYAxisSign": POINT_CLOUD_IMPORT_Y_SIGN,
            "robotPoseCm": frame["robotPoseCm"],
            "path": self._build_capture_reference_path(file_path),
            "accumulatedPath": f"{self.capturesRootRelative}/lidar_point_cloud/{POINT_CLOUD_ACCUMULATED_FILE_NAME}",
            "worldAccumulatedPath": f"{self.capturesRootRelative}/lidar_point_cloud/{POINT_CLOUD_WORLD_ACCUMULATED_FILE_NAME}",
            "summaryPath": f"{self.capturesRootRelative}/lidar_point_cloud/{POINT_CLOUD_SUMMARY_FILE_NAME}",
        }

        index_path = self.capturesRoot / "frames.jsonl"
        with index_path.open("a", encoding="utf-8") as index_file:
            index_file.write(json.dumps(frame_record, ensure_ascii=False) + "\n")

    # capture root 기준 파일 경로를 Python response와 metadata용 참조 문자열로 변환한다.
    def _build_capture_reference_path(self, file_path: Path) -> str:
        if self.capturesRoot is None:
            relative_path = file_path.name
        else:
            try:
                relative_path = file_path.relative_to(self.capturesRoot).as_posix()
            except ValueError:
                relative_path = file_path.name

        return f"{self.capturesRootRelative}/lidar_point_cloud/{relative_path}"

    # 저장된 frame 정보를 capture summary 상태에 누적한다.
    def _update_capture_summary(self, frame: dict[str, Any], file_path: Path) -> None:
        if not self.captureSummary:
            self._reset_capture_summary()

        points = frame["points"]
        frame_count = int(self.captureSummary["frameCount"]) + 1

        if self.captureSummary["firstSensorSequence"] is None:
            self.captureSummary["firstSensorSequence"] = frame["sensorSequence"]
            self.captureSummary["firstSensorTimeSeconds"] = frame["sensorTimeSeconds"]
            self.captureSummary["firstRunTimeSeconds"] = frame["runTimeSeconds"]

        self.captureSummary["frameCount"] = frame_count
        self.captureSummary["totalPointCount"] = int(self.captureSummary["totalPointCount"]) + len(points)
        self.captureSummary["groundPointCount"] = int(self.captureSummary["groundPointCount"]) + sum(
            1 for point in points if point["classification"] == "ground"
        )
        self.captureSummary["wallPointCount"] = int(self.captureSummary["wallPointCount"]) + sum(
            1 for point in points if point["classification"] == "wall"
        )
        self.captureSummary["obstaclePointCount"] = int(self.captureSummary["obstaclePointCount"]) + sum(
            1 for point in points if point["classification"] == "obstacle"
        )
        self.captureSummary["unknownPointCount"] = int(self.captureSummary["unknownPointCount"]) + sum(
            1 for point in points if point["classification"] not in ("ground", "wall", "obstacle")
        )
        self.captureSummary["lastSensorSequence"] = frame["sensorSequence"]
        self.captureSummary["lastSensorTimeSeconds"] = frame["sensorTimeSeconds"]
        self.captureSummary["lastRunTimeSeconds"] = frame["runTimeSeconds"]
        self.captureSummary["latestFramePath"] = self._build_capture_reference_path(file_path)
        self.captureSummary["boundsCm"] = _merge_point_bounds_cm(
            self.captureSummary["boundsCm"],
            _get_point_bounds_cm(points, "xCm", "yCm", "zCm"),
        )
        self.captureSummary["worldBoundsCm"] = _merge_point_bounds_cm(
            self.captureSummary["worldBoundsCm"],
            _get_point_bounds_cm(points, "worldXCm", "worldYCm", "worldZCm"),
        )

    # capture summary 상태를 JSON 파일로 저장한다.
    def _write_capture_summary(self) -> None:
        if self.capturesRoot is None:
            return

        summary_path = self.capturesRoot / POINT_CLOUD_SUMMARY_FILE_NAME
        summary_path.write_text(
            json.dumps(self.captureSummary, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )

    # capture 폴더에 사람이 확인할 수 있는 manifest를 저장한다.
    def _write_manifest(self) -> None:
        if self.capturesRoot is None:
            return

        manifest = {
            "schema": "lidar_point_cloud_capture",
            "version": POINT_CLOUD_SCHEMA_VERSION,
            "profile": self.options.profile,
            "coordinateFrame": POINT_CLOUD_COORDINATE_FRAME,
            "sourceCoordinateFrame": POINT_CLOUD_SOURCE_COORDINATE_FRAME,
            "pointUnit": POINT_CLOUD_UNIT,
            "axis": POINT_CLOUD_AXIS,
            "captureOriginCm": self.captureOriginCm,
            "importYAxisSign": POINT_CLOUD_IMPORT_Y_SIGN,
            "source": "lidar.rays3d",
            "pointSource": "hitLocationCm_or_reconstructed_ray",
            "pointFormat": POINT_CLOUD_POINT_FORMAT,
            "pointColumns": POINT_CLOUD_POINT_COLUMNS,
            "colorMode": "classification_rgb",
            "classificationColors": _get_classification_color_manifest(),
            "summaryFile": POINT_CLOUD_SUMMARY_FILE_NAME,
            "captureEveryNSensorFrames": self.options.captureEveryNSensorFrames,
            "rangeLimitM": self.options.rangeLimitM,
            "includeGroundPoints": self.options.bIncludeGroundPoints,
            "maxPoints": self.options.maxPoints,
            "frameDirectory": POINT_CLOUD_FRAME_DIRECTORY_NAME,
            "frameFilesAreDebugOnly": True,
            "unrealImport": {
                "plugin": "LiDAR Point Cloud Support",
                "viewerRole": "official_review",
                "unrealUnit": "centimeter",
                "recommendedImportScale": 1,
                "recommendedActorLocation": {"x": 0, "y": 0, "z": 0},
                "mainReviewFile": POINT_CLOUD_ACCUMULATED_FILE_NAME,
                "debugWorldFile": POINT_CLOUD_WORLD_ACCUMULATED_FILE_NAME,
                "summaryFile": POINT_CLOUD_SUMMARY_FILE_NAME,
                "accumulatedFile": POINT_CLOUD_ACCUMULATED_FILE_NAME,
                "worldAccumulatedFile": POINT_CLOUD_WORLD_ACCUMULATED_FILE_NAME,
                "frameDirectory": POINT_CLOUD_FRAME_DIRECTORY_NAME,
                "frameFilesAreDebugOnly": True,
                "note": "map_accumulated.xyz is the official plugin review file. Per-frame files under frames/ are debug-only and should not be imported together for map review. world_accumulated.xyz is for raw Unreal world-coordinate validation.",
            },
        }

        manifest_path = self.capturesRoot / "manifest.json"
        manifest_path.write_text(
            json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )


# lidarSpec에서 point cloud 옵션을 안정된 기본값으로 읽는다.
def build_lidar_point_cloud_options(lidar_spec: dict[str, Any]) -> LidarPointCloudOptions:
    point_cloud_spec = lidar_spec.get("pointCloudOptions", {})
    if not isinstance(point_cloud_spec, dict):
        point_cloud_spec = {}

    profile = str(lidar_spec.get("observationProfile", "basic"))

    return LidarPointCloudOptions(
        profile=profile,
        bCaptureEnabled=bool(point_cloud_spec.get("captureEnabled", False)),
        captureEveryNSensorFrames=_get_int_option(point_cloud_spec, "captureEveryNSensorFrames", 10),
        rangeLimitM=_get_float_option_or_none(point_cloud_spec, "rangeLimitM"),
        bIncludeGroundPoints=bool(point_cloud_spec.get("includeGroundPoints", True)),
        maxPoints=max(1, _get_int_option(point_cloud_spec, "maxPoints", 4096)),
    )


# /scenario/start의 시작 위치를 point cloud import review 원점으로 만든다.
def _get_capture_origin_cm(request: ScenarioStartRequest) -> dict[str, float]:
    return {
        "x": request.start.x,
        "y": request.start.y,
        "z": 0.0,
    }


# 원점 정보가 없을 때 사용하는 point cloud import review 원점.
def _get_zero_origin_cm() -> dict[str, float]:
    return {
        "x": 0.0,
        "y": 0.0,
        "z": 0.0,
    }


# point 목록을 Unreal LiDAR Point Cloud용 xyzrgb ASCII line으로 변환한다.
def _build_xyzrgb_lines(points: list[dict[str, Any]]) -> list[str]:
    lines = []

    for point in points:
        red, green, blue = _get_point_color_rgb(point["classification"])
        lines.append(
            f"{point['xCm']:.4f} {point['yCm']:.4f} {point['zCm']:.4f} {red} {green} {blue}"
        )

    return lines


# point 목록을 Unreal world overlay용 xyzrgb ASCII line으로 변환한다.
def _build_world_xyzrgb_lines(points: list[dict[str, Any]]) -> list[str]:
    lines = []

    for point in points:
        red, green, blue = _get_point_color_rgb(point["classification"])
        lines.append(
            f"{point['worldXCm']:.4f} {point['worldYCm']:.4f} {point['worldZCm']:.4f} {red} {green} {blue}"
        )

    return lines


# point 목록에서 지정 좌표 key 기준 bounds를 계산한다.
def _get_point_bounds_cm(
    points: list[dict[str, Any]],
    x_key: str,
    y_key: str,
    z_key: str,
) -> dict[str, float] | None:
    bounds = None

    for point in points:
        try:
            x = float(point[x_key])
            y = float(point[y_key])
            z = float(point[z_key])
        except (KeyError, TypeError, ValueError):
            continue

        if bounds is None:
            bounds = {
                "minX": x,
                "maxX": x,
                "minY": y,
                "maxY": y,
                "minZ": z,
                "maxZ": z,
            }
            continue

        bounds["minX"] = min(bounds["minX"], x)
        bounds["maxX"] = max(bounds["maxX"], x)
        bounds["minY"] = min(bounds["minY"], y)
        bounds["maxY"] = max(bounds["maxY"], y)
        bounds["minZ"] = min(bounds["minZ"], z)
        bounds["maxZ"] = max(bounds["maxZ"], z)

    return bounds


# 기존 bounds와 새 frame bounds를 합쳐 episode 전체 bounds를 만든다.
def _merge_point_bounds_cm(
    current_bounds: dict[str, float] | None,
    new_bounds: dict[str, float] | None,
) -> dict[str, float] | None:
    if current_bounds is None:
        return new_bounds

    if new_bounds is None:
        return current_bounds

    return {
        "minX": min(current_bounds["minX"], new_bounds["minX"]),
        "maxX": max(current_bounds["maxX"], new_bounds["maxX"]),
        "minY": min(current_bounds["minY"], new_bounds["minY"]),
        "maxY": max(current_bounds["maxY"], new_bounds["maxY"]),
        "minZ": min(current_bounds["minZ"], new_bounds["minZ"]),
        "maxZ": max(current_bounds["maxZ"], new_bounds["maxZ"]),
    }


# lidar.rays3d를 import review용 map-local point cloud frame으로 변환한다.
def build_lidar_point_cloud_frame(
    request: ScenarioDecideRequest,
    options: LidarPointCloudOptions,
    capture_origin_cm: dict[str, float] | None = None,
) -> dict[str, Any]:
    points = []

    for ray in request.lidar.rays3d:
        if not ray.hit:
            continue

        if options.rangeLimitM is not None and ray.distanceM > options.rangeLimitM:
            continue

        classification = _classify_lidar_ray(ray)
        if classification == "ground" and not options.bIncludeGroundPoints:
            continue

        point = _convert_ray_to_local_point_m(ray)
        world_point_cm = _convert_ray_to_unreal_world_point_cm(request, ray, point)
        point.update(_get_world_point_metadata_cm(world_point_cm))
        point.update(_convert_world_point_to_import_point_cm(world_point_cm, capture_origin_cm))
        point.update({
            "distanceM": ray.distanceM,
            "yawDegree": ray.yawDegree,
            "pitchDegree": ray.pitchDegree,
            "rayIndex": ray.rayIndex,
            "actorName": ray.actorName or "",
            "actorTags": list(ray.actorTags or []),
            "classification": classification,
        })
        points.append(point)

    if len(points) > options.maxPoints:
        points = _downsample_points(points, options.maxPoints)

    return {
        "frameId": str(request.sensorSequence),
        "sensorSequence": request.sensorSequence,
        "sensorTimeSeconds": request.sensorTimeSeconds,
        "runTimeSeconds": request.runTimeSeconds,
        "coordinateFrame": POINT_CLOUD_COORDINATE_FRAME,
        "sourceCoordinateFrame": POINT_CLOUD_SOURCE_COORDINATE_FRAME,
        "captureOriginCm": capture_origin_cm or _get_zero_origin_cm(),
        "robotPoseCm": _get_robot_pose_cm(request),
        "points": points,
    }


# LiDAR ray의 실제 hit location을 우선 사용해 Unreal world point를 만든다.
def _convert_ray_to_unreal_world_point_cm(
    request: ScenarioDecideRequest,
    ray: LidarRay3D,
    local_point_m: dict[str, float],
) -> dict[str, float]:
    hit_location_cm = _get_ray_hit_location_cm(ray)
    if hit_location_cm is not None:
        return hit_location_cm

    return _convert_local_point_to_unreal_world_cm(request, local_point_m)


# Unreal world point를 metadata로 보존한다.
def _get_world_point_metadata_cm(world_point_cm: dict[str, float]) -> dict[str, float]:
    return {
        "worldXCm": world_point_cm["xCm"],
        "worldYCm": world_point_cm["yCm"],
        "worldZCm": world_point_cm["zCm"],
    }


# Unreal world point를 import review용 map-local point로 변환한다.
def _convert_world_point_to_import_point_cm(
    world_point_cm: dict[str, float],
    capture_origin_cm: dict[str, float] | None,
) -> dict[str, float]:
    origin = capture_origin_cm or _get_zero_origin_cm()

    return {
        "xCm": world_point_cm["xCm"] - float(origin.get("x", 0.0)),
        "yCm": (world_point_cm["yCm"] - float(origin.get("y", 0.0))) * POINT_CLOUD_IMPORT_Y_SIGN,
        "zCm": world_point_cm["zCm"],
    }


# LiDAR yaw/pitch/distance를 robot-local meter 좌표로 변환한다.
def _convert_ray_to_local_point_m(ray: LidarRay3D) -> dict[str, float]:
    yaw_radian = math.radians(ray.yawDegree)
    pitch_radian = math.radians(ray.pitchDegree)
    horizontal_distance_m = ray.distanceM * math.cos(pitch_radian)

    return {
        "xM": horizontal_distance_m * math.cos(yaw_radian),
        "yM": horizontal_distance_m * math.sin(yaw_radian),
        "zM": ray.distanceM * math.sin(pitch_radian),
    }


# LiDAR ray에 포함된 Unreal hit location을 point cloud 컬럼 이름으로 변환한다.
def _get_ray_hit_location_cm(ray: LidarRay3D) -> dict[str, float] | None:
    if ray.hitLocationCm is None:
        return None

    try:
        return {
            "xCm": float(ray.hitLocationCm["x"]),
            "yCm": float(ray.hitLocationCm["y"]),
            "zCm": float(ray.hitLocationCm["z"]),
        }
    except (KeyError, TypeError, ValueError):
        return None


# robot-local meter point를 Unreal world centimeter 좌표로 변환한다.
def _convert_local_point_to_unreal_world_cm(
    request: ScenarioDecideRequest,
    local_point_m: dict[str, float],
) -> dict[str, float]:
    robot_yaw_radian = math.radians(request.robotState.yawDegree)
    local_x_cm = local_point_m["xM"] * 100.0
    local_y_cm = local_point_m["yM"] * 100.0
    local_z_cm = local_point_m["zM"] * 100.0

    cos_yaw = math.cos(robot_yaw_radian)
    sin_yaw = math.sin(robot_yaw_radian)

    return {
        "xCm": request.robotState.x + (local_x_cm * cos_yaw) - (local_y_cm * sin_yaw),
        "yCm": request.robotState.y + (local_x_cm * sin_yaw) + (local_y_cm * cos_yaw),
        "zCm": request.robotState.z + local_z_cm,
    }


# point cloud frame에 남길 robot pose metadata를 만든다.
def _get_robot_pose_cm(request: ScenarioDecideRequest) -> dict[str, float]:
    return {
        "x": request.robotState.x,
        "y": request.robotState.y,
        "z": request.robotState.z,
        "yawDegree": request.robotState.yawDegree,
    }


# actor tag 유무와 actor 이름으로 point classification을 정한다.
def _get_lidar_actor_markers(ray: LidarRay3D) -> tuple[str, set[str]]:
    actor_name = str(ray.actorName or "").lower()
    actor_tags = {str(tag).lower() for tag in (ray.actorTags or [])}
    return actor_name, actor_tags


# Returns true when any marker appears in the actor name or tags.
def _has_lidar_marker(actor_name: str, actor_tags: set[str], markers: tuple[str, ...]) -> bool:
    return any(marker in actor_name or any(marker in tag for tag in actor_tags) for marker in markers)


# Reads the Unreal hit Z coordinate from a LiDAR ray when it is available.
def _get_lidar_hit_z_cm(ray: LidarRay3D) -> float | None:
    if ray.hitLocationCm is None:
        return None

    try:
        return float(ray.hitLocationCm["z"])
    except (KeyError, TypeError, ValueError):
        return None


# Classifies one LiDAR hit for point cloud review coloring.
def _classify_lidar_ray(ray: LidarRay3D) -> str:
    actor_name, actor_tags = _get_lidar_actor_markers(ray)
    hit_z_cm = _get_lidar_hit_z_cm(ray)

    if _has_lidar_marker(actor_name, actor_tags, ("obstacle", "pedestrian", "vehicle", "prop")):
        return "obstacle"

    if _has_lidar_marker(actor_name, actor_tags, ("wall", "barrier", "fence", "building")):
        return "wall"

    if _has_lidar_marker(actor_name, actor_tags, ("ground", "floor", "road", "walkable", "lane")):
        return "ground"

    if "corridor" in actor_name:
        if hit_z_cm is not None and hit_z_cm > 12.0:
            return "wall"
        return "ground"

    if len(actor_tags) == 0:
        return "ground"

    return "unknown"


# point 수가 제한을 넘으면 균등 간격으로 샘플링한다.
def _downsample_points(points: list[dict[str, Any]], max_points: int) -> list[dict[str, Any]]:
    if len(points) <= max_points:
        return points

    step = len(points) / float(max_points)
    return [
        points[min(len(points) - 1, int(index * step))]
        for index in range(max_points)
    ]


# point classification을 xyzrgb 저장용 색으로 변환한다.
def _get_point_color_rgb(classification: str) -> tuple[int, int, int]:
    color = POINT_CLOUD_CLASSIFICATION_COLORS.get(classification)
    if color is not None:
        return color

    return POINT_CLOUD_CLASSIFICATION_COLORS["unknown"]


# classification 색상 상수를 manifest 저장용 list dict로 변환한다.
def _get_classification_color_manifest() -> dict[str, list[int]]:
    return {
        classification: list(color)
        for classification, color in POINT_CLOUD_CLASSIFICATION_COLORS.items()
    }


# option dict에서 int 값을 안전하게 읽는다.
def _get_int_option(options: dict[str, Any], key: str, default_value: int) -> int:
    try:
        return int(options.get(key, default_value))
    except (TypeError, ValueError):
        return default_value


# option dict에서 optional float 값을 안전하게 읽는다.
def _get_float_option_or_none(options: dict[str, Any], key: str) -> float | None:
    value = options.get(key)
    if value is None:
        return None

    try:
        return float(value)
    except (TypeError, ValueError):
        return None
