#include "Scenario/ScenarioCityBlockMaterializer.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Scenario/ScenarioCorridorGeometry.h"

DEFINE_LOG_CATEGORY_STATIC(LogScenarioCityBlockMaterializer, Log, All);

namespace
{
	// Generated-city GroundRegion ids are the only regions eligible for CityBuildings visual blocks.
	const FString GeneratedCityRegionIdPrefix(TEXT("generated_city_"));
	// Surface ids match the reduced authoring/runtime vocabulary.
	const FName CityBlockBuildingSurfaceId(TEXT("building"));
	const FName CityBlockRoadSurfaceId(TEXT("road"));
	const FName CityBlockWalkwaySurfaceId(TEXT("walkway"));
	// Generated curb GroundRegions use this collision tag while remaining road-surface metadata.
	const FString CurbCollisionTag(TEXT("curb"));
	// Generated road GroundRegions use this penalty kind for the two-lane road band.
	const FString RoadPenaltyKind(TEXT("road"));
	// Maximum absolute direction dot product accepted as a generated right-angle corner.
	const double GeneratedCornerRightAngleDotTolerance = 0.05;
	// Maximum distance from a seam-line intersection to each segment endpoint when inferring a corner.
	const double GeneratedCornerJoinToleranceCm = 500.0;
	// Straight road-side visual span reserved on each side of an authored corner asset.
	const double GeneratedCornerStraightReserveGapCm = 500.0;

	// Generated city band markers encode the side needed for corridor-relative edge anchoring.
	const TCHAR* GeneratedLowerSideMarkers[] =
	{
		TEXT("_lower_walkway_extension_"),
		TEXT("_lower_building_"),
		TEXT("_lower_curb_"),
		TEXT("_lower_road_2lane_")
	};

	// Generated city band markers encode the side needed for corridor-relative edge anchoring.
	const TCHAR* GeneratedUpperSideMarkers[] =
	{
		TEXT("_upper_walkway_extension_"),
		TEXT("_upper_building_"),
		TEXT("_upper_curb_"),
		TEXT("_upper_road_2lane_")
	};

	// Continuous seam geometry inferred from one generated road-side curb GroundRegion.
	struct FGeneratedRoadCurbSeam
	{
		// Region id used for gap reservations and debug output.
		FString RegionId;

		// Normalized generated side label, currently lower or upper.
		FString SideLabel;

		// Direction along the source corridor segment in world centimeters.
		FVector Forward = FVector::ForwardVector;

		// Inner walkway-curb seam start point in world centimeters.
		FVector SeamStartCm = FVector::ZeroVector;

		// Inner walkway-curb seam end point in world centimeters.
		FVector SeamEndCm = FVector::ZeroVector;

		// Source region yaw in degrees, reused as the corner actor's first-segment orientation.
		double YawDegrees = 0.0;
	};

	// Reserved local-X span near a road-side corner for straight visual blocks.
	struct FGeneratedRegionStraightGapCm
	{
		// Gap from the negative local-X endpoint of the generated region.
		double StartGapCm = 0.0;

		// Gap from the positive local-X endpoint of the generated region.
		double EndGapCm = 0.0;
	};

	// Visual-only corner actor placement inferred from adjacent generated curb seams.
	struct FGeneratedRoadSideCornerPlacement
	{
		// Actor-origin anchor location in world centimeters, usually the continuous walkway-curb seam intersection.
		FVector AnchorLocationCm = FVector::ZeroVector;

		// Actor yaw in degrees derived from the segment that reaches the inferred corner.
		double YawDegrees = 0.0;

		// Stable diagnostic key used to avoid duplicate corner spawns at the same seam intersection.
		FString DebugKey;
	};

	// Oriented 2D footprint used to keep generated building frontages from overlapping at corridor corners.
	struct FGeneratedBuildingFootprint
	{
		// Authored bounds center in world centimeters.
		FVector CenterCm = FVector::ZeroVector;

		// Normalized local length axis projected into the XY plane.
		FVector Forward = FVector::ForwardVector;

		// Normalized local depth axis projected into the XY plane, pointing away from the corridor.
		FVector Outward = FVector::RightVector;

		// Half of the authored building frontage length in centimeters.
		double HalfLengthCm = 0.0;

		// Half of the authored building depth in centimeters.
		double HalfWidthCm = 0.0;

		// Stable source label used for overlap diagnostics.
		FString DebugSourceId;
	};

	// Filters the materializer to generated straight city padding without touching authored GroundRegions.
	bool IsGeneratedCityVisualRegion(const FScenarioGroundRegionSpec& regionSpec)
	{
		return regionSpec.ShapeType == EScenarioGroundShapeType::Rectangle
			&& regionSpec.RegionId.StartsWith(GeneratedCityRegionIdPrefix);
	}

	// Detects the generated curb band used as the road-side composite placement trigger.
	bool IsGeneratedRoadCurbRegion(const FScenarioGroundRegionSpec& regionSpec)
	{
		const FName surfaceId(*regionSpec.SurfaceId);
		return IsGeneratedCityVisualRegion(regionSpec)
			&& surfaceId == CityBlockRoadSurfaceId
			&& regionSpec.RegionType == EScenarioGroundRegionType::Blocked
			&& (regionSpec.CollisionTag.Equals(CurbCollisionTag, ESearchCase::IgnoreCase)
				|| regionSpec.RegionId.Contains(TEXT("_curb_")));
	}

	// Detects the generated road band that should be covered by a curb-triggered composite block.
	bool IsGeneratedRoadPenaltyRegion(const FScenarioGroundRegionSpec& regionSpec)
	{
		const FName surfaceId(*regionSpec.SurfaceId);
		return IsGeneratedCityVisualRegion(regionSpec)
			&& surfaceId == CityBlockRoadSurfaceId
			&& regionSpec.RegionType == EScenarioGroundRegionType::Penalty
			&& (regionSpec.PenaltyKind.Equals(RoadPenaltyKind, ESearchCase::IgnoreCase)
				|| regionSpec.RegionId.Contains(TEXT("_road_2lane_")));
	}

	// Detects generated building-side walkway extensions without treating them as road-side composite triggers.
	bool IsGeneratedWalkwayExtensionRegion(const FScenarioGroundRegionSpec& regionSpec)
	{
		const FName surfaceId(*regionSpec.SurfaceId);
		return IsGeneratedCityVisualRegion(regionSpec)
			&& surfaceId == CityBlockWalkwaySurfaceId
			&& regionSpec.RegionType == EScenarioGroundRegionType::Walkable
			&& regionSpec.RegionId.Contains(TEXT("_walkway_extension_"));
	}

	// Detects generated building frontage bands that must be anchored away from the Corridor.
	bool IsGeneratedBuildingRegion(const FScenarioGroundRegionSpec& regionSpec)
	{
		const FName surfaceId(*regionSpec.SurfaceId);
		return IsGeneratedCityVisualRegion(regionSpec)
			&& surfaceId == CityBlockBuildingSurfaceId
			&& regionSpec.RegionType == EScenarioGroundRegionType::Blocked
			&& regionSpec.RegionId.Contains(TEXT("_building_"));
	}

	// Returns a normalized XY-plane axis for overlap tests.
	FVector Normalize2DAxis(const FVector& axis)
	{
		return FVector(axis.X, axis.Y, 0.0).GetSafeNormal();
	}

