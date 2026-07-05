#include "DeliveryBot/DeliveryBotLidarRayPattern.h"

namespace
{
	const FName OusterOS1SensorModelName(TEXT("Ouster OS1"));
	constexpr int32 OusterOS1ChannelCount = 64;
	constexpr float OusterOS1VerticalFovDegree = 45.0f;

	// Local validated scan parameters shared by all pattern builders.
	struct FNormalizedLidarPatternConfig
	{
		// Minimum positive horizontal spacing in degrees.
		float AngleStepDegree = 1.0f;

		// Minimum positive vertical spacing in degrees.
		float VerticalStepDegree = 1.0f;

		// Inclusive lower pitch bound in degrees.
		float VerticalMinDegree = -10.0f;

		// Inclusive upper pitch bound in degrees.
		float VerticalMaxDegree = 10.0f;
	};

	FNormalizedLidarPatternConfig NormalizePatternConfig(const FDeliveryBotLidarSensorConfigInfo& Config)
	{
		FNormalizedLidarPatternConfig NormalizedConfig;
		NormalizedConfig.AngleStepDegree = FMath::Max(Config.AngleStepDegree, 1.0f);
		NormalizedConfig.VerticalStepDegree = FMath::Max(Config.VerticalStepDegree, 1.0f);
		NormalizedConfig.VerticalMinDegree = FMath::Clamp(Config.VerticalMinDegree, -89.0f, 89.0f);
		NormalizedConfig.VerticalMaxDegree = FMath::Clamp(Config.VerticalMaxDegree, -89.0f, 89.0f);

		if (NormalizedConfig.VerticalMinDegree > NormalizedConfig.VerticalMaxDegree)
		{
			Swap(NormalizedConfig.VerticalMinDegree, NormalizedConfig.VerticalMaxDegree);
		}

		return NormalizedConfig;
	}

	FDeliveryBotLidarRaySample MakeRaySample(
		const int32 RayIndex,
		const float YawDegree,
		const float PitchDegree,
		const EDeliveryBotLidarRayDimensionType DimensionType,
		const int32 ChannelIndex = INDEX_NONE,
		const int32 ColumnIndex = INDEX_NONE,
		const float RelativeTimeSeconds = 0.0f,
		const FName SensorModel = NAME_None)
	{
		FDeliveryBotLidarRaySample Sample;
		Sample.RayIndex = RayIndex;
		Sample.YawDegree = YawDegree;
		Sample.PitchDegree = PitchDegree;
		Sample.DimensionType = DimensionType;
		Sample.LocalDirection = FRotator(PitchDegree, YawDegree, 0.0f).Vector();
		Sample.ChannelIndex = ChannelIndex;
		Sample.ColumnIndex = ColumnIndex;
		Sample.RelativeTimeSeconds = RelativeTimeSeconds;
		Sample.SensorModel = SensorModel;
		return Sample;
	}

	void AppendOusterOS1RaySamples(
		const FDeliveryBotLidarSensorConfigInfo& Config,
		TArray<FDeliveryBotLidarRaySample>& OutSamples)
	{
		const int32 ColumnCount = FDeliveryBotLidarRayPattern::CountYawSamples(Config);
		const int32 ChannelCount = FDeliveryBotLidarRayPattern::GetOusterOS1ChannelCount();
		const float ScanRateHz = FMath::Max(Config.ScanRateHz, 0.1f);
		const float HalfVerticalFovDegree = FDeliveryBotLidarRayPattern::GetOusterOS1VerticalFovDegree() * 0.5f;

		int32 RayIndex = 0;
		for (int32 ColumnIndex = 0; ColumnIndex < ColumnCount; ++ColumnIndex)
		{
			const float YawDegree = static_cast<float>(ColumnIndex) * 360.0f / static_cast<float>(ColumnCount);
			const float RelativeTimeSeconds =
				static_cast<float>(ColumnIndex) / static_cast<float>(ColumnCount) / ScanRateHz;

			for (int32 ChannelIndex = 0; ChannelIndex < ChannelCount; ++ChannelIndex)
			{
				const float ChannelAlpha = ChannelCount > 1
					? static_cast<float>(ChannelIndex) / static_cast<float>(ChannelCount - 1)
					: 0.5f;
				const float PitchDegree = FMath::Lerp(-HalfVerticalFovDegree, HalfVerticalFovDegree, ChannelAlpha);
				OutSamples.Add(MakeRaySample(
					RayIndex,
					YawDegree,
					PitchDegree,
					EDeliveryBotLidarRayDimensionType::ThreeD,
					ChannelIndex,
					ColumnIndex,
					RelativeTimeSeconds,
					OusterOS1SensorModelName));
				++RayIndex;
			}
		}
	}
}

