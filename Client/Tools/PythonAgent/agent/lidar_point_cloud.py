# DeliveryBot LiDAR 3D ray를 Python 내부 point cloud view와 capture artifact로 변환한다.
from dataclasses import dataclass, field
import json
import math
from pathlib import Path
from typing import Any

from .contract import LidarRay3D, ScenarioDecideRequest, ScenarioStartRequest


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

    # /scenario/start 설정으로 point cloud capture 상태를 초기화한다.
    def configure_from_start(self, request: ScenarioStartRequest) -> None:
        self.options = build_lidar_point_cloud_options(request.lidarSpec)
        self.lastCapturedSensorSequence = -1
        self.capturesRoot = None

        artifact_spec = request.artifactSpec or {}
        captures_root = str(artifact_spec.get("capturesRoot") or "")
        self.capturesRootRelative = str(artifact_spec.get("capturesRootRelative") or "captures")

        if not captures_root or not self.options.bCaptureEnabled:
            return

        try:
            self.capturesRoot = Path(captures_root) / "lidar_point_cloud"
            self.capturesRoot.mkdir(parents=True, exist_ok=True)
            self._write_manifest()
        except OSError:
            self.capturesRoot = None

    # 현재 decide frame을 point cloud capture 파일로 저장하고 stable response 참조를 반환한다.
    def capture_decide(self, request: ScenarioDecideRequest) -> list[dict[str, Any]]:
        if not self._should_capture(request):
            return []

        frame = build_lidar_point_cloud_frame(request, self.options)
        if not frame["points"]:
            return []

        try:
            file_path = self._write_xyzrgb_frame(frame)
            self._append_frame_index(frame, file_path)
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
                "path": f"{self.capturesRootRelative}/lidar_point_cloud/{file_path.name}",
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

        file_path = self.capturesRoot / f"frame_{frame['sensorSequence']:06d}.xyz"
        lines = []

        for point in frame["points"]:
            red, green, blue = _get_point_color_rgb(point["classification"])
            lines.append(
                f"{point['xM']:.4f} {point['yM']:.4f} {point['zM']:.4f} {red} {green} {blue}"
            )

        file_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
        return file_path

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
            "pointCount": len(points),
            "groundPointCount": sum(1 for point in points if point["classification"] == "ground"),
            "obstaclePointCount": sum(1 for point in points if point["classification"] == "obstacle"),
            "format": "xyzrgb_ascii",
            "path": f"{self.capturesRootRelative}/lidar_point_cloud/{file_path.name}",
        }

        index_path = self.capturesRoot / "frames.jsonl"
        with index_path.open("a", encoding="utf-8") as index_file:
            index_file.write(json.dumps(frame_record, ensure_ascii=False) + "\n")

    # capture 폴더에 사람이 확인할 수 있는 manifest를 저장한다.
    def _write_manifest(self) -> None:
        if self.capturesRoot is None:
            return

        manifest = {
            "schema": "lidar_point_cloud_capture",
            "version": 1,
            "profile": self.options.profile,
            "coordinateFrame": "robot_local",
            "source": "lidar.rays3d",
            "pointFormat": "xyzrgb_ascii",
            "captureEveryNSensorFrames": self.options.captureEveryNSensorFrames,
            "rangeLimitM": self.options.rangeLimitM,
            "includeGroundPoints": self.options.bIncludeGroundPoints,
            "maxPoints": self.options.maxPoints,
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


# lidar.rays3d를 robot-local point cloud frame으로 변환한다.
def build_lidar_point_cloud_frame(
    request: ScenarioDecideRequest,
    options: LidarPointCloudOptions,
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
        "coordinateFrame": "robot_local",
        "points": points,
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


# actor tag 유무와 actor 이름으로 point classification을 정한다.
def _classify_lidar_ray(ray: LidarRay3D) -> str:
    actor_name = ray.actorName or ""
    actor_tags = ray.actorTags or []

    if actor_name.startswith("ScenarioGroundRegion") and len(actor_tags) == 0:
        return "ground"

    if len(actor_tags) == 0:
        return "ground"

    return "obstacle"


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
    if classification == "ground":
        return 120, 120, 120

    if classification == "obstacle":
        return 255, 80, 60

    return 80, 160, 255


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