	// Projects two XY-plane vectors onto each other.
	double Dot2D(const FVector& lhs, const FVector& rhs)
	{
		return (lhs.X * rhs.X) + (lhs.Y * rhs.Y);
	}

	// Computes the interval radius for one building footprint on a separating-axis candidate.
	double ProjectBuildingFootprintRadiusCm(
		const FGeneratedBuildingFootprint& footprint,
		const FVector& axis)
	{
		return (FMath::Abs(Dot2D(footprint.Forward, axis)) * footprint.HalfLengthCm)
			+ (FMath::Abs(Dot2D(footprint.Outward, axis)) * footprint.HalfWidthCm);
	}

	// Tests authored building bounds footprints for strict overlap while allowing edge-touching neighbors.
	bool DoBuildingFootprintsOverlap2D(
		const FGeneratedBuildingFootprint& lhs,
		const FGeneratedBuildingFootprint& rhs)
	{
		const FVector axisCandidates[] =
		{
			lhs.Forward,
			lhs.Outward,
			rhs.Forward,
			rhs.Outward
		};
		const FVector centerDelta = rhs.CenterCm - lhs.CenterCm;
		constexpr double EdgeTouchToleranceCm = 0.1;

		for (const FVector& axisCandidate : axisCandidates)
		{
			const FVector axis = Normalize2DAxis(axisCandidate);
			if (axis.IsNearlyZero())
			{
				continue;
			}

			const double separationCm = FMath::Abs(Dot2D(centerDelta, axis));
			const double radiusSumCm =
				ProjectBuildingFootprintRadiusCm(lhs, axis)
				+ ProjectBuildingFootprintRadiusCm(rhs, axis);
			if (separationCm >= radiusSumCm - EdgeTouchToleranceCm)
			{
				return false;
			}
		}

		return true;
	}

	// Maps source-of-truth GroundRegion semantics to the straight block roles supported by the catalog.
	void ResolveCityBlockRolesForRegion(
		const FScenarioGroundRegionSpec& regionSpec,
		TArray<EScenarioCityBlockRole>& outRoles)
	{
		outRoles.Reset();
		const FName surfaceId(*regionSpec.SurfaceId);
		if (IsGeneratedRoadCurbRegion(regionSpec))
		{
			outRoles.Add(EScenarioCityBlockRole::WalkwayRoadStraight);
			return;
		}

		if (IsGeneratedWalkwayExtensionRegion(regionSpec))
		{
			outRoles.Add(EScenarioCityBlockRole::WalkwayBuildingStraight);
			return;
		}

		if (surfaceId == CityBlockRoadSurfaceId && regionSpec.RegionType == EScenarioGroundRegionType::Penalty)
		{
			outRoles.Add(EScenarioCityBlockRole::RoadStraight);
			return;
		}

		if (surfaceId == CityBlockBuildingSurfaceId && regionSpec.RegionType == EScenarioGroundRegionType::Blocked)
		{
			outRoles.Add(EScenarioCityBlockRole::Building);
		}
	}

	// Checks whether a catalog entry explicitly names the GroundRegion surface.
	bool DoesCityBlockEntryNameSurface(
		const FScenarioCityBlockCatalogEntry& blockEntry,
		FName surfaceId)
	{
		return !surfaceId.IsNone() && blockEntry.SemanticProfile.SurfaceIds.Contains(surfaceId);
	}

	// Checks whether a catalog entry is allowed to represent the source GroundRegion surface.
	bool IsCityBlockEntrySurfaceCompatible(
		const FScenarioCityBlockCatalogEntry& blockEntry,
		FName surfaceId)
	{
		return blockEntry.SemanticProfile.SurfaceIds.IsEmpty()
			|| DoesCityBlockEntryNameSurface(blockEntry, surfaceId);
	}

	// Checks optional semantic detail fields only when they are relevant to the source GroundRegion type.
	bool IsCityBlockEntryDetailCompatible(
		const FScenarioCityBlockCatalogEntry& blockEntry,
		const FScenarioGroundRegionSpec& regionSpec)
	{
		if (regionSpec.RegionType == EScenarioGroundRegionType::Blocked
			&& !blockEntry.SemanticProfile.CollisionTag.IsEmpty())
		{
			return blockEntry.SemanticProfile.CollisionTag.Equals(
				regionSpec.CollisionTag,
				ESearchCase::IgnoreCase);
		}

		if (regionSpec.RegionType == EScenarioGroundRegionType::Penalty
			&& !blockEntry.SemanticProfile.PenaltyKind.IsEmpty())
		{
			return blockEntry.SemanticProfile.PenaltyKind.Equals(
				regionSpec.PenaltyKind,
				ESearchCase::IgnoreCase);
		}

		return true;
	}

	// Checks whether optional semantic detail fields explicitly match the source GroundRegion.
	bool DoesCityBlockEntryDetailMatch(
		const FScenarioCityBlockCatalogEntry& blockEntry,
		const FScenarioGroundRegionSpec& regionSpec)
	{
		if (regionSpec.RegionType == EScenarioGroundRegionType::Blocked)
		{
			return !blockEntry.SemanticProfile.CollisionTag.IsEmpty()
				&& blockEntry.SemanticProfile.CollisionTag.Equals(
					regionSpec.CollisionTag,
					ESearchCase::IgnoreCase);
		}

		if (regionSpec.RegionType == EScenarioGroundRegionType::Penalty)
		{
			return !blockEntry.SemanticProfile.PenaltyKind.IsEmpty()
				&& blockEntry.SemanticProfile.PenaltyKind.Equals(
					regionSpec.PenaltyKind,
					ESearchCase::IgnoreCase);
		}

		return false;
	}

	// Builds a side-strip key shared by the curb and road bands in one generated layout chunk.
	bool TryMakeGeneratedRoadSideCompositeKey(
		const FString& regionId,
		FString& outCompositeKey)
	{
		struct FGeneratedSideMarker
		{
			// Stable id fragment embedded between generated segment id and layout/chunk suffix.
			const TCHAR* Marker = TEXT("");

			// Normalized lower/upper side label used in coverage keys.
			const TCHAR* Side = TEXT("");
		};

		const FGeneratedSideMarker sideMarkers[] =
		{
			{ TEXT("_lower_curb_"), TEXT("lower") },
			{ TEXT("_lower_road_2lane_"), TEXT("lower") },
			{ TEXT("_upper_curb_"), TEXT("upper") },
			{ TEXT("_upper_road_2lane_"), TEXT("upper") }
		};

		for (const FGeneratedSideMarker& sideMarker : sideMarkers)
		{
			const FString marker(sideMarker.Marker);
			const int32 markerIndex = regionId.Find(marker, ESearchCase::CaseSensitive);
			if (markerIndex == INDEX_NONE)
			{
				continue;
			}

			const FString segmentPrefix = regionId.Left(markerIndex);
			const FString layoutChunkSuffix = regionId.Mid(markerIndex + marker.Len());
			if (segmentPrefix.IsEmpty() || layoutChunkSuffix.IsEmpty())
			{
				continue;
			}

			outCompositeKey = FString::Printf(
				TEXT("%s|%s|%s"),
				*segmentPrefix,
				sideMarker.Side,
				*layoutChunkSuffix);
			return true;
		}

		outCompositeKey.Reset();
		return false;
	}

