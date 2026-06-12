# Episode Logging

Covers: `Source/ProtoRobotSim/Public/Episode`, `Source/ProtoRobotSim/Private/Episode`, episode-scoped types in `Shared`

"Episode" = one completed simulation iteration and its result artifacts. The old scenario-wide meaning was renamed; environment authoring/compile/spawn now lives in [Scenario](scenario.md).

## Entry Points
- `Public/Episode/EpisodeMeasurementLogSubsystem.h`: per-world measurement log lifecycle (header/tick/event/footer JSONL records)
- `Public/Episode/EpisodeRobotMeasurementAdapter.h`: robot state/lidar/action snapshot adapter feeding tick records
- `Shared/EpisodeMeasurementLogTypes.h`: log record structs and settings
- `Shared/EpisodeJsonlMeasurementWriter.h`: JSONL file writer
- `Shared/EpisodeLogSubjectRegistry.h`: subject id registry for log records
- `Shared/EpisodeEvaluationReportJson.h`: `episode_evaluation_report` JSON build/save
- `Shared/EpisodeConfigTypes.h`: episode evaluation outcome/event/result enums and structs, `FEpisodeRunRecord`

## Notes
- These types keep the `Episode` prefix intentionally under the new terminology (per-iteration artifacts)
- External contracts that must not be renamed unilaterally: report schema `episode_evaluation_report`, policy-server `episode_start`/config-update protocol (`DeliveryBot` HTTP policy components), AI analysis request fields
