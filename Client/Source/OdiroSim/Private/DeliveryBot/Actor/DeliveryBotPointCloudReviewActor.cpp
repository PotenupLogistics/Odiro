#include "DeliveryBot/Actor/DeliveryBotPointCloudReviewActor.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeliveryBotPointCloudReview, Log, All);

namespace
{
	const TCHAR* MapAccumulatedFileName = TEXT("map_accumulated.xyz");
}

ADeliveryBotPointCloudReviewActor::ADeliveryBotPointCloudReviewActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	PointInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("PointInstances"));
	PointInstances->SetupAttachment(SceneRoot);
	PointInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PointInstances->SetMobility(EComponentMobility::Movable);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> sphereMesh(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (sphereMesh.Succeeded())
	{
		PointMesh = sphereMesh.Object;
		PointInstances->SetStaticMesh(PointMesh);
	}
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

	if (PointInstances == nullptr)
	{
		UE_LOG(LogDeliveryBotPointCloudReview, Warning, TEXT("PointInstances component is missing."));
		return false;
	}

	if (PointMesh == nullptr)
	{
		UE_LOG(LogDeliveryBotPointCloudReview, Warning, TEXT("PointMesh is missing."));
		return false;
	}

	PointInstances->SetStaticMesh(PointMesh);

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

	RebuildPointInstances();

	UE_LOG(
		LogDeliveryBotPointCloudReview,
		Log,
		TEXT("Loaded xyz point cloud: path=%s pointCount=%d coordinateType=%s"),
		*filePath,
		LoadedPoints.Num(),
		CoordinateType == EDeliveryBotPointCloudCoordinateTypes::World ? TEXT("World") : TEXT("ActorLocal"));

	return LoadedPoints.Num() > 0;
}

// 외부에서 받은 xyz 경로를 정규화하고 검증한 뒤 기존 로드 흐름에 연결한다.
bool ADeliveryBotPointCloudReviewActor::LoadPointCloudFromFile(const FString& xyzFilePath)
{
	FString resolvedFilePath = xyzFilePath.TrimStartAndEnd();
	if (resolvedFilePath.IsEmpty())
	{
		UE_LOG(LogDeliveryBotPointCloudReview, Warning, TEXT("Point Cloud xyz path is empty."));
		return false;
	}

	resolvedFilePath = FPaths::IsRelative(resolvedFilePath)
		? FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), resolvedFilePath)
		: FPaths::ConvertRelativePathToFull(resolvedFilePath);
	FPaths::NormalizeFilename(resolvedFilePath);

	if (!FPaths::CollapseRelativeDirectories(resolvedFilePath))
	{
		UE_LOG(
			LogDeliveryBotPointCloudReview,
			Warning,
			TEXT("Point Cloud xyz path normalization failed: %s"),
			*xyzFilePath);
		return false;
	}

	if (!FPaths::GetExtension(resolvedFilePath).Equals(TEXT("xyz"), ESearchCase::IgnoreCase))
	{
		UE_LOG(
			LogDeliveryBotPointCloudReview,
			Warning,
			TEXT("Point Cloud file must use the xyz extension: %s"),
			*resolvedFilePath);
		return false;
	}

	if (!FPaths::FileExists(resolvedFilePath))
	{
		UE_LOG(
			LogDeliveryBotPointCloudReview,
			Warning,
			TEXT("Point Cloud xyz file does not exist: %s"),
			*resolvedFilePath);
		return false;
	}

	XyzFilePath.FilePath = MoveTemp(resolvedFilePath);
	return LoadPointCloudFile();
}

// 현재 표시 중인 point instance와 메모리 point 목록을 지운다.
void ADeliveryBotPointCloudReviewActor::ClearPointCloud()
{
	LoadedPoints.Reset();

	if (PointInstances != nullptr)
	{
		PointInstances->ClearInstances();
	}
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
}

// Actor hidden 상태를 외부 Replay 제어에서 사용할 표시 상태로 변환한다.
bool ADeliveryBotPointCloudReviewActor::IsPointCloudVisible() const
{
	return !IsHidden();
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
	UE_LOG(
		LogDeliveryBotPointCloudReview,
		Log,
		TEXT("Loading latest map_accumulated.xyz: scenario=%d path=%s"),
		FMath::Max(0, ScenarioNumber),
		*latestFilePath);

	return LoadPointCloudFile();
}

// xyz 파일의 한 줄을 point 정보로 변환한다.
// 에디터 Details 버튼에서 최신 map_accumulated.xyz 로드를 실행한다.
void ADeliveryBotPointCloudReviewActor::LoadLatestMapAccumulatedInEditor()
{
	LoadLatestMapAccumulated();
}

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

	return true;
}

// coordinate mode에 맞춰 point를 instance transform으로 변환한다.
FTransform ADeliveryBotPointCloudReviewActor::MakePointInstanceTransform(
	const FDeliveryBotPointCloudReviewPointInfo& point) const
{
	const float pointScale = FMath::Max(PointSizeCm, 0.1f) / 100.f;
	return FTransform(
		FRotator::ZeroRotator,
		FVector(point.X, point.Y, point.Z),
		FVector(pointScale));
}

// coordinate mode에 맞춰 point의 실제 world 위치를 계산한다.
FVector ADeliveryBotPointCloudReviewActor::ResolvePointWorldLocation(
	const FDeliveryBotPointCloudReviewPointInfo& point) const
{
	const FVector pointLocation(point.X, point.Y, point.Z);

	if (CoordinateType == EDeliveryBotPointCloudCoordinateTypes::World)
	{
		return pointLocation;
	}

	return GetActorTransform().TransformPosition(pointLocation);
}

// 로드된 point 목록으로 instanced mesh와 선택적 debug overlay를 만든다.
void ADeliveryBotPointCloudReviewActor::RebuildPointInstances()
{
	if (PointInstances == nullptr)
		return;

	PointInstances->ClearInstances();
	PointInstances->PreAllocateInstancesMemory(LoadedPoints.Num());

	const bool bWorldSpace = CoordinateType == EDeliveryBotPointCloudCoordinateTypes::World;
	for (const FDeliveryBotPointCloudReviewPointInfo& point : LoadedPoints)
	{
		PointInstances->AddInstance(MakePointInstanceTransform(point), bWorldSpace);
	}

	PointInstances->MarkRenderStateDirty();
	DrawDebugColorOverlay();
}

// 색상 확인용 DrawDebugPoint overlay를 그린다.
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