	// Resolves generated lower/upper side into a normalized label.
	bool TryResolveGeneratedCitySideLabel(const FString& regionId, FString& outSideLabel)
	{
		for (const TCHAR* marker : GeneratedLowerSideMarkers)
		{
			if (regionId.Contains(marker))
			{
				outSideLabel = TEXT("lower");
				return true;
			}
		}

		for (const TCHAR* marker : GeneratedUpperSideMarkers)
		{
			if (regionId.Contains(marker))
			{
				outSideLabel = TEXT("upper");
				return true;
			}
		}

		outSideLabel.Reset();
		return false;
	}

	// Resolves generated lower/upper side into a sign along the region's local right vector.
	bool TryResolveGeneratedCitySideSign(const FString& regionId, double& outSideSign)
	{
		for (const TCHAR* marker : GeneratedLowerSideMarkers)
		{
			if (regionId.Contains(marker))
			{
				outSideSign = -1.0;
				return true;
			}
		}

		for (const TCHAR* marker : GeneratedUpperSideMarkers)
		{
			if (regionId.Contains(marker))
			{
				outSideSign = 1.0;
				return true;
			}
		}

		outSideSign = 0.0;
		return false;
	}

	// Scores one catalog entry while preserving the role/surface/type fallback order.
	int32 ScoreCityBlockEntryForRegion(
		const FScenarioCityBlockCatalogEntry& blockEntry,
		const FScenarioGroundRegionSpec& regionSpec,
		FName surfaceId)
	{
		int32 matchTier = 1;
		if (DoesCityBlockEntryNameSurface(blockEntry, surfaceId))
		{
			matchTier = 2;
			if (blockEntry.SemanticProfile.PrimaryRegionType == regionSpec.RegionType)
			{
				matchTier = 3;
				if (DoesCityBlockEntryDetailMatch(blockEntry, regionSpec))
				{
					matchTier = 4;
				}
			}
		}

		return (matchTier * 10000) + blockEntry.PlacementProfile.Priority;
	}

	// Selects the most specific catalog entry for one generated GroundRegion.
	bool FindCityBlockEntryForRegion(
		const UScenarioCityBlockCatalog& catalog,
		const FScenarioGroundRegionSpec& regionSpec,
		FScenarioCityBlockCatalogEntry& outBlockEntry)
	{
		outBlockEntry = FScenarioCityBlockCatalogEntry();
		TArray<EScenarioCityBlockRole> roles;
		ResolveCityBlockRolesForRegion(regionSpec, roles);
		if (roles.IsEmpty())
		{
			return false;
		}

		const FName surfaceId(*regionSpec.SurfaceId);
		const FScenarioCityBlockCatalogEntry* bestEntry = nullptr;
		int32 bestScore = MIN_int32;
		for (const FScenarioCityBlockCatalogEntry& blockEntry : catalog.GetEntries())
		{
			if (!roles.Contains(blockEntry.Role))
			{
				continue;
			}

			if (!IsCityBlockEntrySurfaceCompatible(blockEntry, surfaceId)
				|| !IsCityBlockEntryDetailCompatible(blockEntry, regionSpec))
			{
				continue;
			}

			if (IsGeneratedRoadCurbRegion(regionSpec)
				&& blockEntry.SemanticProfile.PrimaryRegionType != regionSpec.RegionType)
			{
				continue;
			}

			const int32 score = ScoreCityBlockEntryForRegion(blockEntry, regionSpec, surfaceId);
			if (!bestEntry || score > bestScore)
			{
				bestEntry = &blockEntry;
				bestScore = score;
			}
		}

		if (bestEntry)
		{
			outBlockEntry = *bestEntry;
			return true;
		}

		return false;
	}

	// Collects building frontage candidates ordered by priority, then by larger authored frontage length.
	void FindCityBlockEntriesForBuildingRegion(
		const UScenarioCityBlockCatalog& catalog,
		const FScenarioGroundRegionSpec& regionSpec,
		TArray<FScenarioCityBlockCatalogEntry>& outBlockEntries)
	{
		outBlockEntries.Reset();
		const FName surfaceId(*regionSpec.SurfaceId);
		for (const FScenarioCityBlockCatalogEntry& blockEntry : catalog.GetEntries())
		{
			if (blockEntry.Role != EScenarioCityBlockRole::Building)
			{
				continue;
			}

			if (!IsCityBlockEntrySurfaceCompatible(blockEntry, surfaceId)
				|| !IsCityBlockEntryDetailCompatible(blockEntry, regionSpec))
			{
				continue;
			}

			outBlockEntries.Add(blockEntry);
		}

		outBlockEntries.Sort([](
			const FScenarioCityBlockCatalogEntry& lhs,
			const FScenarioCityBlockCatalogEntry& rhs)
		{
			if (lhs.PlacementProfile.Priority != rhs.PlacementProfile.Priority)
			{
				return lhs.PlacementProfile.Priority > rhs.PlacementProfile.Priority;
			}

			if (!FMath::IsNearlyEqual(lhs.BoundsMeters.LengthMeters, rhs.BoundsMeters.LengthMeters))
			{
				return lhs.BoundsMeters.LengthMeters > rhs.BoundsMeters.LengthMeters;
			}

			return lhs.BlockId.ToString() < rhs.BlockId.ToString();
		});
	}

	// Selects the preferred visual entry for an inferred road-side corner.
	bool FindCityBlockEntryForRoadSideCorner(
		const UScenarioCityBlockCatalog& catalog,
		FScenarioCityBlockCatalogEntry& outBlockEntry)
	{
		outBlockEntry = FScenarioCityBlockCatalogEntry();
		const EScenarioCityBlockRole preferredRoles[] =
		{
			EScenarioCityBlockRole::Corner,
			EScenarioCityBlockRole::Crossroad
		};

		for (EScenarioCityBlockRole preferredRole : preferredRoles)
		{
			const FScenarioCityBlockCatalogEntry* bestEntry = nullptr;
			int32 bestPriority = MIN_int32;
			for (const FScenarioCityBlockCatalogEntry& blockEntry : catalog.GetEntries())
			{
				if (blockEntry.Role != preferredRole)
				{
					continue;
				}

				if (!blockEntry.SemanticProfile.SurfaceIds.IsEmpty()
					&& !blockEntry.SemanticProfile.SurfaceIds.Contains(CityBlockRoadSurfaceId)
					&& !blockEntry.SemanticProfile.SurfaceIds.Contains(CityBlockWalkwaySurfaceId))
				{
					continue;
				}

				if (!bestEntry || blockEntry.PlacementProfile.Priority > bestPriority)
				{
					bestEntry = &blockEntry;
					bestPriority = blockEntry.PlacementProfile.Priority;
				}
			}

			if (bestEntry)
			{
				outBlockEntry = *bestEntry;
				return true;
			}
		}

		return false;
	}

	// Returns the 2D cross product used for seam-line intersection.
	double Cross2D(const FVector& lhs, const FVector& rhs)
	{
		return (lhs.X * rhs.Y) - (lhs.Y * rhs.X);
	}

