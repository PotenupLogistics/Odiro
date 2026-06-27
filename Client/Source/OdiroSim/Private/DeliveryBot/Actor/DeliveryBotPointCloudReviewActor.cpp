#include "DeliveryBot/Actor/DeliveryBotPointCloudReviewActor.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"
#include "LidarPointCloud.h"
#include "LidarPointCloudComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Paths.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeliveryBotPointCloudReview, Log, All);

namespace
{
	const TCHAR* MapAccumulatedFileName = TEXT("map_accumulated.xyz");
	const TCHAR* TopDownSphereMeshPath = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	const TCHAR* TopDownSphereMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
	const FName PointClassificationGround(TEXT("ground"));
	const FName PointClassificationWall(TEXT("wall"));
	const FName PointClassificationObstacle(TEXT("obstacle"));
	const FName PointClassificationUnknown(TEXT("unknown"));
	const FName MaterialColorParameterName(TEXT("Color"));
	const FName MaterialBaseColorParameterName(TEXT("BaseColor"));
	const FName MaterialTintColorParameterName(TEXT("TintColor"));
	const FColor GroundPointColor(120, 120, 120);
	const FColor WallPointColor(80, 180, 255);
	const FColor ObstaclePointColor(255, 80, 60);
	const FColor UnknownPointColor(160, 120, 255);
	const FColor LegacyUnknownPointColor(80, 160, 255);
}

ADeliveryBotPointCloudReviewActor::ADeliveryBotPointCloudReviewActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	PointCloudComponent = CreateDefaultSubobject<ULidarPointCloudComponent>(TEXT("PointCloudComponent"));
	PointCloudComponent->SetupAttachment(SceneRoot);
	PointCloudComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PointCloudComponent->SetMobility(EComponentMobility::Movable);
	ConfigurePointCloudRendering();

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TopDownSphereMeshPath);
	if (SphereMesh.Succeeded())
	{
		TopDownSphereMesh = SphereMesh.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SphereMaterial(TopDownSphereMaterialPath);
	if (SphereMaterial.Succeeded())
	{
		TopDownSphereBaseMaterial = SphereMaterial.Object;
	}

	TopDownGroundPointInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("TopDownGroundPointInstances"));
	TopDownGroundPointInstances->SetupAttachment(SceneRoot);
	ConfigureTopDownSphereInstanceComponent(TopDownGroundPointInstances);

	TopDownWallPointInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("TopDownWallPointInstances"));
	TopDownWallPointInstances->SetupAttachment(SceneRoot);
	ConfigureTopDownSphereInstanceComponent(TopDownWallPointInstances);

	TopDownObstaclePointInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("TopDownObstaclePointInstances"));
	TopDownObstaclePointInstances->SetupAttachment(SceneRoot);
	ConfigureTopDownSphereInstanceComponent(TopDownObstaclePointInstances);

	TopDownUnknownPointInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("TopDownUnknownPointInstances"));
	TopDownUnknownPointInstances->SetupAttachment(SceneRoot);
	ConfigureTopDownSphereInstanceComponent(TopDownUnknownPointInstances);
}

// BeginPlay에서 옵션에 따라 xyz 파일을 자동 로드한다.
void ADeliveryBotPointCloudReviewActor::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoLoadOnBeginPlay)
	{
		LoadPointCloudFile();
	}
}

// 에디터 배치/속성 변경 시 옵션에 따라 xyz 파일을 자동 로드한다.
void ADeliveryBotPointCloudReviewActor::OnConstruction(const FTransform& transform)
{
	Super::OnConstruction(transform);

	if (bAutoLoadOnConstruction)
	{
		LoadPointCloudFile();
	}
}