void FDeliveryBotLidarRayPattern::BuildRaySamples(
	const FDeliveryBotLidarSensorConfigInfo& Config,
	TArray<FDeliveryBotLidarRaySample>& OutSamples)
{
	OutSamples.Reset();

	if (DoesModeIncludeDimension(Config.LidarModeType, EDeliveryBotLidarRayDimensionType::OneD))
	{
		AppendRaySamplesForDimension(Config, EDeliveryBotLidarRayDimensionType::OneD, OutSamples);
	}
	if (DoesModeIncludeDimension(Config.LidarModeType, EDeliveryBotLidarRayDimensionType::TwoD))
	{
		AppendRaySamplesForDimension(Config, EDeliveryBotLidarRayDimensionType::TwoD, OutSamples);
	}
	if (DoesModeIncludeDimension(Config.LidarModeType, EDeliveryBotLidarRayDimensionType::ThreeD))
	{
		AppendRaySamplesForDimension(Config, EDeliveryBotLidarRayDimensionType::ThreeD, OutSamples);
	}
}

void FDeliveryBotLidarRayPattern::AppendRaySamplesForDimension(
	const FDeliveryBotLidarSensorConfigInfo& Config,
	const EDeliveryBotLidarRayDimensionType DimensionType,
	TArray<FDeliveryBotLidarRaySample>& OutSamples)
{
	const FNormalizedLidarPatternConfig NormalizedConfig = NormalizePatternConfig(Config);

	switch (DimensionType)
	{
	case EDeliveryBotLidarRayDimensionType::OneD:
		OutSamples.Add(MakeRaySample(0, 0.0f, 0.0f, DimensionType));
		return;

	case EDeliveryBotLidarRayDimensionType::TwoD:
	{
		int32 RayIndex = 0;
		for (float YawDegree = 0.0f; YawDegree < 360.0f; YawDegree += NormalizedConfig.AngleStepDegree)
		{
			OutSamples.Add(MakeRaySample(RayIndex, YawDegree, 0.0f, DimensionType));
			++RayIndex;
		}
		return;
	}

	case EDeliveryBotLidarRayDimensionType::ThreeD:
	{
		if (FDeliveryBotLidarRayPattern::IsOusterOS1Mode(Config.LidarModeType))
		{
			AppendOusterOS1RaySamples(Config, OutSamples);
			return;
		}

		int32 RayIndex = 0;
		for (float PitchDegree = NormalizedConfig.VerticalMinDegree;
			PitchDegree <= NormalizedConfig.VerticalMaxDegree;
			PitchDegree += NormalizedConfig.VerticalStepDegree)
		{
			for (float YawDegree = 0.0f; YawDegree < 360.0f; YawDegree += NormalizedConfig.AngleStepDegree)
			{
				OutSamples.Add(MakeRaySample(RayIndex, YawDegree, PitchDegree, DimensionType));
				++RayIndex;
			}
		}
		return;
	}

	default:
		return;
	}
}