	// Intersects two non-parallel infinite 2D lines expressed in world centimeters.
	bool TryIntersectLines2D(
		const FVector& linePointA,
		const FVector& lineDirectionA,
		const FVector& linePointB,
		const FVector& lineDirectionB,
		FVector& outIntersectionCm)
	{
		const double denominator = Cross2D(lineDirectionA, lineDirectionB);
		if (FMath::IsNearlyZero(denominator, KINDA_SMALL_NUMBER))
		{
			outIntersectionCm = FVector::ZeroVector;
			return false;
		}

		const FVector delta = linePointB - linePointA;
		const double distanceAlongA = Cross2D(delta, lineDirectionB) / denominator;
		outIntersectionCm = linePointA + (lineDirectionA * distanceAlongA);
		outIntersectionCm.Z = FScenarioCorridorGeometry::DefaultSurfaceTopZCm;
		return true;
	}

	// Creates the inner seam line represented by one generated road-side curb band.
	bool TryBuildGeneratedRoadCurbSeam(
		const FScenarioGroundRegionSpec& regionSpec,
		FGeneratedRoadCurbSeam& outSeam)
	{
		outSeam = FGeneratedRoadCurbSeam();
		if (!IsGeneratedRoadCurbRegion(regionSpec)
			|| regionSpec.Size.X <= KINDA_SMALL_NUMBER
			|| regionSpec.Size.Y <= KINDA_SMALL_NUMBER)
		{
			return false;
		}

		double sideSign = 0.0;
		FString sideLabel;
		if (!TryResolveGeneratedCitySideSign(regionSpec.RegionId, sideSign)
			|| !TryResolveGeneratedCitySideLabel(regionSpec.RegionId, sideLabel))
		{
			return false;
		}

		const FRotator regionRotation(0.0, regionSpec.YawDegrees, 0.0);
		const FVector forward = regionRotation.RotateVector(FVector::ForwardVector);
		const FVector right = regionRotation.RotateVector(FVector::RightVector);
		FVector seamCenterCm = regionSpec.Center - (right * sideSign * regionSpec.Size.Y * 0.5);
		seamCenterCm.Z = FScenarioCorridorGeometry::DefaultSurfaceTopZCm;

		outSeam.RegionId = regionSpec.RegionId;
		outSeam.SideLabel = MoveTemp(sideLabel);
		outSeam.Forward = forward;
		outSeam.SeamStartCm = seamCenterCm - (forward * regionSpec.Size.X * 0.5);
		outSeam.SeamEndCm = seamCenterCm + (forward * regionSpec.Size.X * 0.5);
		outSeam.YawDegrees = regionSpec.YawDegrees;
		return true;
	}

	// Quantizes a generated corner anchor so adjacent seam pairs do not spawn duplicate corner actors.
	FString MakeGeneratedCornerDebugKey(
		const FString& sideLabel,
		const FVector& anchorLocationCm)
	{
		return FString::Printf(
			TEXT("%s|%d|%d"),
			*sideLabel,
			FMath::RoundToInt(anchorLocationCm.X),
			FMath::RoundToInt(anchorLocationCm.Y));
	}

	// Returns the nearest distance from a candidate corner anchor to either endpoint of one seam segment.
	double GetNearestSeamEndpointDistanceCm(
		const FGeneratedRoadCurbSeam& seam,
		const FVector& anchorLocationCm)
	{
		return FMath::Min(
			FVector::Dist2D(anchorLocationCm, seam.SeamStartCm),
			FVector::Dist2D(anchorLocationCm, seam.SeamEndCm));
	}

	// Resolves the actor yaw from the seam endpoint that reaches the inferred corner anchor.
	double ResolveCornerYawDegrees(
		const FGeneratedRoadCurbSeam& seam,
		const FVector& anchorLocationCm)
	{
		const double startDistanceCm = FVector::Dist2D(anchorLocationCm, seam.SeamStartCm);
		const double endDistanceCm = FVector::Dist2D(anchorLocationCm, seam.SeamEndCm);
		return startDistanceCm <= endDistanceCm
			? FMath::UnwindDegrees(seam.YawDegrees + 180.0)
			: seam.YawDegrees;
	}

	// Applies a straight-block corner reservation to whichever endpoint is closest to the inferred anchor.
	void ApplyCornerGapForSeam(
		const FGeneratedRoadCurbSeam& seam,
		const FVector& anchorLocationCm,
		TMap<FString, FGeneratedRegionStraightGapCm>& inOutStraightGapsCm)
	{
		const double startDistanceCm = FVector::Dist2D(anchorLocationCm, seam.SeamStartCm);
		const double endDistanceCm = FVector::Dist2D(anchorLocationCm, seam.SeamEndCm);
		FGeneratedRegionStraightGapCm& regionGapCm = inOutStraightGapsCm.FindOrAdd(seam.RegionId);
		FGeneratedRegionStraightGapCm* compositeGapCm = nullptr;

		FString compositeKey;
		if (TryMakeGeneratedRoadSideCompositeKey(seam.RegionId, compositeKey))
		{
			compositeGapCm = &inOutStraightGapsCm.FindOrAdd(compositeKey);
		}

		if (startDistanceCm <= endDistanceCm)
		{
			regionGapCm.StartGapCm = FMath::Max(regionGapCm.StartGapCm, GeneratedCornerStraightReserveGapCm);
			if (compositeGapCm)
			{
				compositeGapCm->StartGapCm = FMath::Max(
					compositeGapCm->StartGapCm,
					GeneratedCornerStraightReserveGapCm);
			}
			return;
		}

		regionGapCm.EndGapCm = FMath::Max(regionGapCm.EndGapCm, GeneratedCornerStraightReserveGapCm);
		if (compositeGapCm)
		{
			compositeGapCm->EndGapCm = FMath::Max(
				compositeGapCm->EndGapCm,
				GeneratedCornerStraightReserveGapCm);
		}
	}

	// Resolves a corner reservation for either the curb trigger region or the road band sharing its side-strip key.
	const FGeneratedRegionStraightGapCm* FindCornerGapForGeneratedRoadSideRegion(
		const FScenarioGroundRegionSpec& regionSpec,
		const TMap<FString, FGeneratedRegionStraightGapCm>& straightGapsCm)
	{
		if (const FGeneratedRegionStraightGapCm* regionGapCm = straightGapsCm.Find(regionSpec.RegionId))
		{
			return regionGapCm;
		}

		FString compositeKey;
		if (TryMakeGeneratedRoadSideCompositeKey(regionSpec.RegionId, compositeKey))
		{
			return straightGapsCm.Find(compositeKey);
		}

		return nullptr;
	}