// 설정된 xyz 파일을 읽고 point instance를 다시 만든다.
bool ADeliveryBotPointCloudReviewActor::LoadPointCloudFile()
{
	ClearPointCloud();

	if (PointCloudComponent == nullptr)
	{
		UE_LOG(LogDeliveryBotPointCloudReview, Warning, TEXT("PointCloudComponent is missing."));
		return false;
	}

	const FString filePath = XyzFilePath.FilePath;
	if (filePath.IsEmpty())
	{
		UE_LOG(LogDeliveryBotPointCloudReview, Warning, TEXT("XyzFilePath is empty."));
		return false;
	}

	if (!FPaths::FileExists(filePath))
	{
		UE_LOG(LogDeliveryBotPointCloudReview, Warning, TEXT("xyz file does not exist: %s"), *filePath);
		return false;
	}

	TArray<FString> lines;
	if (!FFileHelper::LoadFileToStringArray(lines, *filePath))
	{
		UE_LOG(LogDeliveryBotPointCloudReview, Warning, TEXT("Failed to read xyz file: %s"), *filePath);
		return false;
	}

	LoadedPoints.Reserve(FMath::Min(lines.Num(), MaxPointCount));

	for (const FString& line : lines)
	{
		if (LoadedPoints.Num() >= MaxPointCount)
			break;

		FDeliveryBotPointCloudReviewPointInfo point;
		if (ParseXyzLine(line, point))
		{
			LoadedPoints.Add(point);
		}
	}

	if (!RebuildPointCloudAsset())
	{
		return false;
	}

	UE_LOG(
		LogDeliveryBotPointCloudReview,
		Log,
		TEXT("Loaded xyz point cloud with LidarPointCloudRuntime: path=%s pointCount=%d coordinateType=%s mapLocalImport=%s"),
		*filePath,
		LoadedPoints.Num(),
		CoordinateType == EDeliveryBotPointCloudCoordinateTypes::World ? TEXT("World") : TEXT("ActorLocal"),
		bUseMapLocalImportTransform ? TEXT("true") : TEXT("false"));

	return LoadedPoints.Num() > 0;
}

// 외부에서 받은 xyz 경로를 절대 경로로 정규화하고 검증한다.
bool ADeliveryBotPointCloudReviewActor::TryResolveXyzFilePath(
	const FString& xyzFilePath,
	FString& outResolvedFilePath) const
{
	outResolvedFilePath = xyzFilePath.TrimStartAndEnd();
	if (outResolvedFilePath.IsEmpty())
	{
		UE_LOG(LogDeliveryBotPointCloudReview, Warning, TEXT("Point Cloud xyz path is empty."));
		return false;
	}

	outResolvedFilePath = FPaths::IsRelative(outResolvedFilePath)
		? FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), outResolvedFilePath)
		: FPaths::ConvertRelativePathToFull(outResolvedFilePath);
	FPaths::NormalizeFilename(outResolvedFilePath);

	if (!FPaths::CollapseRelativeDirectories(outResolvedFilePath))
	{
		UE_LOG(
			LogDeliveryBotPointCloudReview,
			Warning,
			TEXT("Point Cloud xyz path normalization failed: %s"),
			*xyzFilePath);
		return false;
	}

	if (!FPaths::GetExtension(outResolvedFilePath).Equals(TEXT("xyz"), ESearchCase::IgnoreCase))
	{
		UE_LOG(
			LogDeliveryBotPointCloudReview,
			Warning,
			TEXT("Point Cloud file must use the xyz extension: %s"),
			*outResolvedFilePath);
		return false;
	}

	if (!FPaths::FileExists(outResolvedFilePath))
	{
		UE_LOG(
			LogDeliveryBotPointCloudReview,
			Warning,
			TEXT("Point Cloud xyz file does not exist: %s"),
			*outResolvedFilePath);
		return false;
	}

	return true;
}

// 외부에서 받은 xyz 경로를 일반 point cloud 좌표로 로드한다.
bool ADeliveryBotPointCloudReviewActor::LoadPointCloudFromFile(const FString& xyzFilePath)
{
	FString resolvedFilePath;
	if (!TryResolveXyzFilePath(xyzFilePath, resolvedFilePath))
	{
		return false;
	}

	bUseMapLocalImportTransform = false;
	XyzFilePath.FilePath = MoveTemp(resolvedFilePath);
	return LoadPointCloudFile();
}

// map_accumulated.xyz의 map-local 좌표를 source world 좌표로 복원해서 로드한다.
bool ADeliveryBotPointCloudReviewActor::LoadReplayMapPointCloudFromFile(
	const FString& xyzFilePath,
	const FVector& captureOriginCm,
	const float importYAxisSign)
{
	FString resolvedFilePath;
	if (!TryResolveXyzFilePath(xyzFilePath, resolvedFilePath))
	{
		return false;
	}

	MapCaptureOriginCm = captureOriginCm;
	MapImportYAxisSign = FMath::IsNearlyZero(importYAxisSign)
		? -1.0f
		: importYAxisSign;
	bUseMapLocalImportTransform = true;
	CoordinateType = EDeliveryBotPointCloudCoordinateTypes::ActorLocal;

	XyzFilePath.FilePath = MoveTemp(resolvedFilePath);
	return LoadPointCloudFile();
}