bool FDeliveryBotLidarRayPattern::DoesModeIncludeDimension(
	const EDeliveryBotLidarModeType Mode,
	const EDeliveryBotLidarRayDimensionType DimensionType)
{
	switch (DimensionType)
	{
	case EDeliveryBotLidarRayDimensionType::OneD:
		return Mode == EDeliveryBotLidarModeType::OneD
			|| Mode == EDeliveryBotLidarModeType::OneDAndTwoD
			|| Mode == EDeliveryBotLidarModeType::All;

	case EDeliveryBotLidarRayDimensionType::TwoD:
		return Mode == EDeliveryBotLidarModeType::TwoD
			|| Mode == EDeliveryBotLidarModeType::OneDAndTwoD
			|| Mode == EDeliveryBotLidarModeType::TwoDAndThreeD
			|| Mode == EDeliveryBotLidarModeType::All;

	case EDeliveryBotLidarRayDimensionType::ThreeD:
		return Mode == EDeliveryBotLidarModeType::ThreeD
			|| Mode == EDeliveryBotLidarModeType::TwoDAndThreeD
			|| Mode == EDeliveryBotLidarModeType::All
			|| Mode == EDeliveryBotLidarModeType::OusterOS1;

	default:
		return false;
	}
}

bool FDeliveryBotLidarRayPattern::IsOusterOS1Mode(const EDeliveryBotLidarModeType Mode)
{
	return Mode == EDeliveryBotLidarModeType::OusterOS1;
}

FName FDeliveryBotLidarRayPattern::GetSensorModelName(const EDeliveryBotLidarModeType Mode)
{
	switch (Mode)
	{
	case EDeliveryBotLidarModeType::OusterOS1:
		return OusterOS1SensorModelName;
	default:
		return NAME_None;
	}
}

int32 FDeliveryBotLidarRayPattern::CountYawSamples(const FDeliveryBotLidarSensorConfigInfo& Config)
{
	const FNormalizedLidarPatternConfig NormalizedConfig = NormalizePatternConfig(Config);

	int32 RayCount = 0;
	for (float YawDegree = 0.0f; YawDegree < 360.0f; YawDegree += NormalizedConfig.AngleStepDegree)
	{
		++RayCount;
	}

	return FMath::Max(1, RayCount);
}

int32 FDeliveryBotLidarRayPattern::CountPitchSamples(const FDeliveryBotLidarSensorConfigInfo& Config)
{
	if (IsOusterOS1Mode(Config.LidarModeType))
	{
		return GetOusterOS1ChannelCount();
	}

	const FNormalizedLidarPatternConfig NormalizedConfig = NormalizePatternConfig(Config);

	int32 PitchCount = 0;
	for (float PitchDegree = NormalizedConfig.VerticalMinDegree;
		PitchDegree <= NormalizedConfig.VerticalMaxDegree;
		PitchDegree += NormalizedConfig.VerticalStepDegree)
	{
		++PitchCount;
	}

	return FMath::Max(1, PitchCount);
}

int32 FDeliveryBotLidarRayPattern::GetOusterOS1ChannelCount()
{
	return OusterOS1ChannelCount;
}

float FDeliveryBotLidarRayPattern::GetOusterOS1VerticalFovDegree()
{
	return OusterOS1VerticalFovDegree;
}

float FDeliveryBotLidarRayPattern::NormalizeSignedYawDegree(const float YawDegree)
{
	return FMath::UnwindDegrees(YawDegree);
}

bool FDeliveryBotLidarRayPattern::IsFrontYaw(
	const float YawDegree,
	const float FrontHalfAngleDegree)
{
	const float SignedYawDegree = FMath::Abs(NormalizeSignedYawDegree(YawDegree));
	const float ClampedFrontHalfAngleDegree = FMath::Clamp(FrontHalfAngleDegree, 0.0f, 180.0f);
	return SignedYawDegree <= ClampedFrontHalfAngleDegree;
}