	// Infers right-angle road-side corner anchors from adjacent generated curb seam lines.
	void BuildRoadSideCornerPlacements(
		const TArray<FScenarioGroundRegionSpec>& groundRegions,
		TArray<FGeneratedRoadSideCornerPlacement>& outCornerPlacements,
		TMap<FString, FGeneratedRegionStraightGapCm>& outStraightGapsCm)
	{
		outCornerPlacements.Reset();
		outStraightGapsCm.Reset();

		TArray<FGeneratedRoadCurbSeam> seams;
		for (const FScenarioGroundRegionSpec& regionSpec : groundRegions)
		{
			FGeneratedRoadCurbSeam seam;
			if (TryBuildGeneratedRoadCurbSeam(regionSpec, seam))
			{
				seams.Add(MoveTemp(seam));
			}
		}

		TSet<FString> spawnedCornerKeys;
		for (int32 firstIndex = 0; firstIndex < seams.Num(); ++firstIndex)
		{
			for (int32 secondIndex = firstIndex + 1; secondIndex < seams.Num(); ++secondIndex)
			{
				const FGeneratedRoadCurbSeam& firstSeam = seams[firstIndex];
				const FGeneratedRoadCurbSeam& secondSeam = seams[secondIndex];
				if (firstSeam.SideLabel != secondSeam.SideLabel)
				{
					continue;
				}

				const double directionDot = FMath::Abs(
					FVector::DotProduct(firstSeam.Forward.GetSafeNormal2D(), secondSeam.Forward.GetSafeNormal2D()));
				if (directionDot > GeneratedCornerRightAngleDotTolerance)
				{
					continue;
				}

				FVector anchorLocationCm;
				if (!TryIntersectLines2D(
					firstSeam.SeamStartCm,
					firstSeam.Forward,
					secondSeam.SeamStartCm,
					secondSeam.Forward,
					anchorLocationCm))
				{
					continue;
				}

				if (GetNearestSeamEndpointDistanceCm(firstSeam, anchorLocationCm) > GeneratedCornerJoinToleranceCm
					|| GetNearestSeamEndpointDistanceCm(secondSeam, anchorLocationCm) > GeneratedCornerJoinToleranceCm)
				{
					continue;
				}

				const FString cornerKey = MakeGeneratedCornerDebugKey(firstSeam.SideLabel, anchorLocationCm);
				if (spawnedCornerKeys.Contains(cornerKey))
				{
					continue;
				}

				spawnedCornerKeys.Add(cornerKey);
				FGeneratedRoadSideCornerPlacement cornerPlacement;
				cornerPlacement.AnchorLocationCm = anchorLocationCm;
				cornerPlacement.YawDegrees = ResolveCornerYawDegrees(firstSeam, anchorLocationCm);
				cornerPlacement.DebugKey = cornerKey;
				outCornerPlacements.Add(MoveTemp(cornerPlacement));

				ApplyCornerGapForSeam(firstSeam, anchorLocationCm, outStraightGapsCm);
				ApplyCornerGapForSeam(secondSeam, anchorLocationCm, outStraightGapsCm);
			}
		}
	}

	// Finds road-side chunks whose curb band will spawn a composite visual block.
	void BuildRoadSideCompositeCoverageKeys(
		const UScenarioCityBlockCatalog& catalog,
		const TArray<FScenarioGroundRegionSpec>& groundRegions,
		TSet<FString>& outCompositeKeys)
	{
		outCompositeKeys.Reset();
		for (const FScenarioGroundRegionSpec& regionSpec : groundRegions)
		{
			if (!IsGeneratedRoadCurbRegion(regionSpec))
			{
				continue;
			}

			FScenarioCityBlockCatalogEntry blockEntry;
			if (!FindCityBlockEntryForRegion(catalog, regionSpec, blockEntry)
				|| blockEntry.Role != EScenarioCityBlockRole::WalkwayRoadStraight)
			{
				continue;
			}

			FString compositeKey;
			if (TryMakeGeneratedRoadSideCompositeKey(regionSpec.RegionId, compositeKey))
			{
				outCompositeKeys.Add(compositeKey);
			}
		}
	}

	// Checks whether a road band should be skipped because its curb band owns the composite visual.
	bool IsRoadBandCoveredByComposite(
		const FScenarioGroundRegionSpec& regionSpec,
		const TSet<FString>& compositeKeys)
	{
		if (!IsGeneratedRoadPenaltyRegion(regionSpec))
		{
			return false;
		}

		FString compositeKey;
		return TryMakeGeneratedRoadSideCompositeKey(regionSpec.RegionId, compositeKey)
			&& compositeKeys.Contains(compositeKey);
	}

	// Resolves the authored surface height that should receive the visual block origin.
	double ResolveCityBlockSurfaceTopZCm(const FScenarioGroundRegionSpec& regionSpec)
	{
		const FName surfaceId(*regionSpec.SurfaceId);
		if (surfaceId == CityBlockRoadSurfaceId)
		{
			return 0.0;
		}

		if (surfaceId == CityBlockWalkwaySurfaceId || surfaceId == CityBlockBuildingSurfaceId)
		{
			return FScenarioCorridorGeometry::DefaultSurfaceTopZCm;
		}

		return regionSpec.Center.Z;
	}

	// Computes the desired authored bounds center for center- and continuous-edge anchored visual blocks.
	FVector ResolveDesiredBlockBoundsCenter(
		const FScenarioGroundRegionSpec& regionSpec,
		const FScenarioCityBlockCatalogEntry& blockEntry,
		const FRotator& blockRotation,
		const FVector& forward,
		double alongOffsetCm)
	{
		FVector baseRegionCenter = regionSpec.Center + (forward * alongOffsetCm);
		baseRegionCenter.Z = ResolveCityBlockSurfaceTopZCm(regionSpec);
		const EScenarioCityBlockLateralAnchor lateralAnchor =
			blockEntry.PlacementProfile.LateralAnchor;
		if (lateralAnchor == EScenarioCityBlockLateralAnchor::RegionCenter)
		{
			double sideSign = 0.0;
			if (TryResolveGeneratedCitySideSign(regionSpec.RegionId, sideSign))
			{
				const FVector right = blockRotation.RotateVector(FVector::RightVector);
				return baseRegionCenter
					+ (right * sideSign * blockEntry.PlacementProfile.LateralOffsetMeters * 100.0);
			}
			return baseRegionCenter;
		}

		double sideSign = 0.0;
		if (!TryResolveGeneratedCitySideSign(regionSpec.RegionId, sideSign))
		{
			return baseRegionCenter;
		}

		const FVector right = blockRotation.RotateVector(FVector::RightVector);
		const double regionHalfWidthCm = regionSpec.Size.Y * 0.5;
		const double blockHalfWidthCm = blockEntry.BoundsMeters.WidthMeters * 50.0;
		const double postAnchorOffsetCm = blockEntry.PlacementProfile.LateralOffsetMeters * 100.0;

		if (lateralAnchor == EScenarioCityBlockLateralAnchor::RegionInnerEdge)
		{
			// For generated curb bands, this inner edge is the continuous walkway-curb seam.
			return baseRegionCenter
				+ (right * sideSign * (blockHalfWidthCm - regionHalfWidthCm + postAnchorOffsetCm));
		}

		// For generated road bands, this outer edge is the continuous far-side road boundary.
		return baseRegionCenter
			+ (right * sideSign * (regionHalfWidthCm - blockHalfWidthCm + postAnchorOffsetCm));
	}

	// Disables every collision path on visual-only CityBuildings actors.
	void DisableCityBlockActorCollision(AActor& blockActor)
	{
		blockActor.SetActorEnableCollision(false);

		TArray<UPrimitiveComponent*> primitiveComponents;
		blockActor.GetComponents<UPrimitiveComponent>(primitiveComponents);
		for (UPrimitiveComponent* primitiveComponent : primitiveComponents)
		{
			if (!primitiveComponent)
			{
				continue;
			}

			primitiveComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			primitiveComponent->SetGenerateOverlapEvents(false);
		}
	}