// 현재 표시 중인 point instance와 메모리 point 목록을 지운다.
void ADeliveryBotPointCloudReviewActor::ClearPointCloud()
{
	LoadedPoints.Reset();
	PointCloudAsset = nullptr;

	if (PointCloudComponent != nullptr)
	{
		PointCloudComponent->SetPointCloud(nullptr);
	}

	ClearTopDownSphereInstances();
	ApplyReviewRenderMode();
}

// point cloud를 지운 뒤 같은 xyz 파일을 다시 읽는다.
bool ADeliveryBotPointCloudReviewActor::ReloadPointCloud()
{
	return LoadPointCloudFile();
}

// Point Cloud 데이터를 유지한 채 액터 렌더링만 전환한다.
void ADeliveryBotPointCloudReviewActor::SetPointCloudVisible(const bool bVisible)
{
	SetActorHiddenInGame(!bVisible);

	ApplyReviewRenderMode();
}

// Actor hidden 상태를 외부 Replay 제어에서 사용할 표시 상태로 변환한다.
bool ADeliveryBotPointCloudReviewActor::IsPointCloudVisible() const
{
	return !IsHidden();
}

// Sets the renderer used by replay review cameras.
void ADeliveryBotPointCloudReviewActor::SetReviewRenderMode(const EDeliveryBotPointCloudReviewRenderMode NewMode)
{
	if (ReviewRenderMode == NewMode)
	{
		return;
	}

	ReviewRenderMode = NewMode;
	ConfigurePointCloudRendering();
	ApplyReviewRenderMode();
}

// 현재 scenario 번호에 맞는 point cloud capture 폴더를 만든다.
FString ADeliveryBotPointCloudReviewActor::BuildScenarioCaptureDirectory() const
{
	const FString scenarioFolderName = FString::Printf(
		TEXT("scenario_%03d"),
		FMath::Max(0, ScenarioNumber));

	return FPaths::ConvertRelativePathToFull(FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("LidarPointCloudCaptures"),
		scenarioFolderName));
}

// run 폴더 안의 map_accumulated.xyz 전체 경로를 만든다.
FString ADeliveryBotPointCloudReviewActor::BuildMapAccumulatedFilePath(const FString& runDirectory) const
{
	return FPaths::Combine(
		runDirectory,
		TEXT("captures"),
		TEXT("lidar_point_cloud"),
		MapAccumulatedFileName);
}

// 현재 scenario 폴더에서 가장 최신 map_accumulated.xyz 경로를 찾는다.
bool ADeliveryBotPointCloudReviewActor::TryFindLatestMapAccumulatedFilePath(FString& outFilePath) const
{
	outFilePath.Reset();

	const FString scenarioDirectory = BuildScenarioCaptureDirectory();
	if (!FPaths::DirectoryExists(scenarioDirectory))
	{
		UE_LOG(
			LogDeliveryBotPointCloudReview,
			Warning,
			TEXT("Point cloud scenario directory does not exist: %s"),
			*scenarioDirectory);
		return false;
	}

	TArray<FString> runDirectoryNames;
	IFileManager::Get().FindFiles(
		runDirectoryNames,
		*FPaths::Combine(scenarioDirectory, TEXT("*")),
		false,
		true);

	FDateTime latestTimestamp = FDateTime::MinValue();
	FString latestFilePath;

	for (const FString& runDirectoryName : runDirectoryNames)
	{
		const FString runDirectory = FPaths::Combine(scenarioDirectory, runDirectoryName);
		const FString mapFilePath = BuildMapAccumulatedFilePath(runDirectory);
		if (!FPaths::FileExists(mapFilePath))
		{
			continue;
		}

		const FDateTime fileTimestamp = IFileManager::Get().GetTimeStamp(*mapFilePath);
		const bool bShouldUseFile =
			latestFilePath.IsEmpty()
			|| fileTimestamp > latestTimestamp
			|| (fileTimestamp == latestTimestamp && mapFilePath.Compare(latestFilePath) > 0);

		if (bShouldUseFile)
		{
			latestTimestamp = fileTimestamp;
			latestFilePath = mapFilePath;
		}
	}

	if (latestFilePath.IsEmpty())
	{
		UE_LOG(
			LogDeliveryBotPointCloudReview,
			Warning,
			TEXT("No map_accumulated.xyz found under scenario directory: %s"),
			*scenarioDirectory);
		return false;
	}

	outFilePath = latestFilePath;
	return true;
}

