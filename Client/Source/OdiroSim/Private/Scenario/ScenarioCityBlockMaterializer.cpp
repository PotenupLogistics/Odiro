#include "Scenario/ScenarioCityBlockMaterializer.h"

#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/ShapeComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
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
	// Fallback straight road-side span reserved when a corner asset has no authored length.
	const double GeneratedCornerStraightReserveGapCm = 500.0;
	// RoadStraight chunks whose endpoints are this close are treated as one continuous spline strip.
	const double GeneratedRoadStraightSplineJoinToleranceCm = 750.0;
	// Projection tolerance for selecting the corridor-facing edge of a generated polygon expansion.
	const double GeneratedBuildingFrontageCorridorEdgeProjectionToleranceCm = 50.0;
	// Runtime grid only needs a low semantic volume that covers the robot footprint height.
	const double GeneratedBuildingCollisionProxyHeightCm = 200.0;
	// Hidden building proxies use the existing grid rule for blocked areas.
	const FName GeneratedBuildingCollisionProxyProfileName(TEXT("Blocked"));
	// Component name prefix used to identify authored-bounds building collision proxies.
	const FName GeneratedBuildingCollisionProxyComponentName(TEXT("GeneratedBuildingBlockingBounds"));
	// Authored Building BP component name reserved for low navigation blocker volumes.
	const FString BuildingGridNavBlockerComponentName(TEXT("GridNavBlocker"));
	// Clearance reserved only between generated building frontage actors.
	const double GeneratedBuildingInterBlockGapCm = 100.0;

	// Generated city band markers encode the side needed for corridor-relative edge anchoring.
	const TCHAR* GeneratedLowerSideMarkers[] =
	{
		TEXT("_lower_walkway_extension_"),
		TEXT("_lower_building_expansion_"),
		TEXT("_lower_building_"),
		TEXT("_lower_curb_"),
		TEXT("_lower_road_2lane_")
	};

	// Generated city band markers encode the side needed for corridor-relative edge anchoring.
	const TCHAR* GeneratedUpperSideMarkers[] =
	{
		TEXT("_upper_walkway_extension_"),
		TEXT("_upper_building_expansion_"),
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

	// Signed local-X endpoint offsets near a road-side corner for straight visual blocks.
	struct FGeneratedRegionStraightGapCm
	{
		// Whether the negative local-X endpoint has an explicit corner offset.
		bool bHasStartGap = false;

		// Offset from the negative local-X endpoint; negative values extend beyond the generated region.
		double StartGapCm = 0.0;

		// Whether the positive local-X endpoint has an explicit corner offset.
		bool bHasEndGap = false;

		// Offset from the positive local-X endpoint; negative values extend beyond the generated region.
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

	// One stretched RoadStraight span that will be materialized as a spline mesh section.
	struct FGeneratedRoadStraightSplinePiece
	{
		// Original road GroundRegion id used in diagnostics.
		FString RegionId;

		// Road visual group key, usually the generated lower/upper side plus selected block identity.
		FString ChainKey;

		// Selected catalog entry that supplies the RoadStraight visual template.
		FScenarioCityBlockCatalogEntry BlockEntry;

		// Loaded actor class used only as a static mesh/material template.
		UClass* BlockClass = nullptr;

		// Start of the road visual centerline in world centimeters.
		FVector StartCm = FVector::ZeroVector;

		// End of the road visual centerline in world centimeters.
		FVector EndCm = FVector::ZeroVector;

		// True when the start endpoint was trimmed to leave room for an authored corner/crossroad mesh.
		bool bStartHasCornerGap = false;

		// True when the end endpoint was trimmed to leave room for an authored corner/crossroad mesh.
		bool bEndHasCornerGap = false;
	};

	// Ordered RoadStraight pieces owned by one visual actor.
	struct FGeneratedRoadStraightSplineChain
	{
		// Shared side/block key for every piece in the chain.
		FString ChainKey;

		// Ordered road spans that become spline mesh components.
		TArray<FGeneratedRoadStraightSplinePiece> Pieces;
	};

	// Static mesh template data copied from the selected RoadStraight BP class.
	struct FRoadStraightSplineTemplate
	{
		// Static mesh deformed between generated road start/end points.
		UStaticMesh* StaticMesh = nullptr;

		// Materials copied from the source BP static mesh component.
		TArray<UMaterialInterface*> Materials;
	};

	// Disables visual collision while optionally preserving building LiDAR/navigation collision sources.
	int32 DisableCityBlockActorCollision(
		AActor& blockActor,
		bool bPreserveSemanticBlockingComponents,
		bool bPreserveBuildingStaticMeshLidarCollision);

	// Prevents generated visual blocks from receiving projected runtime/editor decals.
	void SetActorReceivesDecals(AActor& blockActor, bool bReceivesDecals);

	// Adds lightweight semantic markers used by LiDAR policy payloads and point cloud classification.
	void ApplyCityBlockSemanticTags(
		AActor& blockActor,
		const FScenarioCityBlockCatalogEntry& blockEntry);

	// Converts a catalog role to a stable diagnostic label.
	const TCHAR* CityBlockRoleToString(EScenarioCityBlockRole role)
	{
		switch (role)
		{
		case EScenarioCityBlockRole::WalkwayRoadStraight:
			return TEXT("WalkwayRoadStraight");
		case EScenarioCityBlockRole::WalkwayBuildingStraight:
			return TEXT("WalkwayBuildingStraight");
		case EScenarioCityBlockRole::RoadStraight:
			return TEXT("RoadStraight");
		case EScenarioCityBlockRole::Corner:
			return TEXT("Corner");
		case EScenarioCityBlockRole::Crossroad:
			return TEXT("Crossroad");
		case EScenarioCityBlockRole::Building:
			return TEXT("Building");
		default:
			return TEXT("Unknown");
		}
	}

	// Converts a lateral anchor to a stable diagnostic label.
	const TCHAR* CityBlockLateralAnchorToString(EScenarioCityBlockLateralAnchor lateralAnchor)
	{
		switch (lateralAnchor)
		{
		case EScenarioCityBlockLateralAnchor::RegionCenter:
			return TEXT("RegionCenter");
		case EScenarioCityBlockLateralAnchor::RegionInnerEdge:
			return TEXT("RegionInnerEdge");
		case EScenarioCityBlockLateralAnchor::RegionOuterEdge:
			return TEXT("RegionOuterEdge");
		default:
			return TEXT("Unknown");
		}
	}

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

	// One expansion edge that can receive repeated building frontage blocks.
	struct FGeneratedBuildingFrontageSpan
	{
		// Start point of the usable frontage edge in world centimeters.
		FVector StartCm = FVector::ZeroVector;

		// End point of the usable frontage edge in world centimeters.
		FVector EndCm = FVector::ZeroVector;

		// Normalized direction from the frontage edge toward the building bounds center.
		FVector Outward = FVector::RightVector;

		// Stable source label used for overlap diagnostics.
		FString DebugSourceId;
	};

	// Building catalog entry prepared for sequential frontage placement.
	struct FGeneratedBuildingFrontageCandidate
	{
		// Catalog entry copied from the DA so spawn-time math is stable while planning a span.
		FScenarioCityBlockCatalogEntry BlockEntry;

		// Loaded actor class for this specific building entry.
		UClass* BlockClass = nullptr;

		// Authored frontage length in world centimeters.
		double LengthCm = 0.0;

		// Half of the authored building depth in world centimeters.
		double HalfWidthCm = 0.0;
	};

	// Filters the materializer to generated straight city padding without touching authored GroundRegions.
	bool IsGeneratedCityVisualRegion(const FScenarioGroundRegionSpec& regionSpec)
	{
		if (!regionSpec.RegionId.StartsWith(GeneratedCityRegionIdPrefix))
		{
			return false;
		}

		return regionSpec.ShapeType == EScenarioGroundShapeType::Rectangle
			|| (regionSpec.ShapeType == EScenarioGroundShapeType::ConvexPolygon
				&& regionSpec.PolygonVertices.Num() >= 3);
	}

	// Detects the generated curb band used as road-side seam metadata.
	bool IsGeneratedRoadCurbRegion(const FScenarioGroundRegionSpec& regionSpec)
	{
		const FName surfaceId(*regionSpec.SurfaceId);
		return IsGeneratedCityVisualRegion(regionSpec)
			&& surfaceId == CityBlockRoadSurfaceId
			&& (regionSpec.CollisionTag.Equals(CurbCollisionTag, ESearchCase::IgnoreCase)
				|| regionSpec.RegionId.Contains(TEXT("_curb_")));
	}

	// Detects the generated road band that receives the RoadStraight curb+road visual block.
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

	// Detects the walkable expansion area whose corridor-facing edge receives Building catalog entries.
	bool IsGeneratedBuildingExpansionRegion(const FScenarioGroundRegionSpec& regionSpec)
	{
		const FName surfaceId(*regionSpec.SurfaceId);
		return IsGeneratedCityVisualRegion(regionSpec)
			&& surfaceId == CityBlockWalkwaySurfaceId
			&& regionSpec.RegionType == EScenarioGroundRegionType::Walkable
			&& regionSpec.RegionId.Contains(TEXT("_building_expansion_"));
	}

	// Detects generated regions that can produce building frontage visual actors.
	bool IsGeneratedBuildingFrontageSourceRegion(const FScenarioGroundRegionSpec& regionSpec)
	{
		return IsGeneratedBuildingRegion(regionSpec)
			|| IsGeneratedBuildingExpansionRegion(regionSpec);
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

	// Expands one footprint only for building-to-building spacing checks, not for actor placement.
	FGeneratedBuildingFootprint MakeBuildingSpacingFootprint(
		const FGeneratedBuildingFootprint& footprint,
		double spacingCm)
	{
		FGeneratedBuildingFootprint spacingFootprint = footprint;
		const double halfSpacingCm = FMath::Max(0.0, spacingCm) * 0.5;
		spacingFootprint.HalfLengthCm += halfSpacingCm;
		spacingFootprint.HalfWidthCm += halfSpacingCm;
		return spacingFootprint;
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
			return;
		}

		if (IsGeneratedWalkwayExtensionRegion(regionSpec))
		{
			outRoles.Add(EScenarioCityBlockRole::WalkwayBuildingStraight);
			return;
		}

		if (IsGeneratedBuildingExpansionRegion(regionSpec))
		{
			outRoles.Add(EScenarioCityBlockRole::Building);
			return;
		}

		if (IsGeneratedRoadPenaltyRegion(regionSpec)
			|| (surfaceId == CityBlockRoadSurfaceId && regionSpec.RegionType == EScenarioGroundRegionType::Penalty))
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

	// Building frontage BPs may still declare the logical building surface while the source area is walkable.
	bool IsCityBlockEntryCompatibleWithBuildingFrontageSource(
		const FScenarioCityBlockCatalogEntry& blockEntry,
		const FScenarioGroundRegionSpec& regionSpec)
	{
		if (!IsGeneratedBuildingExpansionRegion(regionSpec))
		{
			const FName surfaceId(*regionSpec.SurfaceId);
			return IsCityBlockEntrySurfaceCompatible(blockEntry, surfaceId)
				&& IsCityBlockEntryDetailCompatible(blockEntry, regionSpec);
		}

		if (!blockEntry.SemanticProfile.SurfaceIds.IsEmpty()
			&& !blockEntry.SemanticProfile.SurfaceIds.Contains(CityBlockBuildingSurfaceId)
			&& !blockEntry.SemanticProfile.SurfaceIds.Contains(CityBlockWalkwaySurfaceId))
		{
			return false;
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

	// Collects building frontage candidates in DA order so authored building variety is deterministic.
	void FindCityBlockEntriesForBuildingRegion(
		const UScenarioCityBlockCatalog& catalog,
		const FScenarioGroundRegionSpec& regionSpec,
		TArray<FScenarioCityBlockCatalogEntry>& outBlockEntries)
	{
		outBlockEntries.Reset();
		for (const FScenarioCityBlockCatalogEntry& blockEntry : catalog.GetEntries())
		{
			if (blockEntry.Role != EScenarioCityBlockRole::Building)
			{
				continue;
			}

			if (!IsCityBlockEntryCompatibleWithBuildingFrontageSource(blockEntry, regionSpec))
			{
				continue;
			}

			outBlockEntries.Add(blockEntry);
		}
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

	// Projects a corner block's authored bounds from its actor origin onto the requested seam direction.
	double ResolveCornerBoundsExtentFromAnchorCm(
		const FScenarioCityBlockCatalogEntry& blockEntry,
		double cornerYawDegrees,
		const FVector& directionFromAnchorCm,
		double fallbackReserveGapCm)
	{
		const double halfLengthCm = blockEntry.BoundsMeters.LengthMeters * 50.0;
		const double halfWidthCm = blockEntry.BoundsMeters.WidthMeters * 50.0;
		if (halfLengthCm <= KINDA_SMALL_NUMBER || halfWidthCm <= KINDA_SMALL_NUMBER)
		{
			return FMath::Max(0.0, fallbackReserveGapCm);
		}

		const FVector safeDirection = directionFromAnchorCm.GetSafeNormal2D();
		if (safeDirection.IsNearlyZero())
		{
			return FMath::Max(0.0, fallbackReserveGapCm);
		}

		const FRotator cornerRotation(0.0, cornerYawDegrees, 0.0);
		const FVector boundsCenterOffsetCm = blockEntry.BoundsMeters.CenterOffsetMeters * 100.0;
		const FVector localCornersCm[] =
		{
			boundsCenterOffsetCm + FVector(-halfLengthCm, -halfWidthCm, 0.0),
			boundsCenterOffsetCm + FVector(-halfLengthCm, halfWidthCm, 0.0),
			boundsCenterOffsetCm + FVector(halfLengthCm, -halfWidthCm, 0.0),
			boundsCenterOffsetCm + FVector(halfLengthCm, halfWidthCm, 0.0)
		};

		double maxProjectionCm = 0.0;
		for (const FVector& localCornerCm : localCornersCm)
		{
			maxProjectionCm = FMath::Max(
				maxProjectionCm,
				FVector::DotProduct(cornerRotation.RotateVector(localCornerCm), safeDirection));
		}

		return FMath::Max(0.0, maxProjectionCm);
	}

	// Resolves how far a corner block occupies the seam side adjacent to one straight road segment.
	double ResolveCornerReserveGapForSeamCm(
		const FGeneratedRoadCurbSeam& seam,
		const FVector& anchorLocationCm,
		double cornerYawDegrees,
		const FScenarioCityBlockCatalogEntry* cornerBlockEntry,
		double fallbackReserveGapCm)
	{
		if (!cornerBlockEntry)
		{
			return FMath::Max(0.0, fallbackReserveGapCm);
		}

		const FVector seamForward = seam.Forward.GetSafeNormal2D();
		if (seamForward.IsNearlyZero())
		{
			return FMath::Max(0.0, fallbackReserveGapCm);
		}

		const double startDistanceCm = FVector::Dist2D(anchorLocationCm, seam.SeamStartCm);
		const double endDistanceCm = FVector::Dist2D(anchorLocationCm, seam.SeamEndCm);
		const FVector directionFromAnchorCm = startDistanceCm <= endDistanceCm
			? seamForward
			: -seamForward;
		return ResolveCornerBoundsExtentFromAnchorCm(
			*cornerBlockEntry,
			cornerYawDegrees,
			directionFromAnchorCm,
			fallbackReserveGapCm);
	}

	// Keeps the most conservative signed endpoint offset when several corner seams share one region key.
	void ApplyEndpointGapCm(
		FGeneratedRegionStraightGapCm& inOutGapCm,
		bool bStartEndpoint,
		double gapCm)
	{
		if (bStartEndpoint)
		{
			if (!inOutGapCm.bHasStartGap || gapCm > inOutGapCm.StartGapCm)
			{
				inOutGapCm.bHasStartGap = true;
				inOutGapCm.StartGapCm = gapCm;
			}
			return;
		}

		if (!inOutGapCm.bHasEndGap || gapCm > inOutGapCm.EndGapCm)
		{
			inOutGapCm.bHasEndGap = true;
			inOutGapCm.EndGapCm = gapCm;
		}
	}

	// Applies a straight-block corner reservation to whichever endpoint is closest to the inferred anchor.
	void ApplyCornerGapForSeam(
		const FGeneratedRoadCurbSeam& seam,
		const FVector& anchorLocationCm,
		double cornerYawDegrees,
		const FScenarioCityBlockCatalogEntry* cornerBlockEntry,
		double fallbackReserveGapCm,
		TMap<FString, FGeneratedRegionStraightGapCm>& inOutStraightGapsCm)
	{
		const FVector seamForward = seam.Forward.GetSafeNormal2D();
		const double seamLengthCm = FVector::Dist2D(seam.SeamStartCm, seam.SeamEndCm);
		if (seamForward.IsNearlyZero() || seamLengthCm <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		const FVector seamCenterCm = (seam.SeamStartCm + seam.SeamEndCm) * 0.5;
		const double halfSeamLengthCm = seamLengthCm * 0.5;
		const double anchorLocalCm = FVector::DotProduct(anchorLocationCm - seamCenterCm, seamForward);
		const double reserveGapCm = ResolveCornerReserveGapForSeamCm(
			seam,
			anchorLocationCm,
			cornerYawDegrees,
			cornerBlockEntry,
			fallbackReserveGapCm);
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
			const double startGapCm = (anchorLocalCm + reserveGapCm) - (-halfSeamLengthCm);
			ApplyEndpointGapCm(regionGapCm, true, startGapCm);
			if (compositeGapCm)
			{
				ApplyEndpointGapCm(*compositeGapCm, true, startGapCm);
			}
			return;
		}

		const double endGapCm = halfSeamLengthCm - (anchorLocalCm - reserveGapCm);
		ApplyEndpointGapCm(regionGapCm, false, endGapCm);
		if (compositeGapCm)
		{
			ApplyEndpointGapCm(*compositeGapCm, false, endGapCm);
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
		const FScenarioCityBlockCatalogEntry* cornerBlockEntry,
		double fallbackStraightReserveGapCm,
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

				const double cornerYawDegrees = outCornerPlacements.Last().YawDegrees;
				ApplyCornerGapForSeam(
					firstSeam,
					anchorLocationCm,
					cornerYawDegrees,
					cornerBlockEntry,
					fallbackStraightReserveGapCm,
					outStraightGapsCm);
				ApplyCornerGapForSeam(
					secondSeam,
					anchorLocationCm,
					cornerYawDegrees,
					cornerBlockEntry,
					fallbackStraightReserveGapCm,
					outStraightGapsCm);
			}
		}
	}

	// Uses the authored corner/crossroad length so stretched RoadStraight visuals meet the corner mesh edge.
	double ResolveRoadSideCornerStraightReserveGapCm(const FScenarioCityBlockCatalogEntry& blockEntry)
	{
		const double authoredHalfLengthCm = blockEntry.BoundsMeters.LengthMeters * 50.0;
		return authoredHalfLengthCm > KINDA_SMALL_NUMBER
			? authoredHalfLengthCm
			: GeneratedCornerStraightReserveGapCm;
	}

	// Finds road-side chunks whose generated road band has a configured RoadStraight visual block.
	void BuildRoadSideCompositeCandidateKeys(
		const UScenarioCityBlockCatalog& catalog,
		const TArray<FScenarioGroundRegionSpec>& groundRegions,
		TSet<FString>& outCompositeKeys)
	{
		outCompositeKeys.Reset();
		for (const FScenarioGroundRegionSpec& regionSpec : groundRegions)
		{
			if (!IsGeneratedRoadPenaltyRegion(regionSpec))
			{
				continue;
			}

			FScenarioCityBlockCatalogEntry blockEntry;
			if (!FindCityBlockEntryForRegion(catalog, regionSpec, blockEntry)
				|| blockEntry.Role != EScenarioCityBlockRole::RoadStraight)
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

	// Checks whether a generated road chunk has a configured RoadStraight composite candidate.
	bool IsRoadSideCompositeCandidate(
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

	// Builds the grouping key for RoadStraight spline visuals that can share one owner actor.
	FString MakeRoadStraightSplineChainKey(
		const FScenarioGroundRegionSpec& regionSpec,
		const FScenarioCityBlockCatalogEntry& blockEntry)
	{
		FString sideLabel;
		if (!TryResolveGeneratedCitySideLabel(regionSpec.RegionId, sideLabel))
		{
			sideLabel = TEXT("unknown");
		}

		return FString::Printf(
			TEXT("%s|%s|%s"),
			*sideLabel,
			*blockEntry.BlockId.ToString(),
			*blockEntry.BPClass.ToSoftObjectPath().ToString());
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
			// RoadStraight owns curb+road, so generated road bands anchor one curb-width inward at the walkway-curb seam.
			const double roadSideSeamInsetCm =
				IsGeneratedRoadPenaltyRegion(regionSpec) && blockEntry.Role == EScenarioCityBlockRole::RoadStraight
				? FScenarioCorridorGeometry::GeneratedCityCurbWidthMeters * 100.0
				: 0.0;
			return baseRegionCenter
				+ (right * sideSign * (blockHalfWidthCm - regionHalfWidthCm - roadSideSeamInsetCm
					+ postAnchorOffsetCm));
		}

		// For generated road bands, this outer edge is the continuous far-side road boundary.
		return baseRegionCenter
			+ (right * sideSign * (regionHalfWidthCm - blockHalfWidthCm + postAnchorOffsetCm));
	}

	// Matches authored blocker component instances even when Blueprint duplication appends a suffix.
	bool IsCityBlockGridNavBlockerComponentName(const UActorComponent* component)
	{
		if (!IsValid(component))
		{
			return false;
		}

		const FString componentName = component->GetName();
		return componentName.Equals(BuildingGridNavBlockerComponentName, ESearchCase::IgnoreCase)
			|| componentName.StartsWith(BuildingGridNavBlockerComponentName + TEXT("_"), ESearchCase::IgnoreCase);
	}

	// Identifies BP-authored shape components that intentionally represent semantic building blockers.
	bool IsCityBlockSemanticBlockingComponent(const UPrimitiveComponent* primitiveComponent)
	{
		if (!IsValid(primitiveComponent)
			|| !primitiveComponent->IsA<UShapeComponent>())
		{
			return false;
		}

		return primitiveComponent->GetCollisionProfileName() == GeneratedBuildingCollisionProxyProfileName
			|| IsCityBlockGridNavBlockerComponentName(primitiveComponent);
	}

	// Identifies BP static mesh components that should remain visible to robot LiDAR traces.
	bool IsCityBlockStaticMeshLidarComponent(const UPrimitiveComponent* primitiveComponent)
	{
		const UStaticMeshComponent* staticMeshComponent = Cast<UStaticMeshComponent>(primitiveComponent);
		return IsValid(staticMeshComponent)
			&& IsValid(staticMeshComponent->GetStaticMesh())
			&& staticMeshComponent->GetCollisionEnabled() != ECollisionEnabled::NoCollision;
	}

	// Normalizes preserved BP-authored blocker components for grid/navigation queries.
	void ConfigureCityBlockSemanticBlockingComponent(UPrimitiveComponent& primitiveComponent)
	{
		primitiveComponent.SetCollisionProfileName(GeneratedBuildingCollisionProxyProfileName);
		primitiveComponent.SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		primitiveComponent.SetCollisionResponseToAllChannels(ECR_Ignore);
		primitiveComponent.SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
		primitiveComponent.SetCollisionResponseToChannel(ECC_GameTraceChannel8, ECR_Block);
		primitiveComponent.SetGenerateOverlapEvents(false);
		primitiveComponent.SetHiddenInGame(true);
		primitiveComponent.SetVisibility(false);
	}

	// Keeps visual building geometry raycastable without making it authoritative for grid generation.
	void ConfigureCityBlockStaticMeshLidarComponent(UPrimitiveComponent& primitiveComponent)
	{
		primitiveComponent.SetCollisionProfileName(GeneratedBuildingCollisionProxyProfileName);
		primitiveComponent.SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		primitiveComponent.SetCollisionResponseToAllChannels(ECR_Ignore);
		primitiveComponent.SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		primitiveComponent.SetCollisionResponseToChannel(ECC_GameTraceChannel8, ECR_Ignore);
		primitiveComponent.SetGenerateOverlapEvents(false);
	}

	// Identifies navigation blockers preserved after visual collision has been stripped from a CityBuildings actor.
	bool IsCityBlockNavigationBlockingComponent(const UPrimitiveComponent* primitiveComponent)
	{
		if (!IsValid(primitiveComponent)
			|| primitiveComponent->GetCollisionEnabled() == ECollisionEnabled::NoCollision
			|| primitiveComponent->GetCollisionProfileName() != GeneratedBuildingCollisionProxyProfileName)
		{
			return false;
		}

		return primitiveComponent->IsA<UShapeComponent>();
	}

	// Counts preserved building navigation blockers before falling back to generated bounds proxies.
	int32 CountCityBlockBuildingNavigationBlockingComponents(const AActor& blockActor)
	{
		int32 blockingComponentCount = 0;
		TArray<UPrimitiveComponent*> primitiveComponents;
		blockActor.GetComponents<UPrimitiveComponent>(primitiveComponents);
		for (const UPrimitiveComponent* primitiveComponent : primitiveComponents)
		{
			if (IsCityBlockNavigationBlockingComponent(primitiveComponent))
			{
				++blockingComponentCount;
			}
		}

		return blockingComponentCount;
	}

	// Disables visual CityBuildings collision while optionally preserving building blockers.
	int32 DisableCityBlockActorCollision(
		AActor& blockActor,
		bool bPreserveSemanticBlockingComponents,
		bool bPreserveBuildingStaticMeshLidarCollision)
	{
		blockActor.SetActorEnableCollision(false);

		int32 preservedBlockingComponentCount = 0;
		TArray<UPrimitiveComponent*> primitiveComponents;
		blockActor.GetComponents<UPrimitiveComponent>(primitiveComponents);
		for (UPrimitiveComponent* primitiveComponent : primitiveComponents)
		{
			if (!primitiveComponent)
			{
				continue;
			}

			if (bPreserveSemanticBlockingComponents
				&& IsCityBlockSemanticBlockingComponent(primitiveComponent))
			{
				ConfigureCityBlockSemanticBlockingComponent(*primitiveComponent);
				++preservedBlockingComponentCount;
				continue;
			}

			if (bPreserveBuildingStaticMeshLidarCollision
				&& IsCityBlockStaticMeshLidarComponent(primitiveComponent))
			{
				ConfigureCityBlockStaticMeshLidarComponent(*primitiveComponent);
				++preservedBlockingComponentCount;
				continue;
			}

			primitiveComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			primitiveComponent->SetGenerateOverlapEvents(false);
		}

		if (preservedBlockingComponentCount > 0)
		{
			blockActor.SetActorEnableCollision(true);
		}

		return preservedBlockingComponentCount;
	}

	// Adds a hidden Blocked box based on authored building bounds for runtime grid classification.
	bool AttachBuildingCollisionProxy(
		AActor& blockActor,
		const FGeneratedBuildingFootprint& buildingFootprint,
		const FRotator& blockRotation)
	{
		USceneComponent* rootComponent = blockActor.GetRootComponent();
		if (!rootComponent)
		{
			return false;
		}

		UBoxComponent* proxyComponent = NewObject<UBoxComponent>(
			&blockActor,
			MakeUniqueObjectName(
				&blockActor,
				UBoxComponent::StaticClass(),
				GeneratedBuildingCollisionProxyComponentName));
		if (!proxyComponent)
		{
			return false;
		}

		const double halfHeightCm = GeneratedBuildingCollisionProxyHeightCm * 0.5;
		const FVector proxyCenterCm =
			buildingFootprint.CenterCm + FVector(0.0, 0.0, halfHeightCm);

		proxyComponent->SetBoxExtent(FVector(
			buildingFootprint.HalfLengthCm,
			buildingFootprint.HalfWidthCm,
			halfHeightCm));
		proxyComponent->SetWorldLocation(proxyCenterCm);
		proxyComponent->SetWorldRotation(blockRotation);
		proxyComponent->SetCollisionProfileName(GeneratedBuildingCollisionProxyProfileName);
		proxyComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		proxyComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
		proxyComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
		proxyComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel8, ECR_Block);
		proxyComponent->SetGenerateOverlapEvents(false);
		proxyComponent->SetHiddenInGame(true);
		proxyComponent->SetVisibility(false);

		blockActor.AddInstanceComponent(proxyComponent);
		proxyComponent->RegisterComponent();
		proxyComponent->AttachToComponent(
			rootComponent,
			FAttachmentTransformRules::KeepWorldTransform);
		blockActor.SetActorEnableCollision(true);
		return true;
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

	// Adds catalog semantics to generated actors without making visual mesh collision authoritative.
	void ApplyCityBlockSemanticTags(
		AActor& blockActor,
		const FScenarioCityBlockCatalogEntry& blockEntry)
	{
		blockActor.Tags.AddUnique(FName(TEXT("city_block")));
		blockActor.Tags.AddUnique(FName(*FString::Printf(
			TEXT("city_block_role_%s"),
			CityBlockRoleToString(blockEntry.Role))));

		for (const FName& surfaceId : blockEntry.SemanticProfile.SurfaceIds)
		{
			if (!surfaceId.IsNone())
			{
				blockActor.Tags.AddUnique(surfaceId);
			}
		}

		if (!blockEntry.SemanticProfile.CollisionTag.IsEmpty())
		{
			blockActor.Tags.AddUnique(FName(*blockEntry.SemanticProfile.CollisionTag));
		}
		if (!blockEntry.SemanticProfile.PenaltyKind.IsEmpty())
		{
			blockActor.Tags.AddUnique(FName(*blockEntry.SemanticProfile.PenaltyKind));
		}

		if (blockEntry.Role == EScenarioCityBlockRole::Building)
		{
			blockActor.Tags.AddUnique(FName(TEXT("building")));
			blockActor.Tags.AddUnique(FName(TEXT("wall")));
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

	// Reads the first static mesh component from a RoadStraight BP class so the mesh can be stretched as a spline.
	bool TryCopyRoadStraightSplineTemplateFromComponent(
		const UStaticMeshComponent& staticMeshComponent,
		FRoadStraightSplineTemplate& outTemplate)
	{
		if (!staticMeshComponent.GetStaticMesh())
		{
			return false;
		}

		outTemplate.StaticMesh = staticMeshComponent.GetStaticMesh();
		const int32 materialCount = staticMeshComponent.GetNumMaterials();
		for (int32 materialIndex = 0; materialIndex < materialCount; ++materialIndex)
		{
			outTemplate.Materials.Add(staticMeshComponent.GetMaterial(materialIndex));
		}
		return true;
	}

	// Reads the first static mesh component from a RoadStraight BP class so the mesh can be stretched as a spline.
	bool TryResolveRoadStraightSplineTemplateFromDefaultObject(
		UClass& blockClass,
		FRoadStraightSplineTemplate& outTemplate)
	{
		AActor* defaultActor = Cast<AActor>(blockClass.GetDefaultObject());
		if (!defaultActor)
		{
			return false;
		}

		TArray<UStaticMeshComponent*> staticMeshComponents;
		defaultActor->GetComponents<UStaticMeshComponent>(staticMeshComponents);
		for (const UStaticMeshComponent* staticMeshComponent : staticMeshComponents)
		{
			if (!staticMeshComponent)
			{
				continue;
			}

			if (TryCopyRoadStraightSplineTemplateFromComponent(*staticMeshComponent, outTemplate))
			{
				return true;
			}
		}

		return false;
	}

	// Reads Blueprint-authored component templates that are not exposed through the class default actor's component list.
	bool TryResolveRoadStraightSplineTemplateFromBlueprintSCS(
		UClass& blockClass,
		FRoadStraightSplineTemplate& outTemplate)
	{
		UBlueprintGeneratedClass* blueprintClass = Cast<UBlueprintGeneratedClass>(&blockClass);
		if (!blueprintClass || !blueprintClass->SimpleConstructionScript)
		{
			return false;
		}

		for (USCS_Node* scsNode : blueprintClass->SimpleConstructionScript->GetAllNodes())
		{
			if (!scsNode)
			{
				continue;
			}

			const UStaticMeshComponent* staticMeshTemplate =
				Cast<UStaticMeshComponent>(scsNode->GetActualComponentTemplate(blueprintClass));
			if (!staticMeshTemplate)
			{
				continue;
			}

			if (TryCopyRoadStraightSplineTemplateFromComponent(*staticMeshTemplate, outTemplate))
			{
				return true;
			}
		}

		return false;
	}

	// Resolves RoadStraight mesh/material data from native or Blueprint-authored component templates.
	bool TryResolveRoadStraightSplineTemplate(
		UClass& blockClass,
		FRoadStraightSplineTemplate& outTemplate)
	{
		outTemplate = FRoadStraightSplineTemplate();
		return TryResolveRoadStraightSplineTemplateFromDefaultObject(blockClass, outTemplate)
			|| TryResolveRoadStraightSplineTemplateFromBlueprintSCS(blockClass, outTemplate);
	}

	// Resolves one generated road GroundRegion into a RoadStraight spline span.
	bool TryBuildRoadStraightSplinePiece(
		const FScenarioGroundRegionSpec& regionSpec,
		const FScenarioCityBlockCatalogEntry& blockEntry,
		const FGeneratedRegionStraightGapCm* straightGapCm,
		FGeneratedRoadStraightSplinePiece& outPiece,
		const FScenarioCityBlockMaterializationOptions& options)
	{
		outPiece = FGeneratedRoadStraightSplinePiece();
		UClass* blockClass = LoadCityBlockActorClass(blockEntry, regionSpec.RegionId, options);
		if (!blockClass)
		{
			return false;
		}

		if (regionSpec.Size.X <= KINDA_SMALL_NUMBER)
		{
			UE_LOG(
				LogScenarioCityBlockMaterializer,
				Warning,
				TEXT("%s RoadStraight spline skipped | Region: %s, BlockId: %s, RegionLengthCm: %.2f"),
				*options.LogContext,
				*regionSpec.RegionId,
				*blockEntry.BlockId.ToString(),
				regionSpec.Size.X);
			return false;
		}

		const double reservedStartGapCm = straightGapCm && straightGapCm->bHasStartGap
			? straightGapCm->StartGapCm
			: 0.0;
		const double reservedEndGapCm = straightGapCm && straightGapCm->bHasEndGap
			? straightGapCm->EndGapCm
			: 0.0;
		const double usableStartCm = (-regionSpec.Size.X * 0.5) + reservedStartGapCm;
		const double usableEndCm = (regionSpec.Size.X * 0.5) - reservedEndGapCm;
		const double usableLengthCm = usableEndCm - usableStartCm;
		if (usableLengthCm <= KINDA_SMALL_NUMBER)
		{
			UE_LOG(
				LogScenarioCityBlockMaterializer,
				Log,
				TEXT(
					"%s RoadStraight spline skipped | Region: %s, BlockId: %s, "
					"StartGapCm: %.2f, EndGapCm: %.2f, UsableLengthCm: %.2f"),
				*options.LogContext,
				*regionSpec.RegionId,
				*blockEntry.BlockId.ToString(),
				reservedStartGapCm,
				reservedEndGapCm,
				usableLengthCm);
			return false;
		}

		const FRotator blockRotation(0.0, regionSpec.YawDegrees, 0.0);
		const FVector forward = blockRotation.RotateVector(FVector::ForwardVector);
		const double alongOffsetCm = (usableStartCm + usableEndCm) * 0.5;
		const FVector desiredBoundsCenter = ResolveDesiredBlockBoundsCenter(
			regionSpec,
			blockEntry,
			blockRotation,
			forward,
			alongOffsetCm);
		const FVector startCm = desiredBoundsCenter - (forward * usableLengthCm * 0.5);
		const FVector endCm = desiredBoundsCenter + (forward * usableLengthCm * 0.5);

		outPiece.RegionId = regionSpec.RegionId;
		outPiece.ChainKey = MakeRoadStraightSplineChainKey(regionSpec, blockEntry);
		outPiece.BlockEntry = blockEntry;
		outPiece.BlockClass = blockClass;
		outPiece.StartCm = startCm;
		outPiece.EndCm = endCm;
		outPiece.bStartHasCornerGap = straightGapCm && straightGapCm->bHasStartGap;
		outPiece.bEndHasCornerGap = straightGapCm && straightGapCm->bHasEndGap;

		UE_LOG(
			LogScenarioCityBlockMaterializer,
			Log,
			TEXT(
				"%s RoadStraight spline piece | Region: %s, BlockId: %s, BP: %s, "
				"BoundsMeters(L=%.3f W=%.3f H=%.3f), CenterOffsetMeters=(%.3f, %.3f, %.3f), "
				"LateralAnchor: %s, RegionLengthCm: %.2f, StartGapCm: %.2f, EndGapCm: %.2f, "
				"UsableStartCm: %.2f, UsableEndCm: %.2f, UsableLengthCm: %.2f, "
				"StartCm=(%.2f, %.2f, %.2f), EndCm=(%.2f, %.2f, %.2f), YawDeg: %.2f"),
			*options.LogContext,
			*regionSpec.RegionId,
			*blockEntry.BlockId.ToString(),
			*blockEntry.BPClass.ToSoftObjectPath().ToString(),
			blockEntry.BoundsMeters.LengthMeters,
			blockEntry.BoundsMeters.WidthMeters,
			blockEntry.BoundsMeters.HeightMeters,
			blockEntry.BoundsMeters.CenterOffsetMeters.X,
			blockEntry.BoundsMeters.CenterOffsetMeters.Y,
			blockEntry.BoundsMeters.CenterOffsetMeters.Z,
			CityBlockLateralAnchorToString(blockEntry.PlacementProfile.LateralAnchor),
			regionSpec.Size.X,
			reservedStartGapCm,
			reservedEndGapCm,
			usableStartCm,
			usableEndCm,
			usableLengthCm,
			startCm.X,
			startCm.Y,
			startCm.Z,
			endCm.X,
			endCm.Y,
			endCm.Z,
			regionSpec.YawDegrees);
		return true;
	}

	// Returns a copy of a road spline piece with its direction reversed for chain construction.
	FGeneratedRoadStraightSplinePiece ReverseRoadStraightSplinePiece(
		const FGeneratedRoadStraightSplinePiece& piece)
	{
		FGeneratedRoadStraightSplinePiece reversedPiece = piece;
		Swap(reversedPiece.StartCm, reversedPiece.EndCm);
		Swap(reversedPiece.bStartHasCornerGap, reversedPiece.bEndHasCornerGap);
		return reversedPiece;
	}

	// Checks whether two RoadStraight endpoints can be merged into one continuous spline chain.
	bool CanJoinRoadStraightSplineEndpoints(
		const FGeneratedRoadStraightSplinePiece& firstPiece,
		bool bFirstUsesEnd,
		const FGeneratedRoadStraightSplinePiece& secondPiece,
		bool bSecondUsesStart)
	{
		if (firstPiece.ChainKey != secondPiece.ChainKey)
		{
			return false;
		}

		const bool bFirstHasCornerGap = bFirstUsesEnd
			? firstPiece.bEndHasCornerGap
			: firstPiece.bStartHasCornerGap;
		const bool bSecondHasCornerGap = bSecondUsesStart
			? secondPiece.bStartHasCornerGap
			: secondPiece.bEndHasCornerGap;
		if (bFirstHasCornerGap || bSecondHasCornerGap)
		{
			return false;
		}

		const FVector firstEndpoint = bFirstUsesEnd ? firstPiece.EndCm : firstPiece.StartCm;
		const FVector secondEndpoint = bSecondUsesStart ? secondPiece.StartCm : secondPiece.EndCm;
		return FVector::Dist2D(firstEndpoint, secondEndpoint) <= GeneratedRoadStraightSplineJoinToleranceCm;
	}

	// Snaps a chain join to one shared point so adjacent spline mesh sections do not leave a visual gap.
	void MergeRoadStraightSplineJoin(
		FGeneratedRoadStraightSplinePiece& firstPiece,
		bool bFirstUsesEnd,
		FGeneratedRoadStraightSplinePiece& secondPiece,
		bool bSecondUsesStart)
	{
		const FVector firstEndpoint = bFirstUsesEnd ? firstPiece.EndCm : firstPiece.StartCm;
		const FVector secondEndpoint = bSecondUsesStart ? secondPiece.StartCm : secondPiece.EndCm;
		const FVector mergedEndpoint = (firstEndpoint + secondEndpoint) * 0.5;
		if (bFirstUsesEnd)
		{
			firstPiece.EndCm = mergedEndpoint;
		}
		else
		{
			firstPiece.StartCm = mergedEndpoint;
		}

		if (bSecondUsesStart)
		{
			secondPiece.StartCm = mergedEndpoint;
		}
		else
		{
			secondPiece.EndCm = mergedEndpoint;
		}
	}

	// Builds continuous RoadStraight chains from generated road spans.
	void BuildRoadStraightSplineChains(
		const TArray<FGeneratedRoadStraightSplinePiece>& pieces,
		TArray<FGeneratedRoadStraightSplineChain>& outChains)
	{
		outChains.Reset();
		TArray<bool> usedPieces;
		usedPieces.Init(false, pieces.Num());

		for (int32 seedIndex = 0; seedIndex < pieces.Num(); ++seedIndex)
		{
			if (usedPieces[seedIndex])
			{
				continue;
			}

			FGeneratedRoadStraightSplineChain chain;
			chain.ChainKey = pieces[seedIndex].ChainKey;
			chain.Pieces.Add(pieces[seedIndex]);
			usedPieces[seedIndex] = true;

			bool bExtended = true;
			while (bExtended)
			{
				bExtended = false;
				for (int32 candidateIndex = 0; candidateIndex < pieces.Num(); ++candidateIndex)
				{
					if (usedPieces[candidateIndex] || pieces[candidateIndex].ChainKey != chain.ChainKey)
					{
						continue;
					}

					FGeneratedRoadStraightSplinePiece appendCandidate = pieces[candidateIndex];
					if (CanJoinRoadStraightSplineEndpoints(chain.Pieces.Last(), true, appendCandidate, true))
					{
						MergeRoadStraightSplineJoin(chain.Pieces.Last(), true, appendCandidate, true);
						chain.Pieces.Add(appendCandidate);
						usedPieces[candidateIndex] = true;
						bExtended = true;
						break;
					}

					appendCandidate = ReverseRoadStraightSplinePiece(pieces[candidateIndex]);
					if (CanJoinRoadStraightSplineEndpoints(chain.Pieces.Last(), true, appendCandidate, true))
					{
						MergeRoadStraightSplineJoin(chain.Pieces.Last(), true, appendCandidate, true);
						chain.Pieces.Add(appendCandidate);
						usedPieces[candidateIndex] = true;
						bExtended = true;
						break;
					}

					FGeneratedRoadStraightSplinePiece prependCandidate = pieces[candidateIndex];
					if (CanJoinRoadStraightSplineEndpoints(prependCandidate, true, chain.Pieces[0], true))
					{
						MergeRoadStraightSplineJoin(prependCandidate, true, chain.Pieces[0], true);
						chain.Pieces.Insert(prependCandidate, 0);
						usedPieces[candidateIndex] = true;
						bExtended = true;
						break;
					}

					prependCandidate = ReverseRoadStraightSplinePiece(pieces[candidateIndex]);
					if (CanJoinRoadStraightSplineEndpoints(prependCandidate, true, chain.Pieces[0], true))
					{
						MergeRoadStraightSplineJoin(prependCandidate, true, chain.Pieces[0], true);
						chain.Pieces.Insert(prependCandidate, 0);
						usedPieces[candidateIndex] = true;
						bExtended = true;
						break;
					}
				}
			}

			outChains.Add(MoveTemp(chain));
		}
	}

	// Resolves a spline tangent for an ordered RoadStraight chain point.
	FVector ResolveRoadStraightSplineTangent(
		const FGeneratedRoadStraightSplineChain& chain,
		int32 pointIndex)
	{
		if (chain.Pieces.IsEmpty())
		{
			return FVector::ZeroVector;
		}

		if (pointIndex <= 0)
		{
			return chain.Pieces[0].EndCm - chain.Pieces[0].StartCm;
		}

		if (pointIndex >= chain.Pieces.Num())
		{
			const FGeneratedRoadStraightSplinePiece& lastPiece = chain.Pieces.Last();
			return lastPiece.EndCm - lastPiece.StartCm;
		}

		const FVector previousDelta = chain.Pieces[pointIndex - 1].EndCm - chain.Pieces[pointIndex - 1].StartCm;
		const FVector nextDelta = chain.Pieces[pointIndex].EndCm - chain.Pieces[pointIndex].StartCm;
		const FVector previousDirection = previousDelta.GetSafeNormal2D();
		const FVector nextDirection = nextDelta.GetSafeNormal2D();
		FVector tangentDirection = (previousDirection + nextDirection).GetSafeNormal2D();
		if (tangentDirection.IsNearlyZero())
		{
			tangentDirection = nextDirection.IsNearlyZero() ? previousDirection : nextDirection;
		}

		const double tangentLengthCm = FMath::Min(previousDelta.Size2D(), nextDelta.Size2D()) * 0.5;
		return tangentDirection * tangentLengthCm;
	}

	// Spawns spline mesh owner actors for all generated RoadStraight visual chains.
	int32 SpawnRoadStraightSplineVisuals(
		UWorld& world,
		const TArray<FGeneratedRoadStraightSplinePiece>& pieces,
		TArray<TObjectPtr<AActor>>& outSpawnedActors,
		int32& outSkippedSpawnFailureCount,
		const FScenarioCityBlockMaterializationOptions& options)
	{
		outSkippedSpawnFailureCount = 0;
		if (pieces.IsEmpty())
		{
			return 0;
		}

		TArray<FGeneratedRoadStraightSplineChain> chains;
		BuildRoadStraightSplineChains(pieces, chains);

		int32 spawnedActorCount = 0;
		for (const FGeneratedRoadStraightSplineChain& chain : chains)
		{
			if (chain.Pieces.IsEmpty() || !chain.Pieces[0].BlockClass)
			{
				++outSkippedSpawnFailureCount;
				continue;
			}

			FRoadStraightSplineTemplate splineTemplate;
			if (!TryResolveRoadStraightSplineTemplate(*chain.Pieces[0].BlockClass, splineTemplate)
				|| !splineTemplate.StaticMesh)
			{
				UE_LOG(
					LogScenarioCityBlockMaterializer,
					Warning,
					TEXT("%s RoadStraight spline skipped | Chain: %s, BlockId: %s because BPClass has no static mesh component."),
					*options.LogContext,
					*chain.ChainKey,
					*chain.Pieces[0].BlockEntry.BlockId.ToString());
				++outSkippedSpawnFailureCount;
				continue;
			}

			const FVector chainOriginCm = chain.Pieces[0].StartCm;
			FActorSpawnParameters spawnParams;
			spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AActor* roadActor = world.SpawnActor<AActor>(
				AActor::StaticClass(),
				FTransform(FRotator::ZeroRotator, chainOriginCm),
				spawnParams);
			if (!roadActor)
			{
				UE_LOG(
					LogScenarioCityBlockMaterializer,
					Warning,
					TEXT("%s RoadStraight spline actor failed to spawn | Chain: %s"),
					*options.LogContext,
					*chain.ChainKey);
				++outSkippedSpawnFailureCount;
				continue;
			}

			USceneComponent* sceneRoot = NewObject<USceneComponent>(roadActor, TEXT("RoadStraightSplineRoot"));
			sceneRoot->SetMobility(EComponentMobility::Movable);
			roadActor->SetRootComponent(sceneRoot);
			roadActor->AddInstanceComponent(sceneRoot);
			sceneRoot->RegisterComponent();
			sceneRoot->SetWorldLocation(chainOriginCm);
			roadActor->SetActorEnableCollision(false);
			ApplyCityBlockSemanticTags(*roadActor, chain.Pieces[0].BlockEntry);

			for (int32 pieceIndex = 0; pieceIndex < chain.Pieces.Num(); ++pieceIndex)
			{
				const FGeneratedRoadStraightSplinePiece& piece = chain.Pieces[pieceIndex];
				const FName componentName(*FString::Printf(TEXT("RoadStraightSplineMesh_%02d"), pieceIndex));
				USplineMeshComponent* splineMeshComponent = NewObject<USplineMeshComponent>(roadActor, componentName);
				splineMeshComponent->SetMobility(EComponentMobility::Movable);
				splineMeshComponent->SetupAttachment(sceneRoot);
				splineMeshComponent->SetStaticMesh(splineTemplate.StaticMesh);
				splineMeshComponent->SetForwardAxis(ESplineMeshAxis::X, false);
				splineMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				splineMeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
				splineMeshComponent->SetGenerateOverlapEvents(false);
				splineMeshComponent->SetCastShadow(false);
				splineMeshComponent->SetReceivesDecals(false);
				for (int32 materialIndex = 0; materialIndex < splineTemplate.Materials.Num(); ++materialIndex)
				{
					if (splineTemplate.Materials[materialIndex])
					{
						splineMeshComponent->SetMaterial(materialIndex, splineTemplate.Materials[materialIndex]);
					}
				}

				const FVector startLocalCm = piece.StartCm - chainOriginCm;
				const FVector endLocalCm = piece.EndCm - chainOriginCm;
				const FVector startTangentCm = ResolveRoadStraightSplineTangent(chain, pieceIndex);
				const FVector endTangentCm = ResolveRoadStraightSplineTangent(chain, pieceIndex + 1);
				roadActor->AddInstanceComponent(splineMeshComponent);
				splineMeshComponent->RegisterComponent();
				splineMeshComponent->SetStartAndEnd(startLocalCm, startTangentCm, endLocalCm, endTangentCm, true);
			}

			DisableCityBlockActorCollision(*roadActor, false, false);
			SetActorReceivesDecals(*roadActor, false);
			outSpawnedActors.Add(roadActor);
			++spawnedActorCount;

			UE_LOG(
				LogScenarioCityBlockMaterializer,
				Log,
				TEXT("%s RoadStraight spline chain spawned | Chain: %s, Pieces: %d, OriginCm=(%.2f, %.2f, %.2f)"),
				*options.LogContext,
				*chain.ChainKey,
				chain.Pieces.Num(),
				chainOriginCm.X,
				chainOriginCm.Y,
				chainOriginCm.Z);
		}

		return spawnedActorCount;
	}

	// Spawns one visual-only block actor at a resolved actor-origin transform.
	AActor* SpawnCityBlockActorAtLocation(
		UWorld& world,
		UClass& blockClass,
		const FScenarioCityBlockCatalogEntry& blockEntry,
		const FRotator& blockRotation,
		const FVector& actorLocation,
		const FString& debugSourceId,
		TArray<TObjectPtr<AActor>>& outSpawnedActors,
		const FScenarioCityBlockMaterializationOptions& options)
	{
		FActorSpawnParameters spawnParams;
		spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AActor* blockActor = world.SpawnActor<AActor>(
			&blockClass,
			FTransform(blockRotation, actorLocation),
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

		const bool bPreserveBuildingCollisionSources =
			options.bCreateBuildingCollisionProxies && blockEntry.Role == EScenarioCityBlockRole::Building;
		const int32 preservedCollisionSourceCount = DisableCityBlockActorCollision(
			*blockActor,
			bPreserveBuildingCollisionSources,
			bPreserveBuildingCollisionSources);
		ApplyCityBlockSemanticTags(*blockActor, blockEntry);
		if (preservedCollisionSourceCount > 0)
		{
			UE_LOG(
				LogScenarioCityBlockMaterializer,
				Verbose,
				TEXT("%s generated city block '%s' preserved %d BP-authored building collision component(s) for '%s'."),
				*options.LogContext,
				*blockEntry.BlockId.ToString(),
				preservedCollisionSourceCount,
				*debugSourceId);
		}
		SetActorReceivesDecals(*blockActor, false);
		outSpawnedActors.Add(blockActor);
		return blockActor;
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
		const FVector boundsCenterOffsetCm = blockEntry.BoundsMeters.CenterOffsetMeters * 100.0;
		const FVector actorLocation =
			desiredBoundsCenter - blockRotation.RotateVector(boundsCenterOffsetCm);
		return SpawnCityBlockActorAtLocation(
			world,
			blockClass,
			blockEntry,
			blockRotation,
			actorLocation,
			debugSourceId,
			outSpawnedActors,
			options);
	}

	// Transforms one polygon vertex from region-local centimeters into world centimeters.
	FVector ResolveGroundRegionPolygonVertexWorldCm(
		const FScenarioGroundRegionSpec& regionSpec,
		const FVector2D& localVertexCm)
	{
		const FRotator regionRotation(0.0, regionSpec.YawDegrees, 0.0);
		FVector worldVertexCm = regionSpec.Center
			+ regionRotation.RotateVector(FVector(localVertexCm.X, localVertexCm.Y, 0.0));
		worldVertexCm.Z = ResolveCityBlockSurfaceTopZCm(regionSpec);
		return worldVertexCm;
	}

	// Builds the frontage span for a rectangular legacy building band or walkable expansion.
	bool TryBuildRectangleBuildingFrontageSpan(
		const FScenarioGroundRegionSpec& regionSpec,
		FGeneratedBuildingFrontageSpan& outSpan)
	{
		outSpan = FGeneratedBuildingFrontageSpan();
		if (regionSpec.Size.X <= KINDA_SMALL_NUMBER || regionSpec.Size.Y <= KINDA_SMALL_NUMBER)
		{
			return false;
		}

		double sideSign = 0.0;
		if (!TryResolveGeneratedCitySideSign(regionSpec.RegionId, sideSign))
		{
			return false;
		}

		const FRotator regionRotation(0.0, regionSpec.YawDegrees, 0.0);
		const FVector forward = Normalize2DAxis(regionRotation.RotateVector(FVector::ForwardVector));
		const FVector outward = Normalize2DAxis(regionRotation.RotateVector(FVector::RightVector) * sideSign);
		if (forward.IsNearlyZero() || outward.IsNearlyZero())
		{
			return false;
		}

		FVector edgeCenterCm =
			regionSpec.Center - (outward * regionSpec.Size.Y * 0.5);
		edgeCenterCm.Z = ResolveCityBlockSurfaceTopZCm(regionSpec);

		outSpan.StartCm = edgeCenterCm - (forward * regionSpec.Size.X * 0.5);
		outSpan.EndCm = edgeCenterCm + (forward * regionSpec.Size.X * 0.5);
		outSpan.Outward = outward;
		outSpan.DebugSourceId = regionSpec.RegionId;
		return true;
	}

	// Builds frontage spans from the corridor-facing edge of a generated convex walkable expansion.
	bool BuildPolygonBuildingFrontageSpans(
		const FScenarioGroundRegionSpec& regionSpec,
		TArray<FGeneratedBuildingFrontageSpan>& outSpans)
	{
		outSpans.Reset();
		if (!IsGeneratedBuildingExpansionRegion(regionSpec)
			|| regionSpec.PolygonVertices.Num() < 3)
		{
			return false;
		}

		double sideSign = 0.0;
		if (!TryResolveGeneratedCitySideSign(regionSpec.RegionId, sideSign))
		{
			return false;
		}

		const FRotator regionRotation(0.0, regionSpec.YawDegrees, 0.0);
		const FVector preferredForward =
			Normalize2DAxis(regionRotation.RotateVector(FVector::ForwardVector));
		const FVector outward =
			Normalize2DAxis(regionRotation.RotateVector(FVector::RightVector) * sideSign);
		if (preferredForward.IsNearlyZero() || outward.IsNearlyZero())
		{
			return false;
		}

		TArray<FVector> worldVerticesCm;
		worldVerticesCm.Reserve(regionSpec.PolygonVertices.Num());
		for (const FVector2D& localVertexCm : regionSpec.PolygonVertices)
		{
			worldVerticesCm.Add(ResolveGroundRegionPolygonVertexWorldCm(regionSpec, localVertexCm));
		}

		double minCorridorProjectionCm = MAX_dbl;
		for (int32 vertexIndex = 0; vertexIndex < worldVerticesCm.Num(); ++vertexIndex)
		{
			const FVector& startCm = worldVerticesCm[vertexIndex];
			const FVector& endCm = worldVerticesCm[(vertexIndex + 1) % worldVerticesCm.Num()];
			const FVector edgeVectorCm = endCm - startCm;
			if (Normalize2DAxis(edgeVectorCm).IsNearlyZero())
			{
				continue;
			}

			const FVector midpointCm = (startCm + endCm) * 0.5;
			minCorridorProjectionCm = FMath::Min(minCorridorProjectionCm, Dot2D(midpointCm, outward));
		}

		if (minCorridorProjectionCm >= MAX_dbl * 0.5)
		{
			return false;
		}

		for (int32 vertexIndex = 0; vertexIndex < worldVerticesCm.Num(); ++vertexIndex)
		{
			FVector startCm = worldVerticesCm[vertexIndex];
			FVector endCm = worldVerticesCm[(vertexIndex + 1) % worldVerticesCm.Num()];
			FVector edgeForward = Normalize2DAxis(endCm - startCm);
			if (edgeForward.IsNearlyZero())
			{
				continue;
			}

			const FVector midpointCm = (startCm + endCm) * 0.5;
			const double edgeProjectionCm = Dot2D(midpointCm, outward);
			if (edgeProjectionCm > minCorridorProjectionCm + GeneratedBuildingFrontageCorridorEdgeProjectionToleranceCm)
			{
				continue;
			}

			if (Dot2D(edgeForward, preferredForward) < 0.0)
			{
				const FVector previousStartCm = startCm;
				startCm = endCm;
				endCm = previousStartCm;
				edgeForward *= -1.0;
			}

			FGeneratedBuildingFrontageSpan span;
			span.StartCm = startCm;
			span.EndCm = endCm;
			span.Outward = outward;
			span.DebugSourceId = FString::Printf(
				TEXT("%s_edge_%d"),
				*regionSpec.RegionId,
				vertexIndex);
			outSpans.Add(span);
		}

		return !outSpans.IsEmpty();
	}

	// Resolves one or more building frontage spans from a generated source region.
	bool BuildBuildingFrontageSpansForRegion(
		const FScenarioGroundRegionSpec& regionSpec,
		TArray<FGeneratedBuildingFrontageSpan>& outSpans)
	{
		outSpans.Reset();
		if (regionSpec.ShapeType == EScenarioGroundShapeType::Rectangle)
		{
			FGeneratedBuildingFrontageSpan span;
			if (!TryBuildRectangleBuildingFrontageSpan(regionSpec, span))
			{
				return false;
			}

			outSpans.Add(span);
			return true;
		}

		if (regionSpec.ShapeType == EScenarioGroundShapeType::ConvexPolygon)
		{
			return BuildPolygonBuildingFrontageSpans(regionSpec, outSpans);
		}

		return false;
	}

	// Spawns building frontage blocks using authored bounds rather than fixed 10m modular spacing.
	int32 SpawnBuildingFrontageVisualsForRegion(
		UWorld& world,
		const FScenarioGroundRegionSpec& regionSpec,
		const TArray<FScenarioCityBlockCatalogEntry>& blockEntries,
		TArray<FGeneratedBuildingFootprint>& inOutAcceptedFootprints,
		TArray<TObjectPtr<AActor>>& outSpawnedActors,
		int32& outSkippedOverlapCount,
		int32& outSpawnedCollisionProxyCount,
		const FScenarioCityBlockMaterializationOptions& options)
	{
		outSkippedOverlapCount = 0;
		outSpawnedCollisionProxyCount = 0;
		TArray<FGeneratedBuildingFrontageSpan> frontageSpans;
		if (!BuildBuildingFrontageSpansForRegion(regionSpec, frontageSpans))
		{
			UE_LOG(
				LogScenarioCityBlockMaterializer,
				Warning,
				TEXT("%s generated building frontage skipped for region '%s' because no valid frontage span could be resolved."),
				*options.LogContext,
				*regionSpec.RegionId);
			return 0;
		}

		double maxSpanLengthCm = 0.0;
		for (const FGeneratedBuildingFrontageSpan& frontageSpan : frontageSpans)
		{
			maxSpanLengthCm = FMath::Max(
				maxSpanLengthCm,
				FVector::Dist2D(frontageSpan.StartCm, frontageSpan.EndCm));
		}

		TArray<FGeneratedBuildingFrontageCandidate> buildingCandidates;
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

			if (blockLengthCm > maxSpanLengthCm + KINDA_SMALL_NUMBER)
			{
				continue;
			}

			UClass* blockClass = LoadCityBlockActorClass(blockEntry, regionSpec.RegionId, options);
			if (!blockClass)
			{
				continue;
			}

			FGeneratedBuildingFrontageCandidate candidate;
			candidate.BlockEntry = blockEntry;
			candidate.BlockClass = blockClass;
			candidate.LengthCm = blockLengthCm;
			candidate.HalfWidthCm = blockWidthCm * 0.5;
			buildingCandidates.Add(candidate);
		}

		if (buildingCandidates.IsEmpty())
		{
			UE_LOG(
				LogScenarioCityBlockMaterializer,
				Warning,
				TEXT("%s generated building frontage skipped for region '%s' because no building block fits the longest frontage span %.2f cm."),
				*options.LogContext,
				*regionSpec.RegionId,
				maxSpanLengthCm);
			return 0;
		}

		int32 nextCandidateIndex = 0;
		int32 spawnedActorCount = 0;
		for (const FGeneratedBuildingFrontageSpan& frontageSpan : frontageSpans)
		{
			const double spanLengthCm = FVector::Dist2D(frontageSpan.StartCm, frontageSpan.EndCm);
			const FVector forward = Normalize2DAxis(frontageSpan.EndCm - frontageSpan.StartCm);
			const FVector outward = Normalize2DAxis(frontageSpan.Outward);
			if (spanLengthCm <= KINDA_SMALL_NUMBER || forward.IsNearlyZero() || outward.IsNearlyZero())
			{
				continue;
			}

			TArray<int32> plannedCandidateIndices;
			double plannedLengthCm = 0.0;
			int32 spanNextCandidateIndex = nextCandidateIndex;
			while (plannedLengthCm < spanLengthCm - KINDA_SMALL_NUMBER)
			{
				bool bFoundFittingCandidate = false;
				for (int32 candidateAttemptIndex = 0;
					candidateAttemptIndex < buildingCandidates.Num();
					++candidateAttemptIndex)
				{
					const int32 candidateIndex =
						(spanNextCandidateIndex + candidateAttemptIndex) % buildingCandidates.Num();
					const FGeneratedBuildingFrontageCandidate& candidate =
						buildingCandidates[candidateIndex];
					const double leadingGapCm = plannedCandidateIndices.IsEmpty()
						? 0.0
						: GeneratedBuildingInterBlockGapCm;
					if (plannedLengthCm + leadingGapCm + candidate.LengthCm > spanLengthCm + KINDA_SMALL_NUMBER)
					{
						continue;
					}

					plannedCandidateIndices.Add(candidateIndex);
					plannedLengthCm += leadingGapCm + candidate.LengthCm;
					spanNextCandidateIndex = (candidateIndex + 1) % buildingCandidates.Num();
					bFoundFittingCandidate = true;
					break;
				}

				if (!bFoundFittingCandidate)
				{
					break;
				}
			}

			if (plannedCandidateIndices.IsEmpty())
			{
				continue;
			}

			nextCandidateIndex = spanNextCandidateIndex;
			const FRotator blockRotation(
				0.0,
				FMath::RadiansToDegrees(FMath::Atan2(forward.Y, forward.X)),
				0.0);
			double alongCursorCm = (spanLengthCm - plannedLengthCm) * 0.5;

			for (int32 blockIndex = 0; blockIndex < plannedCandidateIndices.Num(); ++blockIndex)
			{
				const FGeneratedBuildingFrontageCandidate& candidate =
					buildingCandidates[plannedCandidateIndices[blockIndex]];
				if (blockIndex > 0)
				{
					alongCursorCm += GeneratedBuildingInterBlockGapCm;
				}
				const double alongOffsetCm = alongCursorCm + (candidate.LengthCm * 0.5);
				alongCursorCm += candidate.LengthCm;
				const double outwardOffsetCm =
					candidate.HalfWidthCm + (candidate.BlockEntry.PlacementProfile.LateralOffsetMeters * 100.0);
				const FVector desiredBoundsCenter =
					frontageSpan.StartCm
					+ (forward * alongOffsetCm)
					+ (outward * outwardOffsetCm);

				FGeneratedBuildingFootprint candidateFootprint;
				candidateFootprint.CenterCm = desiredBoundsCenter;
				candidateFootprint.Forward = forward;
				candidateFootprint.Outward = outward;
				candidateFootprint.HalfLengthCm = candidate.LengthCm * 0.5;
				candidateFootprint.HalfWidthCm = candidate.HalfWidthCm;
				candidateFootprint.DebugSourceId = FString::Printf(
					TEXT("%s#%d"),
					*frontageSpan.DebugSourceId,
					blockIndex);
				const FGeneratedBuildingFootprint spacingFootprint = MakeBuildingSpacingFootprint(
					candidateFootprint,
					GeneratedBuildingInterBlockGapCm);

				bool bOverlapsAcceptedFootprint = false;
				for (const FGeneratedBuildingFootprint& acceptedFootprint : inOutAcceptedFootprints)
				{
					if (DoBuildingFootprintsOverlap2D(spacingFootprint, acceptedFootprint))
					{
						bOverlapsAcceptedFootprint = true;
						UE_LOG(
							LogScenarioCityBlockMaterializer,
							Verbose,
							TEXT("%s generated building block '%s' skipped for '%s' because it overlaps accepted footprint '%s'."),
							*options.LogContext,
							*candidate.BlockEntry.BlockId.ToString(),
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

				AActor* spawnedActor = SpawnCityBlockActorAtBoundsCenter(
					world,
					*candidate.BlockClass,
					candidate.BlockEntry,
					blockRotation,
					desiredBoundsCenter,
					candidateFootprint.DebugSourceId,
					outSpawnedActors,
					options);
				if (spawnedActor)
				{
					if (options.bCreateBuildingCollisionProxies)
					{
						const int32 blockingComponentCount =
							CountCityBlockBuildingNavigationBlockingComponents(*spawnedActor);
						if (blockingComponentCount > 0)
						{
							outSpawnedCollisionProxyCount += blockingComponentCount;
						}
						else if (AttachBuildingCollisionProxy(*spawnedActor, candidateFootprint, blockRotation))
						{
							++outSpawnedCollisionProxyCount;
						}
					}

					inOutAcceptedFootprints.Add(spacingFootprint);
					++spawnedActorCount;
				}
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

		const double reservedStartGapCm = straightGapCm && straightGapCm->bHasStartGap
			? FMath::Max(0.0, straightGapCm->StartGapCm)
			: 0.0;
		const double reservedEndGapCm = straightGapCm && straightGapCm->bHasEndGap
			? FMath::Max(0.0, straightGapCm->EndGapCm)
			: 0.0;
		const double usableStartCm = (-regionSpec.Size.X * 0.5) + reservedStartGapCm;
		const double usableEndCm = (regionSpec.Size.X * 0.5) - reservedEndGapCm;
		const double usableLengthCm = usableEndCm - usableStartCm;
		const FRotator blockRotation(0.0, regionSpec.YawDegrees, 0.0);
		const FVector forward = blockRotation.RotateVector(FVector::ForwardVector);

		int32 blockCount = 0;
		double firstAlongOffsetCm = 0.0;
		const bool bHasReservedGap = straightGapCm
			&& (straightGapCm->bHasStartGap || straightGapCm->bHasEndGap);
		if (bHasReservedGap)
		{
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
			UE_LOG(
				LogScenarioCityBlockMaterializer,
				Log,
				TEXT(
					"%s RoadSideCorner spawn | Key: %s, BlockId: %s, Role: %s, BP: %s, "
					"BoundsMeters(L=%.3f W=%.3f H=%.3f), CenterOffsetMeters=(%.3f, %.3f, %.3f), "
					"AnchorCm=(%.2f, %.2f, %.2f), BoundsCenterCm=(%.2f, %.2f, %.2f), YawDeg: %.2f"),
				*options.LogContext,
				*cornerPlacement.DebugKey,
				*blockEntry.BlockId.ToString(),
				CityBlockRoleToString(blockEntry.Role),
				*blockEntry.BPClass.ToSoftObjectPath().ToString(),
				blockEntry.BoundsMeters.LengthMeters,
				blockEntry.BoundsMeters.WidthMeters,
				blockEntry.BoundsMeters.HeightMeters,
				blockEntry.BoundsMeters.CenterOffsetMeters.X,
				blockEntry.BoundsMeters.CenterOffsetMeters.Y,
				blockEntry.BoundsMeters.CenterOffsetMeters.Z,
				cornerPlacement.AnchorLocationCm.X,
				cornerPlacement.AnchorLocationCm.Y,
				cornerPlacement.AnchorLocationCm.Z,
				desiredBoundsCenter.X,
				desiredBoundsCenter.Y,
				desiredBoundsCenter.Z,
				cornerPlacement.YawDegrees);
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

	TSet<FString> roadSideCompositeCandidateKeys;
	BuildRoadSideCompositeCandidateKeys(*catalog, groundRegions, roadSideCompositeCandidateKeys);
	result.RoadSideCompositeCandidateCount = roadSideCompositeCandidateKeys.Num();

	TArray<FGeneratedRoadSideCornerPlacement> roadSideCornerPlacements;
	TMap<FString, FGeneratedRegionStraightGapCm> roadSideCornerGapsCm;
	BuildRoadSideCornerPlacements(
		groundRegions,
		nullptr,
		GeneratedCornerStraightReserveGapCm,
		roadSideCornerPlacements,
		roadSideCornerGapsCm);

	FScenarioCityBlockCatalogEntry roadSideCornerBlockEntry;
	const bool bHasRoadSideCornerEntry = roadSideCornerPlacements.IsEmpty()
		? false
		: FindCityBlockEntryForRoadSideCorner(*catalog, roadSideCornerBlockEntry);
	if (!roadSideCornerPlacements.IsEmpty())
	{
		if (bHasRoadSideCornerEntry)
		{
			UE_LOG(
				LogScenarioCityBlockMaterializer,
				Log,
				TEXT(
					"%s selected road-side corner entry | BlockId: %s, Role: %s, BP: %s, "
					"Priority: %d, BoundsMeters(L=%.3f W=%.3f H=%.3f), CenterOffsetMeters=(%.3f, %.3f, %.3f), "
					"SurfaceIds: %d"),
				*options.LogContext,
				*roadSideCornerBlockEntry.BlockId.ToString(),
				CityBlockRoleToString(roadSideCornerBlockEntry.Role),
				*roadSideCornerBlockEntry.BPClass.ToSoftObjectPath().ToString(),
				roadSideCornerBlockEntry.PlacementProfile.Priority,
				roadSideCornerBlockEntry.BoundsMeters.LengthMeters,
				roadSideCornerBlockEntry.BoundsMeters.WidthMeters,
				roadSideCornerBlockEntry.BoundsMeters.HeightMeters,
				roadSideCornerBlockEntry.BoundsMeters.CenterOffsetMeters.X,
				roadSideCornerBlockEntry.BoundsMeters.CenterOffsetMeters.Y,
				roadSideCornerBlockEntry.BoundsMeters.CenterOffsetMeters.Z,
				roadSideCornerBlockEntry.SemanticProfile.SurfaceIds.Num());
		}
		else
		{
			UE_LOG(
				LogScenarioCityBlockMaterializer,
				Log,
				TEXT("%s no road-side corner entry matched %d inferred corner placement(s). Catalog: %s"),
				*options.LogContext,
				roadSideCornerPlacements.Num(),
				*options.CatalogDebugName);
		}
	}
	if (bHasRoadSideCornerEntry)
	{
		BuildRoadSideCornerPlacements(
			groundRegions,
			&roadSideCornerBlockEntry,
			ResolveRoadSideCornerStraightReserveGapCm(roadSideCornerBlockEntry),
			roadSideCornerPlacements,
			roadSideCornerGapsCm);
	}
	result.CornerCandidateCount = roadSideCornerPlacements.Num();
	const TMap<FString, FGeneratedRegionStraightGapCm>* activeRoadSideCornerGapsCm =
		bHasRoadSideCornerEntry ? &roadSideCornerGapsCm : nullptr;
	TArray<FGeneratedBuildingFootprint> acceptedBuildingFootprints;
	TArray<FGeneratedRoadStraightSplinePiece> roadStraightSplinePieces;

	for (const FScenarioGroundRegionSpec& regionSpec : groundRegions)
	{
		TArray<EScenarioCityBlockRole> roles;
		ResolveCityBlockRolesForRegion(regionSpec, roles);
		if (!IsGeneratedCityVisualRegion(regionSpec) || roles.IsEmpty())
		{
			continue;
		}

		++result.CandidateRegionCount;
		if (IsGeneratedBuildingFrontageSourceRegion(regionSpec))
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
			int32 spawnedCollisionProxyCount = 0;
			result.SpawnedActorCount += SpawnBuildingFrontageVisualsForRegion(
				*world,
				regionSpec,
				buildingBlockEntries,
				acceptedBuildingFootprints,
				outSpawnedActors,
				skippedOverlapCount,
				spawnedCollisionProxyCount,
				options);
			result.SkippedBuildingOverlapCount += skippedOverlapCount;
			result.SpawnedBuildingCollisionProxyCount += spawnedCollisionProxyCount;
			if (outSpawnedActors.Num() == beforeSpawnCount && skippedOverlapCount == 0)
			{
				++result.SkippedSpawnFailureCount;
			}
			continue;
		}

		FScenarioCityBlockCatalogEntry blockEntry;
		if (!FindCityBlockEntryForRegion(*catalog, regionSpec, blockEntry))
		{
			if (IsGeneratedRoadPenaltyRegion(regionSpec)
				&& !IsRoadSideCompositeCandidate(regionSpec, roadSideCompositeCandidateKeys))
			{
				++result.SkippedRoadSideCompositeNoEntryCount;
			}
			++result.SkippedNoEntryCount;
			continue;
		}

		const int32 beforeSpawnCount = outSpawnedActors.Num();
		const FGeneratedRegionStraightGapCm* straightGapCm = activeRoadSideCornerGapsCm
			? FindCornerGapForGeneratedRoadSideRegion(regionSpec, *activeRoadSideCornerGapsCm)
			: nullptr;
		if (IsGeneratedRoadPenaltyRegion(regionSpec)
			&& blockEntry.Role == EScenarioCityBlockRole::RoadStraight)
		{
			FGeneratedRoadStraightSplinePiece roadStraightSplinePiece;
			if (TryBuildRoadStraightSplinePiece(
				regionSpec,
				blockEntry,
				straightGapCm,
				roadStraightSplinePiece,
				options))
			{
				roadStraightSplinePieces.Add(MoveTemp(roadStraightSplinePiece));
			}
			else
			{
				++result.SkippedSpawnFailureCount;
			}
			continue;
		}

		const int32 spawnedRegionActorCount = SpawnCityBlockVisualsForRegion(
			*world,
			regionSpec,
			blockEntry,
			straightGapCm,
			outSpawnedActors,
			options);
		result.SpawnedActorCount += spawnedRegionActorCount;
		if (spawnedRegionActorCount > 0)
		{
			if (IsGeneratedRoadPenaltyRegion(regionSpec)
				&& blockEntry.Role == EScenarioCityBlockRole::RoadStraight)
			{
				result.SpawnedRoadSideCompositeCount += spawnedRegionActorCount;
			}
		}
		if (outSpawnedActors.Num() == beforeSpawnCount)
		{
			++result.SkippedSpawnFailureCount;
		}
	}

	if (!roadStraightSplinePieces.IsEmpty())
	{
		int32 skippedRoadSplineSpawnFailureCount = 0;
		const int32 spawnedRoadSplineActorCount = SpawnRoadStraightSplineVisuals(
			*world,
			roadStraightSplinePieces,
			outSpawnedActors,
			skippedRoadSplineSpawnFailureCount,
			options);
		result.SpawnedActorCount += spawnedRoadSplineActorCount;
		result.SpawnedRoadSideCompositeCount += spawnedRoadSplineActorCount;
		result.SkippedSpawnFailureCount += skippedRoadSplineSpawnFailureCount;
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
		|| result.RoadSideCompositeCandidateCount > 0
		|| result.SkippedNoEntryCount > 0
		|| result.SkippedRoadSideCompositeNoEntryCount > 0
		|| result.SkippedCornerNoEntryCount > 0
		|| result.SkippedSpawnFailureCount > 0
		|| result.SkippedCornerSpawnFailureCount > 0
		|| result.SpawnedRoadSideCompositeCount > 0
		|| result.SpawnedBuildingCollisionProxyCount > 0
		|| result.SkippedBuildingOverlapCount > 0)
	{
		UE_LOG(
			LogScenarioCityBlockMaterializer,
			Log,
			TEXT(
				"%s generated city block visuals complete | "
				"CandidateRegions: %d, SpawnedActors: %d, CornerCandidates: %d, "
				"RoadSideCompositeCandidates: %d, SkippedNoEntry: %d, "
				"SkippedRoadSideCompositeNoEntry: %d, SkippedCornerNoEntry: %d, "
				"SkippedSpawnFailure: %d, SkippedCornerSpawnFailure: %d, "
				"SpawnedRoadSideComposite: %d, "
				"SpawnedBuildingCollisionProxies: %d, SkippedBuildingOverlap: %d"),
			*options.LogContext,
			result.CandidateRegionCount,
			result.SpawnedActorCount,
			result.CornerCandidateCount,
			result.RoadSideCompositeCandidateCount,
			result.SkippedNoEntryCount,
			result.SkippedRoadSideCompositeNoEntryCount,
			result.SkippedCornerNoEntryCount,
			result.SkippedSpawnFailureCount,
			result.SkippedCornerSpawnFailureCount,
			result.SpawnedRoadSideCompositeCount,
			result.SpawnedBuildingCollisionProxyCount,
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