	// Prevents generated visual blocks from receiving projected runtime/editor decals.
	void SetActorReceivesDecals(AActor& blockActor, bool bReceivesDecals)
	{
		TArray<UPrimitiveComponent*> primitiveComponents;
		blockActor.GetComponents<UPrimitiveComponent>(primitiveComponents);
		for (UPrimitiveComponent* primitiveComponent : primitiveComponents)
		{
			if (primitiveComponent)
			{
				primitiveComponent->SetReceivesDecals(bReceivesDecals);
			}
		}
	}

	// Loads and validates the configured actor class for one catalog entry.
	UClass* LoadCityBlockActorClass(
		const FScenarioCityBlockCatalogEntry& blockEntry,
		const FString& debugSourceId,
		const FScenarioCityBlockMaterializationOptions& options)
	{
		UClass* blockClass = blockEntry.BPClass.LoadSynchronous();
		if (!IsValid(blockClass) || !blockClass->IsChildOf(AActor::StaticClass()))
		{
			UE_LOG(
				LogScenarioCityBlockMaterializer,
				Warning,
				TEXT("%s generated city block '%s' skipped for '%s' because BPClass is empty or not an Actor class."),
				*options.LogContext,
				*blockEntry.BlockId.ToString(),
				*debugSourceId);
			return nullptr;
		}

		return blockClass;
	}

	// Spawns one visual-only block actor whose authored bounds center has already been resolved.
	AActor* SpawnCityBlockActorAtBoundsCenter(
		UWorld& world,
		UClass& blockClass,
		const FScenarioCityBlockCatalogEntry& blockEntry,
		const FRotator& blockRotation,
		const FVector& desiredBoundsCenter,
		const FString& debugSourceId,
		TArray<TObjectPtr<AActor>>& outSpawnedActors,
		const FScenarioCityBlockMaterializationOptions& options)
	{
		const FVector boundsCenterOffsetCm =
			blockRotation.RotateVector(blockEntry.BoundsMeters.CenterOffsetMeters * 100.0);
		const FVector spawnLocation = desiredBoundsCenter - boundsCenterOffsetCm;

		FActorSpawnParameters spawnParams;
		spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AActor* blockActor = world.SpawnActor<AActor>(
			&blockClass,
			FTransform(blockRotation, spawnLocation),
			spawnParams);
		if (!blockActor)
		{
			UE_LOG(
				LogScenarioCityBlockMaterializer,
				Warning,
				TEXT("%s generated city block '%s' failed to spawn for '%s'."),
				*options.LogContext,
				*blockEntry.BlockId.ToString(),
				*debugSourceId);
			return nullptr;
		}

		DisableCityBlockActorCollision(*blockActor);
		SetActorReceivesDecals(*blockActor, false);
		outSpawnedActors.Add(blockActor);
		return blockActor;
	}

	// Spawns building frontage blocks using authored bounds rather than fixed 10m modular spacing.
	int32 SpawnBuildingFrontageVisualsForRegion(
		UWorld& world,
		const FScenarioGroundRegionSpec& regionSpec,
		const TArray<FScenarioCityBlockCatalogEntry>& blockEntries,
		TArray<FGeneratedBuildingFootprint>& inOutAcceptedFootprints,
		TArray<TObjectPtr<AActor>>& outSpawnedActors,
		int32& outSkippedOverlapCount,
		const FScenarioCityBlockMaterializationOptions& options)
	{
		outSkippedOverlapCount = 0;
		double sideSign = 0.0;
		if (!TryResolveGeneratedCitySideSign(regionSpec.RegionId, sideSign))
		{
			UE_LOG(
				LogScenarioCityBlockMaterializer,
				Warning,
				TEXT("%s generated building frontage skipped for region '%s' because the generated side is unknown."),
				*options.LogContext,
				*regionSpec.RegionId);
			return 0;
		}

		const FScenarioCityBlockCatalogEntry* selectedBlockEntry = nullptr;
		UClass* selectedBlockClass = nullptr;
		for (const FScenarioCityBlockCatalogEntry& blockEntry : blockEntries)
		{
			const double blockLengthCm = blockEntry.BoundsMeters.LengthMeters * 100.0;
			const double blockWidthCm = blockEntry.BoundsMeters.WidthMeters * 100.0;
			if (blockLengthCm <= KINDA_SMALL_NUMBER || blockWidthCm <= KINDA_SMALL_NUMBER)
			{
				UE_LOG(
					LogScenarioCityBlockMaterializer,
					Warning,
					TEXT("%s generated building block '%s' skipped for region '%s' because bounds length or width is invalid."),
					*options.LogContext,
					*blockEntry.BlockId.ToString(),
					*regionSpec.RegionId);
				continue;
			}

			if (blockEntry.PlacementProfile.LateralOffsetMeters < -KINDA_SMALL_NUMBER)
			{
				UE_LOG(
					LogScenarioCityBlockMaterializer,
					Warning,
					TEXT("%s generated building block '%s' skipped for region '%s' because negative lateral offset would cross the building inner edge."),
					*options.LogContext,
					*blockEntry.BlockId.ToString(),
					*regionSpec.RegionId);
				continue;
			}

			if (blockLengthCm > regionSpec.Size.X + KINDA_SMALL_NUMBER)
			{
				continue;
			}

			UClass* blockClass = LoadCityBlockActorClass(blockEntry, regionSpec.RegionId, options);
			if (!blockClass)
			{
				continue;
			}

			selectedBlockEntry = &blockEntry;
			selectedBlockClass = blockClass;
			break;
		}

		if (!selectedBlockEntry || !selectedBlockClass)
		{
			UE_LOG(
				LogScenarioCityBlockMaterializer,
				Warning,
				TEXT("%s generated building frontage skipped for region '%s' because no building block fits the region length %.2f cm."),
				*options.LogContext,
				*regionSpec.RegionId,
				regionSpec.Size.X);
			return 0;
		}

		const double blockLengthCm = selectedBlockEntry->BoundsMeters.LengthMeters * 100.0;
		const double blockHalfWidthCm = selectedBlockEntry->BoundsMeters.WidthMeters * 50.0;
		const int32 blockCount = FMath::FloorToInt((regionSpec.Size.X + KINDA_SMALL_NUMBER) / blockLengthCm);
		if (blockCount <= 0)
		{
			return 0;
		}

		const FRotator blockRotation(0.0, regionSpec.YawDegrees, 0.0);
		const FVector forward = Normalize2DAxis(blockRotation.RotateVector(FVector::ForwardVector));
		const FVector outward = Normalize2DAxis(blockRotation.RotateVector(FVector::RightVector) * sideSign);
		FVector innerEdgeCenterCm =
			regionSpec.Center - (outward * regionSpec.Size.Y * 0.5);
		innerEdgeCenterCm.Z = ResolveCityBlockSurfaceTopZCm(regionSpec);

		const double usedLengthCm = static_cast<double>(blockCount) * blockLengthCm;
		const double centeredUnusedLengthCm = (regionSpec.Size.X - usedLengthCm) * 0.5;
		const double firstAlongOffsetCm =
			(-regionSpec.Size.X * 0.5) + centeredUnusedLengthCm + (blockLengthCm * 0.5);
		const double outwardOffsetCm =
			blockHalfWidthCm + (selectedBlockEntry->PlacementProfile.LateralOffsetMeters * 100.0);

		int32 spawnedActorCount = 0;
		for (int32 blockIndex = 0; blockIndex < blockCount; ++blockIndex)
		{
			const double alongOffsetCm =
				firstAlongOffsetCm + (static_cast<double>(blockIndex) * blockLengthCm);
			const FVector desiredBoundsCenter =
				innerEdgeCenterCm
				+ (forward * alongOffsetCm)
				+ (outward * outwardOffsetCm);

			FGeneratedBuildingFootprint candidateFootprint;
			candidateFootprint.CenterCm = desiredBoundsCenter;
			candidateFootprint.Forward = forward;
			candidateFootprint.Outward = outward;
			candidateFootprint.HalfLengthCm = blockLengthCm * 0.5;
			candidateFootprint.HalfWidthCm = blockHalfWidthCm;
			candidateFootprint.DebugSourceId = FString::Printf(
				TEXT("%s#%d"),
				*regionSpec.RegionId,
				blockIndex);

			bool bOverlapsAcceptedFootprint = false;
			for (const FGeneratedBuildingFootprint& acceptedFootprint : inOutAcceptedFootprints)
			{
				if (DoBuildingFootprintsOverlap2D(candidateFootprint, acceptedFootprint))
				{
					bOverlapsAcceptedFootprint = true;
					UE_LOG(
						LogScenarioCityBlockMaterializer,
						Verbose,
						TEXT("%s generated building block '%s' skipped for '%s' because it overlaps accepted footprint '%s'."),
						*options.LogContext,
						*selectedBlockEntry->BlockId.ToString(),
						*candidateFootprint.DebugSourceId,
						*acceptedFootprint.DebugSourceId);
					break;
				}
			}

			if (bOverlapsAcceptedFootprint)
			{
				++outSkippedOverlapCount;
				continue;
			}

			if (SpawnCityBlockActorAtBoundsCenter(
				world,
				*selectedBlockClass,
				*selectedBlockEntry,
				blockRotation,
				desiredBoundsCenter,
				regionSpec.RegionId,
				outSpawnedActors,
				options))
			{
				inOutAcceptedFootprints.Add(candidateFootprint);
				++spawnedActorCount;
			}
		}

		return spawnedActorCount;
	}