// 현재 scenario 폴더에서 가장 최신 map_accumulated.xyz를 찾아 로드한다.
bool ADeliveryBotPointCloudReviewActor::LoadLatestMapAccumulated()
{
	FString latestFilePath;
	if (!TryFindLatestMapAccumulatedFilePath(latestFilePath))
	{
		return false;
	}

	XyzFilePath.FilePath = latestFilePath;
	bUseMapLocalImportTransform = false;
	UE_LOG(
		LogDeliveryBotPointCloudReview,
		Log,
		TEXT("Loading latest map_accumulated.xyz: scenario=%d path=%s"),
		FMath::Max(0, ScenarioNumber),
		*latestFilePath);

	return LoadPointCloudFile();
}

// 에디터 Details 버튼에서 최신 map_accumulated.xyz 로드를 실행한다.
void ADeliveryBotPointCloudReviewActor::LoadLatestMapAccumulatedInEditor()
{
	LoadLatestMapAccumulated();
}

// xyz 파일의 한 줄을 point 정보로 변환한다.
bool ADeliveryBotPointCloudReviewActor::ParseXyzLine(
	const FString& line,
	FDeliveryBotPointCloudReviewPointInfo& outPoint) const
{
	FString trimmedLine = line;
	trimmedLine.TrimStartAndEndInline();

	if (trimmedLine.IsEmpty() || trimmedLine.StartsWith(TEXT("#")))
		return false;

	TArray<FString> tokens;
	trimmedLine.ParseIntoArrayWS(tokens);

	if (tokens.Num() < 3)
		return false;

	float x = 0.f;
	float y = 0.f;
	float z = 0.f;

	if (!LexTryParseString(x, *tokens[0])
		|| !LexTryParseString(y, *tokens[1])
		|| !LexTryParseString(z, *tokens[2]))
	{
		return false;
	}

	int32 red = 255;
	int32 green = 255;
	int32 blue = 255;

	if (tokens.Num() >= 6)
	{
		LexTryParseString(red, *tokens[3]);
		LexTryParseString(green, *tokens[4]);
		LexTryParseString(blue, *tokens[5]);
	}

	outPoint.X = x;
	outPoint.Y = y;
	outPoint.Z = z;
	outPoint.Color = FColor(
		static_cast<uint8>(FMath::Clamp(red, 0, 255)),
		static_cast<uint8>(FMath::Clamp(green, 0, 255)),
		static_cast<uint8>(FMath::Clamp(blue, 0, 255)));
	outPoint.Classification = ResolvePointClassificationFromColor(outPoint.Color);

	return true;
}

// map_accumulated.xyz의 map-local 좌표를 source world 좌표로 변환한다.
// Resolves the semantic class encoded in one xyz RGB color.
FName ADeliveryBotPointCloudReviewActor::ResolvePointClassificationFromColor(const FColor& Color) const
{
	if (Color == GroundPointColor)
	{
		return PointClassificationGround;
	}

	if (Color == WallPointColor)
	{
		return PointClassificationWall;
	}

	if (Color == ObstaclePointColor)
	{
		return PointClassificationObstacle;
	}

	if (Color == UnknownPointColor || Color == LegacyUnknownPointColor)
	{
		return PointClassificationUnknown;
	}

	return PointClassificationUnknown;
}

// Restores map-local xyz coordinates into source world coordinates.
FVector ADeliveryBotPointCloudReviewActor::ResolveReplayMapSourceWorldLocation(
	const FDeliveryBotPointCloudReviewPointInfo& point) const
{
	return FVector(
		MapCaptureOriginCm.X + point.X,
		MapCaptureOriginCm.Y + MapImportYAxisSign * point.Y,
		point.Z);
}

// point instance 렌더링에 사용할 좌표를 현재 import 모드에 맞춰 계산한다.
FVector ADeliveryBotPointCloudReviewActor::ResolvePointCloudLocalLocation(
	const FDeliveryBotPointCloudReviewPointInfo& point) const
{
	FVector SourceLocation(point.X, point.Y, point.Z);
	if (bUseMapLocalImportTransform)
	{
		SourceLocation = ResolveReplayMapSourceWorldLocation(point);
	}

	return CoordinateType == EDeliveryBotPointCloudCoordinateTypes::World
		? GetActorTransform().InverseTransformPosition(SourceLocation)
		: SourceLocation;
}

// coordinate mode에 맞춰 point를 instance transform으로 변환한다.
// coordinate mode에 맞춰 point의 실제 world 위치를 계산한다.
FVector ADeliveryBotPointCloudReviewActor::ResolvePointWorldLocation(
	const FDeliveryBotPointCloudReviewPointInfo& point) const
{
	return GetActorTransform().TransformPosition(ResolvePointCloudLocalLocation(point));
}

// 로드된 point 목록으로 instanced mesh와 선택적 debug overlay를 만든다.
bool ADeliveryBotPointCloudReviewActor::RebuildPointCloudAsset()
{
	if (PointCloudComponent == nullptr)
	{
		return false;
	}

	if (LoadedPoints.IsEmpty())
	{
		UE_LOG(LogDeliveryBotPointCloudReview, Warning, TEXT("No valid points were parsed from xyz file."));
		return false;
	}

	TArray<FLidarPointCloudPoint> LidarPoints;
	LidarPoints.Reserve(LoadedPoints.Num());

	for (const FDeliveryBotPointCloudReviewPointInfo& point : LoadedPoints)
	{
		const FVector LocalLocation = ResolvePointCloudLocalLocation(point);
		LidarPoints.Emplace(
			FVector3f(
				static_cast<float>(LocalLocation.X),
				static_cast<float>(LocalLocation.Y),
				static_cast<float>(LocalLocation.Z)),
			point.Color,
			true,
			0);
	}

	PointCloudAsset = ULidarPointCloud::CreateFromData(LidarPoints, false);
	if (!IsValid(PointCloudAsset))
	{
		UE_LOG(LogDeliveryBotPointCloudReview, Warning, TEXT("Failed to create Lidar Point Cloud asset."));
		return false;
	}

	PointCloudAsset->SetSourcePath(XyzFilePath.FilePath);
	PointCloudComponent->SetPointCloud(PointCloudAsset);
	ConfigurePointCloudRendering();
	BuildTopDownSphereInstances();
	ApplyReviewRenderMode();

	DrawDebugColorOverlay();
	return PointCloudAsset->GetNumPoints() > 0;
}

// 색상 확인용 DrawDebugPoint overlay를 그린다.
// Applies RGB, size, shape, and camera-mode scaling to plugin-rendered review points.
void ADeliveryBotPointCloudReviewActor::ConfigurePointCloudRendering()
{
	if (PointCloudComponent == nullptr)
	{
		return;
	}

	PointCloudComponent->PointSize = FMath::Max(PointSizeCm, 0.0f);
	PointCloudComponent->ColorSource = ELidarPointCloudColorationMode::Data;
	PointCloudComponent->PointOrientation = ELidarPointCloudSpriteOrientation::PreferFacingCamera;
	PointCloudComponent->ScalingMethod = ELidarPointCloudScalingMethod::PerNodeAdaptive;
	PointCloudComponent->GapFillingStrength = 0.0f;
	PointCloudComponent->SetPointShape(ELidarPointCloudSpriteShape::Circle);
	PointCloudComponent->MarkRenderStateDirty();
}