	// Spawns repeated visual-only blocks along one generated rectangular GroundRegion.
	int32 SpawnCityBlockVisualsForRegion(
		UWorld& world,
		const FScenarioGroundRegionSpec& regionSpec,
		const FScenarioCityBlockCatalogEntry& blockEntry,
		const FGeneratedRegionStraightGapCm* straightGapCm,
		TArray<TObjectPtr<AActor>>& outSpawnedActors,
		const FScenarioCityBlockMaterializationOptions& options)
	{
		UClass* blockClass = LoadCityBlockActorClass(blockEntry, regionSpec.RegionId, options);
		if (!blockClass)
		{
			return 0;
		}

		const double blockLengthCm = blockEntry.BoundsMeters.LengthMeters * 100.0;
		if (blockLengthCm <= KINDA_SMALL_NUMBER || regionSpec.Size.X <= KINDA_SMALL_NUMBER)
		{
			UE_LOG(
				LogScenarioCityBlockMaterializer,
				Warning,
				TEXT("%s generated city block '%s' skipped for region '%s' because block or region length is invalid."),
				*options.LogContext,
				*blockEntry.BlockId.ToString(),
				*regionSpec.RegionId);
			return 0;
		}

		int32 blockCount = 0;
		double firstAlongOffsetCm = 0.0;
		const bool bHasReservedGap = straightGapCm
			&& (straightGapCm->StartGapCm > KINDA_SMALL_NUMBER
				|| straightGapCm->EndGapCm > KINDA_SMALL_NUMBER);
		if (bHasReservedGap)
		{
			const double usableStartCm =
				(-regionSpec.Size.X * 0.5) + FMath::Max(0.0, straightGapCm->StartGapCm);
			const double usableEndCm =
				(regionSpec.Size.X * 0.5) - FMath::Max(0.0, straightGapCm->EndGapCm);
			const double usableLengthCm = usableEndCm - usableStartCm;
			if (usableLengthCm + KINDA_SMALL_NUMBER < blockLengthCm)
			{
				return 0;
			}

			blockCount = FMath::Max(1, FMath::FloorToInt((usableLengthCm + KINDA_SMALL_NUMBER) / blockLengthCm));
			const double usedLengthCm = static_cast<double>(blockCount) * blockLengthCm;
			const double centeredUnusedLengthCm = (usableLengthCm - usedLengthCm) * 0.5;
			firstAlongOffsetCm = usableStartCm + centeredUnusedLengthCm + (blockLengthCm * 0.5);
		}
		else
		{
			blockCount = FMath::Max(1, FMath::CeilToInt(regionSpec.Size.X / blockLengthCm));
			const double chainLengthCm = static_cast<double>(blockCount - 1) * blockLengthCm;
			firstAlongOffsetCm = chainLengthCm * -0.5;
		}

		const FRotator blockRotation(0.0, regionSpec.YawDegrees, 0.0);
		const FVector forward = blockRotation.RotateVector(FVector::ForwardVector);

		int32 spawnedActorCount = 0;
		for (int32 blockIndex = 0; blockIndex < blockCount; ++blockIndex)
		{
			const double alongOffsetCm = firstAlongOffsetCm + (static_cast<double>(blockIndex) * blockLengthCm);
			const FVector desiredBoundsCenter = ResolveDesiredBlockBoundsCenter(
				regionSpec,
				blockEntry,
				blockRotation,
				forward,
				alongOffsetCm);
			if (SpawnCityBlockActorAtBoundsCenter(
				world,
				*blockClass,
				blockEntry,
				blockRotation,
				desiredBoundsCenter,
				regionSpec.RegionId,
				outSpawnedActors,
				options))
			{
				++spawnedActorCount;
			}
		}

		return spawnedActorCount;
	}

	// Spawns visual-only corner blocks at inferred road-side seam anchors.
	int32 SpawnRoadSideCornerVisuals(
		UWorld& world,
		const FScenarioCityBlockCatalogEntry& blockEntry,
		const TArray<FGeneratedRoadSideCornerPlacement>& cornerPlacements,
		TArray<TObjectPtr<AActor>>& outSpawnedActors,
		const FScenarioCityBlockMaterializationOptions& options)
	{
		UClass* blockClass = LoadCityBlockActorClass(
			blockEntry,
			TEXT("road-side-corner"),
			options);
		if (!blockClass)
		{
			return 0;
		}

		int32 spawnedActorCount = 0;
		for (const FGeneratedRoadSideCornerPlacement& cornerPlacement : cornerPlacements)
		{
			const FRotator blockRotation(0.0, cornerPlacement.YawDegrees, 0.0);
			const FVector desiredBoundsCenter =
				cornerPlacement.AnchorLocationCm
				+ blockRotation.RotateVector(blockEntry.BoundsMeters.CenterOffsetMeters * 100.0);
			if (SpawnCityBlockActorAtBoundsCenter(
				world,
				*blockClass,
				blockEntry,
				blockRotation,
				desiredBoundsCenter,
				cornerPlacement.DebugKey,
				outSpawnedActors,
				options))
			{
				++spawnedActorCount;
			}
		}

		return spawnedActorCount;
	}
}