// Rebuilds the TopDown-only sphere instances from loaded point colors.
bool ADeliveryBotPointCloudReviewActor::BuildTopDownSphereInstances()
{
	ClearTopDownSphereInstances();

	if (LoadedPoints.IsEmpty())
	{
		return false;
	}

	if (TopDownSphereMesh == nullptr)
	{
		UE_LOG(LogDeliveryBotPointCloudReview, Warning, TEXT("TopDownSphereMesh is missing."));
		return false;
	}

	ConfigureTopDownSphereInstanceComponent(TopDownGroundPointInstances);
	ConfigureTopDownSphereInstanceComponent(TopDownWallPointInstances);
	ConfigureTopDownSphereInstanceComponent(TopDownObstaclePointInstances);
	ConfigureTopDownSphereInstanceComponent(TopDownUnknownPointInstances);
	ApplyTopDownSphereMaterials();

	int32 GroundCount = 0;
	int32 WallCount = 0;
	int32 ObstacleCount = 0;
	int32 UnknownCount = 0;
	for (const FDeliveryBotPointCloudReviewPointInfo& Point : LoadedPoints)
	{
		if (Point.Classification == PointClassificationGround)
		{
			++GroundCount;
		}
		else if (Point.Classification == PointClassificationWall)
		{
			++WallCount;
		}
		else if (Point.Classification == PointClassificationObstacle)
		{
			++ObstacleCount;
		}
		else
		{
			++UnknownCount;
		}
	}

	if (TopDownGroundPointInstances != nullptr)
	{
		TopDownGroundPointInstances->PreAllocateInstancesMemory(GroundCount);
	}
	if (TopDownWallPointInstances != nullptr)
	{
		TopDownWallPointInstances->PreAllocateInstancesMemory(WallCount);
	}
	if (TopDownObstaclePointInstances != nullptr)
	{
		TopDownObstaclePointInstances->PreAllocateInstancesMemory(ObstacleCount);
	}
	if (TopDownUnknownPointInstances != nullptr)
	{
		TopDownUnknownPointInstances->PreAllocateInstancesMemory(UnknownCount);
	}

	const float SphereScale = FMath::Max(TopDownSphereSizeCm, 0.1f) / 100.0f;
	int32 AddedPointCount = 0;
	for (const FDeliveryBotPointCloudReviewPointInfo& Point : LoadedPoints)
	{
		UHierarchicalInstancedStaticMeshComponent* Component =
			ResolveTopDownSphereComponentForClassification(Point.Classification);
		if (Component == nullptr)
		{
			continue;
		}

		const FVector LocalLocation =
			ResolvePointCloudLocalLocation(Point) + FVector(0.0, 0.0, TopDownSphereZOffsetCm);
		Component->AddInstance(
			FTransform(FRotator::ZeroRotator, LocalLocation, FVector(SphereScale)),
			false);
		++AddedPointCount;
	}

	if (TopDownGroundPointInstances != nullptr)
	{
		TopDownGroundPointInstances->MarkRenderStateDirty();
	}
	if (TopDownWallPointInstances != nullptr)
	{
		TopDownWallPointInstances->MarkRenderStateDirty();
	}
	if (TopDownObstaclePointInstances != nullptr)
	{
		TopDownObstaclePointInstances->MarkRenderStateDirty();
	}
	if (TopDownUnknownPointInstances != nullptr)
	{
		TopDownUnknownPointInstances->MarkRenderStateDirty();
	}

	return AddedPointCount > 0;
}

// Clears the TopDown-only sphere instances.
void ADeliveryBotPointCloudReviewActor::ClearTopDownSphereInstances()
{
	if (TopDownGroundPointInstances != nullptr)
	{
		TopDownGroundPointInstances->ClearInstances();
	}
	if (TopDownWallPointInstances != nullptr)
	{
		TopDownWallPointInstances->ClearInstances();
	}
	if (TopDownObstaclePointInstances != nullptr)
	{
		TopDownObstaclePointInstances->ClearInstances();
	}
	if (TopDownUnknownPointInstances != nullptr)
	{
		TopDownUnknownPointInstances->ClearInstances();
	}
}

// Applies common rendering settings to one TopDown sphere instance component.
void ADeliveryBotPointCloudReviewActor::ConfigureTopDownSphereInstanceComponent(
	UHierarchicalInstancedStaticMeshComponent* component) const
{
	if (component == nullptr)
	{
		return;
	}

	component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	component->SetGenerateOverlapEvents(false);
	component->SetMobility(EComponentMobility::Movable);
	component->SetCastShadow(false);
	component->SetVisibility(false, true);
	component->SetStaticMesh(TopDownSphereMesh);
}