FScenarioCityBlockMaterializationResult FScenarioCityBlockMaterializer::SpawnGeneratedCityBlocks(
	UWorld* world,
	const UScenarioCityBlockCatalog* catalog,
	const TArray<FScenarioGroundRegionSpec>& groundRegions,
	TArray<TObjectPtr<AActor>>& outSpawnedActors,
	const FScenarioCityBlockMaterializationOptions& options)
{
	FScenarioCityBlockMaterializationResult result;
	if (!world)
	{
		UE_LOG(
			LogScenarioCityBlockMaterializer,
			Warning,
			TEXT("%s generated city block visuals skipped because world is unavailable."),
			*options.LogContext);
		return result;
	}

	if (!IsValid(catalog))
	{
		UE_LOG(
			LogScenarioCityBlockMaterializer,
			Warning,
			TEXT("%s scenario city block catalog is not configured or failed to load: %s"),
			*options.LogContext,
			*options.CatalogDebugName);
		return result;
	}

	TSet<FString> roadSideCompositeKeys;
	BuildRoadSideCompositeCoverageKeys(*catalog, groundRegions, roadSideCompositeKeys);

	TArray<FGeneratedRoadSideCornerPlacement> roadSideCornerPlacements;
	TMap<FString, FGeneratedRegionStraightGapCm> roadSideCornerGapsCm;
	BuildRoadSideCornerPlacements(groundRegions, roadSideCornerPlacements, roadSideCornerGapsCm);
	result.CornerCandidateCount = roadSideCornerPlacements.Num();

	FScenarioCityBlockCatalogEntry roadSideCornerBlockEntry;
	const bool bHasRoadSideCornerEntry = roadSideCornerPlacements.IsEmpty()
		? false
		: FindCityBlockEntryForRoadSideCorner(*catalog, roadSideCornerBlockEntry);
	const TMap<FString, FGeneratedRegionStraightGapCm>* activeRoadSideCornerGapsCm =
		bHasRoadSideCornerEntry ? &roadSideCornerGapsCm : nullptr;
	TArray<FGeneratedBuildingFootprint> acceptedBuildingFootprints;

	for (const FScenarioGroundRegionSpec& regionSpec : groundRegions)
	{
		TArray<EScenarioCityBlockRole> roles;
		ResolveCityBlockRolesForRegion(regionSpec, roles);
		if (!IsGeneratedCityVisualRegion(regionSpec) || roles.IsEmpty())
		{
			continue;
		}

		++result.CandidateRegionCount;
		if (IsRoadBandCoveredByComposite(regionSpec, roadSideCompositeKeys))
		{
			++result.SkippedCoveredByCompositeCount;
			continue;
		}

		if (IsGeneratedBuildingRegion(regionSpec))
		{
			TArray<FScenarioCityBlockCatalogEntry> buildingBlockEntries;
			FindCityBlockEntriesForBuildingRegion(*catalog, regionSpec, buildingBlockEntries);
			if (buildingBlockEntries.IsEmpty())
			{
				++result.SkippedNoEntryCount;
				continue;
			}

			const int32 beforeSpawnCount = outSpawnedActors.Num();
			int32 skippedOverlapCount = 0;
			result.SpawnedActorCount += SpawnBuildingFrontageVisualsForRegion(
				*world,
				regionSpec,
				buildingBlockEntries,
				acceptedBuildingFootprints,
				outSpawnedActors,
				skippedOverlapCount,
				options);
			result.SkippedBuildingOverlapCount += skippedOverlapCount;
			if (outSpawnedActors.Num() == beforeSpawnCount && skippedOverlapCount == 0)
			{
				++result.SkippedSpawnFailureCount;
			}
			continue;
		}

		FScenarioCityBlockCatalogEntry blockEntry;
		if (!FindCityBlockEntryForRegion(*catalog, regionSpec, blockEntry))
		{
			++result.SkippedNoEntryCount;
			continue;
		}

		const int32 beforeSpawnCount = outSpawnedActors.Num();
		const FGeneratedRegionStraightGapCm* straightGapCm = activeRoadSideCornerGapsCm
			? FindCornerGapForGeneratedRoadSideRegion(regionSpec, *activeRoadSideCornerGapsCm)
			: nullptr;
		result.SpawnedActorCount += SpawnCityBlockVisualsForRegion(
			*world,
			regionSpec,
			blockEntry,
			straightGapCm,
			outSpawnedActors,
			options);
		if (outSpawnedActors.Num() == beforeSpawnCount)
		{
			++result.SkippedSpawnFailureCount;
		}
	}

	if (!roadSideCornerPlacements.IsEmpty())
	{
		if (!bHasRoadSideCornerEntry)
		{
			result.SkippedCornerNoEntryCount = roadSideCornerPlacements.Num();
		}
		else
		{
			const int32 spawnedCornerCount = SpawnRoadSideCornerVisuals(
				*world,
				roadSideCornerBlockEntry,
				roadSideCornerPlacements,
				outSpawnedActors,
				options);
			result.SpawnedActorCount += spawnedCornerCount;
			result.SkippedCornerSpawnFailureCount =
				roadSideCornerPlacements.Num() - spawnedCornerCount;
		}
	}

	if (result.CandidateRegionCount > 0
		|| result.SpawnedActorCount > 0
		|| result.CornerCandidateCount > 0
		|| result.SkippedNoEntryCount > 0
		|| result.SkippedCornerNoEntryCount > 0
		|| result.SkippedSpawnFailureCount > 0
		|| result.SkippedCornerSpawnFailureCount > 0
		|| result.SkippedCoveredByCompositeCount > 0
		|| result.SkippedBuildingOverlapCount > 0)
	{
		UE_LOG(
			LogScenarioCityBlockMaterializer,
			Log,
			TEXT(
				"%s generated city block visuals complete | "
				"CandidateRegions: %d, SpawnedActors: %d, CornerCandidates: %d, "
				"SkippedNoEntry: %d, SkippedCornerNoEntry: %d, SkippedSpawnFailure: %d, "
				"SkippedCornerSpawnFailure: %d, SkippedCoveredByComposite: %d, "
				"SkippedBuildingOverlap: %d"),
			*options.LogContext,
			result.CandidateRegionCount,
			result.SpawnedActorCount,
			result.CornerCandidateCount,
			result.SkippedNoEntryCount,
			result.SkippedCornerNoEntryCount,
			result.SkippedSpawnFailureCount,
			result.SkippedCornerSpawnFailureCount,
			result.SkippedCoveredByCompositeCount,
			result.SkippedBuildingOverlapCount);
	}

	return result;
}

void FScenarioCityBlockMaterializer::DestroySpawnedActors(TArray<TObjectPtr<AActor>>& spawnedActors)
{
	for (int32 index = spawnedActors.Num() - 1; index >= 0; --index)
	{
		if (AActor* actor = spawnedActors[index].Get())
		{
			actor->Destroy();
		}
	}
	spawnedActors.Reset();
}