// Creates or updates TopDown sphere materials for semantic point colors.
void ADeliveryBotPointCloudReviewActor::ApplyTopDownSphereMaterials()
{
	if (TopDownGroundPointInstances != nullptr)
	{
		TopDownGroundPointInstances->SetMaterial(
			0,
			GetOrCreateTopDownSphereMaterial(TopDownGroundPointMaterial, GroundPointColor));
	}
	if (TopDownWallPointInstances != nullptr)
	{
		TopDownWallPointInstances->SetMaterial(
			0,
			GetOrCreateTopDownSphereMaterial(TopDownWallPointMaterial, WallPointColor));
	}
	if (TopDownObstaclePointInstances != nullptr)
	{
		TopDownObstaclePointInstances->SetMaterial(
			0,
			GetOrCreateTopDownSphereMaterial(TopDownObstaclePointMaterial, ObstaclePointColor));
	}
	if (TopDownUnknownPointInstances != nullptr)
	{
		TopDownUnknownPointInstances->SetMaterial(
			0,
			GetOrCreateTopDownSphereMaterial(TopDownUnknownPointMaterial, UnknownPointColor));
	}
}

// Returns the TopDown sphere component that matches one parsed point classification.
UHierarchicalInstancedStaticMeshComponent* ADeliveryBotPointCloudReviewActor::ResolveTopDownSphereComponentForClassification(
	const FName& classification) const
{
	if (classification == PointClassificationGround)
	{
		return TopDownGroundPointInstances;
	}

	if (classification == PointClassificationWall)
	{
		return TopDownWallPointInstances;
	}

	if (classification == PointClassificationObstacle)
	{
		return TopDownObstaclePointInstances;
	}

	return TopDownUnknownPointInstances;
}

// Creates or updates one dynamic TopDown sphere material.
UMaterialInstanceDynamic* ADeliveryBotPointCloudReviewActor::GetOrCreateTopDownSphereMaterial(
	TObjectPtr<UMaterialInstanceDynamic>& materialSlot,
	const FColor& color)
{
	if (TopDownSphereBaseMaterial == nullptr)
	{
		return nullptr;
	}

	if (!IsValid(materialSlot))
	{
		materialSlot = UMaterialInstanceDynamic::Create(TopDownSphereBaseMaterial, this);
	}

	const FLinearColor LinearColor(color);
	materialSlot->SetVectorParameterValue(MaterialColorParameterName, LinearColor);
	materialSlot->SetVectorParameterValue(MaterialBaseColorParameterName, LinearColor);
	materialSlot->SetVectorParameterValue(MaterialTintColorParameterName, LinearColor);
	return materialSlot;
}

// Applies the active review render mode to owned render components.
void ADeliveryBotPointCloudReviewActor::ApplyReviewRenderMode()
{
	const bool bVisible = IsPointCloudVisible();
	const bool bShowPlugin3D =
		bVisible && ReviewRenderMode == EDeliveryBotPointCloudReviewRenderMode::Plugin3D;
	const bool bShowTopDownSpheres =
		bVisible && ReviewRenderMode == EDeliveryBotPointCloudReviewRenderMode::TopDownProjection;

	if (PointCloudComponent != nullptr)
	{
		PointCloudComponent->SetVisibility(bShowPlugin3D, true);
	}

	if (TopDownGroundPointInstances != nullptr)
	{
		TopDownGroundPointInstances->SetVisibility(bShowTopDownSpheres, true);
	}
	if (TopDownWallPointInstances != nullptr)
	{
		TopDownWallPointInstances->SetVisibility(bShowTopDownSpheres, true);
	}
	if (TopDownObstaclePointInstances != nullptr)
	{
		TopDownObstaclePointInstances->SetVisibility(bShowTopDownSpheres, true);
	}
	if (TopDownUnknownPointInstances != nullptr)
	{
		TopDownUnknownPointInstances->SetVisibility(bShowTopDownSpheres, true);
	}
}

void ADeliveryBotPointCloudReviewActor::DrawDebugColorOverlay() const
{
	if (!bDrawDebugColorOverlay)
		return;

	UWorld* world = GetWorld();
	if (world == nullptr)
		return;

	const float lifeTimeSeconds = FMath::Max(DebugOverlayLifeTimeSeconds, 0.1f);
	const float pointSize = FMath::Max(DebugOverlayPointSize, 1.f);

	for (const FDeliveryBotPointCloudReviewPointInfo& point : LoadedPoints)
	{
		DrawDebugPoint(
			world,
			ResolvePointWorldLocation(point),
			pointSize,
			point.Color,
			false,
			lifeTimeSeconds);
	}
}
