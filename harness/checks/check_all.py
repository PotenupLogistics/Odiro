from __future__ import annotations

import json
from pathlib import Path

from harness.checks.check_api_shell import run_check as run_api_shell_check
from harness.checks.check_high_priority_review import run_check as run_high_priority_check
from harness.checks.check_handoff_response_summary import run_check as run_handoff_response_summary_check
from harness.checks.check_contract_validation import run_check as run_contract_validation_check
from harness.checks.check_generation_endpoint import run_check as run_generation_endpoint_check
from harness.checks.check_generic_obstacle_scenario import run_check as run_generic_obstacle_scenario_check
from harness.checks.check_episode_spec_adapter import run_check as run_episode_spec_adapter_check
from harness.checks.check_episode_spec_scenario_reflection import run_check as run_episode_spec_scenario_reflection_check
from harness.checks.check_ue5_episode_spec_handoff_smoke import run_check as run_ue5_episode_spec_handoff_smoke_check
from harness.checks.check_ue_team_handoff_package import run_check as run_ue_team_handoff_package_check
from harness.checks.check_project_handoff_readiness_docs import run_check as run_project_handoff_readiness_docs_check
from harness.checks.check_root_readme import run_check as run_root_readme_check
from harness.checks.check_handoff_release_readiness import run_check as run_handoff_release_readiness_check
from harness.checks.check_environment_parameter_spec import run_check as run_environment_parameter_spec_check
from harness.checks.check_environment_sampler import run_check as run_environment_sampler_check
from harness.checks.check_environment_sampler_generation_integration import run_check as run_environment_sampler_generation_integration_check
from harness.checks.check_environment_sampling_handoff_result_docs import run_check as run_environment_sampling_handoff_result_docs_check
from harness.checks.check_json_schemas import run_check as run_json_schemas_check
from harness.checks.check_llm_client_abstraction import run_check as run_llm_client_abstraction_check
from harness.checks.check_map_generation_data_sources_docs import run_check as run_map_generation_data_sources_docs_check
from harness.checks.check_llm_provider_config import run_check as run_llm_provider_config_check
from harness.checks.check_manual_confirmation import run_check as run_manual_confirmation_check
from harness.checks.check_manual_review_pack import run_check as run_manual_review_pack_check
from harness.checks.check_natural_language_plan import run_check as run_natural_language_plan_check
from harness.checks.check_ollama_failure_diagnostics import run_check as run_ollama_failure_diagnostics_check
from harness.checks.check_ollama_live_smoke_tooling import run_check as run_ollama_live_smoke_tooling_check
from harness.checks.check_ollama_provider import run_check as run_ollama_provider_check
from harness.checks.check_ollama_timeout_tuning import run_check as run_ollama_timeout_tuning_check
from harness.checks.check_openai_provider import run_check as run_openai_provider_check
from harness.checks.check_openai_first_handoff_docs import run_check as run_openai_first_handoff_docs_check
from harness.checks.check_page_hints import run_check as run_page_hints_check
from harness.checks.check_policy_candidates import run_check as run_candidate_check
from harness.checks.check_policy_cards import run_check as run_policy_cards_check
from harness.checks.check_policy_mapping_docs import run_check as run_policy_mapping_docs_check
from harness.checks.check_policy_triage import run_check as run_triage_check
from harness.checks.check_rag_chunks import run_check as run_rag_chunks_check
from harness.checks.check_rag_retrieval import run_check as run_rag_retrieval_check
from harness.checks.check_report_serialization import run_check as run_report_serialization_check
from harness.checks.check_route_relative_placement import run_check as run_route_relative_placement_check
from harness.checks.check_world_config_prompt_builder import run_check as run_world_config_prompt_builder_check
from harness.checks.check_world_config_generation_orchestrator import run_check as run_world_config_generation_orchestrator_check
from harness.checks.check_world_config_prompt_hardening import run_check as run_world_config_prompt_hardening_check
from harness.checks.check_world_config_output_contract import run_check as run_world_config_output_contract_check
from harness.checks.check_processed_sources import run_check as run_processed_check
from harness.checks.check_research_review import run_check as run_research_review_check
from harness.checks.check_research_sources import run_check as run_research_check
from harness.checks.check_review_readiness import run_check as run_review_check
from harness.checks.check_scenario_intent_and_reflection import run_check as run_scenario_intent_and_reflection_check
from harness.checks.check_scenario_post_processing import run_check as run_scenario_post_processing_check
from harness.checks.check_scenario_repair_prompt import run_check as run_scenario_repair_prompt_check
from harness.checks.check_sources import run_check as run_source_check
from harness.checks.check_ue5_handoff import run_check as run_ue5_handoff_check
from harness.checks.check_ue5_handoff_docs_and_export import run_check as run_ue5_handoff_docs_and_export_check
from harness.checks.check_ue_episode_spec_guide_alignment import run_check as run_ue_episode_spec_guide_alignment_check


ROOT = Path(__file__).resolve().parents[2]
REPORTS_DIR = ROOT / "harness" / "reports"
SOURCE_REPORT_PATH = REPORTS_DIR / "source_registry_report.json"
HARNESS_SUMMARY_JSON_PATH = REPORTS_DIR / "harness_summary.json"
SUMMARY_PATH = REPORTS_DIR / "harness_summary.md"


def _overall_status(
    source_result: dict,
    processed_result: dict,
    review_result: dict,
    candidate_result: dict,
    triage_result: dict,
    high_priority_result: dict,
    handoff_response_summary_result: dict,
    manual_confirmation_result: dict,
    manual_review_pack_result: dict,
    page_hints_result: dict,
    policy_cards_result: dict,
    policy_mapping_docs_result: dict,
    json_schemas_result: dict,
    contract_validation_result: dict,
    natural_language_plan_result: dict,
    world_config_prompt_builder_result: dict,
    world_config_generation_orchestrator_result: dict,
    world_config_prompt_hardening_result: dict,
    world_config_output_contract_result: dict,
    api_shell_result: dict,
    generation_endpoint_result: dict,
    generic_obstacle_scenario_result: dict,
    episode_spec_adapter_result: dict,
    episode_spec_scenario_reflection_result: dict,
    ue5_episode_spec_handoff_smoke_result: dict,
    ue_team_handoff_package_result: dict,
    project_handoff_readiness_docs_result: dict,
    root_readme_result: dict,
    handoff_release_readiness_result: dict,
    environment_parameter_spec_result: dict,
    environment_sampler_result: dict,
    environment_sampler_generation_integration_result: dict,
    environment_sampling_handoff_result_docs_result: dict,
    ue5_handoff_result: dict,
    ue5_handoff_docs_and_export_result: dict,
    ue_episode_spec_guide_alignment_result: dict,
    llm_client_abstraction_result: dict,
    map_generation_data_sources_docs_result: dict,
    llm_provider_config_result: dict,
    ollama_provider_result: dict,
    openai_provider_result: dict,
    openai_first_handoff_docs_result: dict,
    ollama_live_smoke_tooling_result: dict,
    ollama_failure_diagnostics_result: dict,
    ollama_timeout_tuning_result: dict,
    scenario_intent_and_reflection_result: dict,
    scenario_post_processing_result: dict,
    scenario_repair_prompt_result: dict,
    rag_chunks_result: dict,
    rag_retrieval_result: dict,
    report_serialization_result: dict,
    route_relative_placement_result: dict,
    research_result: dict,
    research_review_result: dict,
) -> str:
    if (
        not source_result["passed"]
        or not processed_result["passed"]
        or not review_result["passed"]
        or not candidate_result["passed"]
        or not triage_result["passed"]
        or not high_priority_result["passed"]
        or not handoff_response_summary_result["passed"]
        or not manual_confirmation_result["passed"]
        or not manual_review_pack_result["passed"]
        or not page_hints_result["passed"]
        or not policy_cards_result["passed"]
        or not policy_mapping_docs_result["passed"]
        or not json_schemas_result["passed"]
        or not contract_validation_result["passed"]
        or not natural_language_plan_result["passed"]
        or not world_config_prompt_builder_result["passed"]
        or not world_config_generation_orchestrator_result["passed"]
        or not world_config_prompt_hardening_result["passed"]
        or not world_config_output_contract_result["passed"]
        or not api_shell_result["passed"]
        or not generation_endpoint_result["passed"]
        or not generic_obstacle_scenario_result["passed"]
        or not episode_spec_adapter_result["passed"]
        or not episode_spec_scenario_reflection_result["passed"]
        or not ue5_episode_spec_handoff_smoke_result["passed"]
        or not ue_team_handoff_package_result["passed"]
        or not project_handoff_readiness_docs_result["passed"]
        or not root_readme_result["passed"]
        or not handoff_release_readiness_result["passed"]
        or not environment_parameter_spec_result["passed"]
        or not environment_sampler_result["passed"]
        or not environment_sampler_generation_integration_result["passed"]
        or not environment_sampling_handoff_result_docs_result["passed"]
        or not ue5_handoff_result["passed"]
        or not ue5_handoff_docs_and_export_result["passed"]
        or not ue_episode_spec_guide_alignment_result["passed"]
        or not llm_client_abstraction_result["passed"]
        or not map_generation_data_sources_docs_result["passed"]
        or not llm_provider_config_result["passed"]
        or not ollama_provider_result["passed"]
        or not openai_provider_result["passed"]
        or not openai_first_handoff_docs_result["passed"]
        or not ollama_live_smoke_tooling_result["passed"]
        or not ollama_failure_diagnostics_result["passed"]
        or not ollama_timeout_tuning_result["passed"]
        or not scenario_intent_and_reflection_result["passed"]
        or not scenario_post_processing_result["passed"]
        or not scenario_repair_prompt_result["passed"]
        or not rag_chunks_result["passed"]
        or not rag_retrieval_result["passed"]
        or not report_serialization_result["passed"]
        or not route_relative_placement_result["passed"]
        or not research_result["passed"]
        or not research_review_result["passed"]
    ):
        return "FAIL"
    if (
        processed_result.get("warning")
        or review_result.get("warning")
        or candidate_result.get("warning")
        or triage_result.get("warning")
        or high_priority_result.get("warning")
        or handoff_response_summary_result.get("warning")
        or manual_confirmation_result.get("warning")
        or manual_review_pack_result.get("warning")
        or page_hints_result.get("warning")
        or policy_cards_result.get("warning")
        or policy_mapping_docs_result.get("warning")
        or json_schemas_result.get("warning")
        or contract_validation_result.get("warning")
        or natural_language_plan_result.get("warning")
        or world_config_prompt_builder_result.get("warning")
        or world_config_generation_orchestrator_result.get("warning")
        or world_config_prompt_hardening_result.get("warning")
        or world_config_output_contract_result.get("warning")
        or api_shell_result.get("warning")
        or generation_endpoint_result.get("warning")
        or generic_obstacle_scenario_result.get("warning")
        or episode_spec_adapter_result.get("warning")
        or episode_spec_scenario_reflection_result.get("warning")
        or ue5_episode_spec_handoff_smoke_result.get("warning")
        or ue_team_handoff_package_result.get("warning")
        or project_handoff_readiness_docs_result.get("warning")
        or root_readme_result.get("warning")
        or handoff_release_readiness_result.get("warning")
        or environment_parameter_spec_result.get("warning")
        or environment_sampler_result.get("warning")
        or environment_sampler_generation_integration_result.get("warning")
        or environment_sampling_handoff_result_docs_result.get("warning")
        or ue5_handoff_result.get("warning")
        or ue5_handoff_docs_and_export_result.get("warning")
        or ue_episode_spec_guide_alignment_result.get("warning")
        or llm_client_abstraction_result.get("warning")
        or map_generation_data_sources_docs_result.get("warning")
        or llm_provider_config_result.get("warning")
        or ollama_provider_result.get("warning")
        or openai_provider_result.get("warning")
        or openai_first_handoff_docs_result.get("warning")
        or ollama_live_smoke_tooling_result.get("warning")
        or ollama_failure_diagnostics_result.get("warning")
        or ollama_timeout_tuning_result.get("warning")
        or scenario_intent_and_reflection_result.get("warning")
        or scenario_post_processing_result.get("warning")
        or scenario_repair_prompt_result.get("warning")
        or rag_chunks_result.get("warning")
        or rag_retrieval_result.get("warning")
        or report_serialization_result.get("warning")
        or route_relative_placement_result.get("warning")
        or research_result.get("warning")
        or research_review_result.get("warning")
    ):
        return "PASS_WITH_WARNING"
    return "PASS"


def build_summary(
    source_result: dict,
    processed_result: dict,
    review_result: dict,
    candidate_result: dict,
    triage_result: dict,
    high_priority_result: dict,
    handoff_response_summary_result: dict,
    manual_confirmation_result: dict,
    manual_review_pack_result: dict,
    page_hints_result: dict,
    policy_cards_result: dict,
    policy_mapping_docs_result: dict,
    json_schemas_result: dict,
    contract_validation_result: dict,
    natural_language_plan_result: dict,
    world_config_prompt_builder_result: dict,
    world_config_generation_orchestrator_result: dict,
    world_config_prompt_hardening_result: dict,
    world_config_output_contract_result: dict,
    api_shell_result: dict,
    generation_endpoint_result: dict,
    generic_obstacle_scenario_result: dict,
    episode_spec_adapter_result: dict,
    episode_spec_scenario_reflection_result: dict,
    ue5_episode_spec_handoff_smoke_result: dict,
    ue_team_handoff_package_result: dict,
    project_handoff_readiness_docs_result: dict,
    root_readme_result: dict,
    handoff_release_readiness_result: dict,
    environment_parameter_spec_result: dict,
    environment_sampler_result: dict,
    environment_sampler_generation_integration_result: dict,
    environment_sampling_handoff_result_docs_result: dict,
    ue5_handoff_result: dict,
    ue5_handoff_docs_and_export_result: dict,
    ue_episode_spec_guide_alignment_result: dict,
    llm_client_abstraction_result: dict,
    map_generation_data_sources_docs_result: dict,
    llm_provider_config_result: dict,
    ollama_provider_result: dict,
    openai_provider_result: dict,
    openai_first_handoff_docs_result: dict,
    ollama_live_smoke_tooling_result: dict,
    ollama_failure_diagnostics_result: dict,
    ollama_timeout_tuning_result: dict,
    scenario_intent_and_reflection_result: dict,
    scenario_post_processing_result: dict,
    scenario_repair_prompt_result: dict,
    rag_chunks_result: dict,
    rag_retrieval_result: dict,
    report_serialization_result: dict,
    route_relative_placement_result: dict,
    research_result: dict,
    research_review_result: dict,
    status_text: str,
) -> str:
    manual_review_sources = processed_result["manualReviewSources"]
    not_started_sources = review_result["notStartedSources"]
    source_candidate_counts = candidate_result["sourceCandidateCounts"]

    lines = [
        "# Harness Summary",
        "",
        f"- Overall result: {status_text}",
        f"- Source registry result: {'PASS' if source_result['passed'] else 'FAIL'}",
        f"- Processed source result: {'PASS' if processed_result['passed'] else 'FAIL'}",
        f"- Review readiness result: {'PASS' if review_result['passed'] else 'FAIL'}",
        f"- Policy candidate extraction result: {'PASS' if candidate_result['passed'] else 'FAIL'}",
        f"- Policy triage result: {'PASS' if triage_result['passed'] else 'FAIL'}",
        f"- High priority review result: {'PASS' if high_priority_result['passed'] else 'FAIL'}",
        f"- Handoff response summary result: {'PASS' if handoff_response_summary_result['passed'] else 'FAIL'}",
        f"- Manual confirmation result: {'PASS' if manual_confirmation_result['passed'] else 'FAIL'}",
        f"- Manual review pack result: {'PASS' if manual_review_pack_result['passed'] else 'FAIL'}",
        f"- Page hints result: {'PASS' if page_hints_result['passed'] else 'FAIL'}",
        f"- Policy cards result: {'PASS' if policy_cards_result['passed'] else 'FAIL'}",
        f"- Policy mapping docs result: {'PASS' if policy_mapping_docs_result['passed'] else 'FAIL'}",
        f"- JSON schemas result: {'PASS' if json_schemas_result['passed'] else 'FAIL'}",
        f"- Contract validation result: {'PASS' if contract_validation_result['passed'] else 'FAIL'}",
        f"- Natural language plan result: {'PASS' if natural_language_plan_result['passed'] else 'FAIL'}",
        f"- World Config prompt builder result: {'PASS' if world_config_prompt_builder_result['passed'] else 'FAIL'}",
        f"- World Config generation orchestrator result: {'PASS' if world_config_generation_orchestrator_result['passed'] else 'FAIL'}",
        f"- World Config prompt hardening result: {'PASS' if world_config_prompt_hardening_result['passed'] else 'FAIL'}",
        f"- World Config output contract result: {'PASS' if world_config_output_contract_result['passed'] else 'FAIL'}",
        f"- API shell result: {'PASS' if api_shell_result['passed'] else 'FAIL'}",
        f"- Generation endpoint result: {'PASS' if generation_endpoint_result['passed'] else 'FAIL'}",
        f"- Generic obstacle scenario result: {'PASS' if generic_obstacle_scenario_result['passed'] else 'FAIL'}",
        f"- EpisodeSpec adapter result: {'PASS' if episode_spec_adapter_result['passed'] else 'FAIL'}",
        f"- EpisodeSpec scenario reflection result: {'PASS' if episode_spec_scenario_reflection_result['passed'] else 'FAIL'}",
        f"- UE5 EpisodeSpec handoff smoke result: {'PASS' if ue5_episode_spec_handoff_smoke_result['passed'] else 'FAIL'}",
        f"- UE team handoff package result: {'PASS' if ue_team_handoff_package_result['passed'] else 'FAIL'}",
        f"- Project handoff readiness docs result: {'PASS' if project_handoff_readiness_docs_result['passed'] else 'FAIL'}",
        f"- Root README result: {'PASS' if root_readme_result['passed'] else 'FAIL'}",
        f"- Handoff release readiness result: {'PASS' if handoff_release_readiness_result['passed'] else 'FAIL'}",
        f"- Environment parameter spec result: {'PASS' if environment_parameter_spec_result['passed'] else 'FAIL'}",
        f"- Environment sampler result: {'PASS' if environment_sampler_result['passed'] else 'FAIL'}",
        f"- Environment sampler generation integration result: {'PASS' if environment_sampler_generation_integration_result['passed'] else 'FAIL'}",
        f"- Environment sampling handoff result docs result: {'PASS' if environment_sampling_handoff_result_docs_result['passed'] else 'FAIL'}",
        f"- UE5 handoff result: {'PASS' if ue5_handoff_result['passed'] else 'FAIL'}",
        f"- UE5 handoff docs/export result: {'PASS' if ue5_handoff_docs_and_export_result['passed'] else 'FAIL'}",
        f"- UE EpisodeSpec guide alignment result: {'PASS' if ue_episode_spec_guide_alignment_result['passed'] else 'FAIL'}",
        f"- LLM client abstraction result: {'PASS' if llm_client_abstraction_result['passed'] else 'FAIL'}",
        f"- Map generation data sources docs result: {'PASS' if map_generation_data_sources_docs_result['passed'] else 'FAIL'}",
        f"- LLM provider config result: {'PASS' if llm_provider_config_result['passed'] else 'FAIL'}",
        f"- Ollama provider result: {'PASS' if ollama_provider_result['passed'] else 'FAIL'}",
        f"- OpenAI provider result: {'PASS' if openai_provider_result['passed'] else 'FAIL'}",
        f"- OpenAI-first handoff docs result: {'PASS' if openai_first_handoff_docs_result['passed'] else 'FAIL'}",
        f"- Ollama live smoke tooling result: {'PASS' if ollama_live_smoke_tooling_result['passed'] else 'FAIL'}",
        f"- Ollama failure diagnostics result: {'PASS' if ollama_failure_diagnostics_result['passed'] else 'FAIL'}",
        f"- Ollama timeout tuning result: {'PASS' if ollama_timeout_tuning_result['passed'] else 'FAIL'}",
        f"- Scenario intent/reflection result: {'PASS' if scenario_intent_and_reflection_result['passed'] else 'FAIL'}",
        f"- Scenario post-processing result: {'PASS' if scenario_post_processing_result['passed'] else 'FAIL'}",
        f"- Scenario repair prompt result: {'PASS' if scenario_repair_prompt_result['passed'] else 'FAIL'}",
        f"- RAG chunks result: {'PASS' if rag_chunks_result['passed'] else 'FAIL'}",
        f"- RAG retrieval result: {'PASS' if rag_retrieval_result['passed'] else 'FAIL'}",
        f"- Report serialization result: {'PASS' if report_serialization_result['passed'] else 'FAIL'}",
        f"- Route-relative placement result: {'PASS' if route_relative_placement_result['passed'] else 'FAIL'}",
        f"- Research source result: {'PASS' if research_result['passed'] else 'FAIL'}",
        f"- Research review readiness result: {'PASS' if research_review_result['passed'] else 'FAIL'}",
        f"- Manual review pending sources: {len(not_started_sources)}",
        f"- Total policy candidates: {candidate_result['totalCandidateCount']}",
        "",
        "## Source Registry",
        "",
        f"- Total source count: {source_result['sourceCount']}",
        f"- Missing raw files: {len(source_result['missingFiles'])}",
        f"- Missing required field entries: {len(source_result['missingRequiredFields'])}",
        f"- Duplicate sourceIds: {len(source_result['duplicateSourceIds'])}",
        "",
        "## Processed Sources",
        "",
        f"- Processed source count: {processed_result['sourceCount']}",
        f"- Missing processed files: {len(processed_result['missingProcessedFiles'])}",
        f"- Manual review required: {len(manual_review_sources)}",
        "",
        "### Manual Review Required Sources",
    ]

    if manual_review_sources:
        lines.extend(
            [
                f"- {item['sourceId']}: {item['extractionStatus']}"
                for item in manual_review_sources
            ]
        )
    else:
        lines.append("- None")

    lines.extend(
        [
            "",
            "## Review Readiness",
            "",
            f"- Review status entries: {review_result['sourceCount']}",
            f"- Missing checklist files: {len(review_result['missingChecklistFiles'])}",
            f"- Sources waiting for manual review: {len(not_started_sources)}",
            "",
            "### Manual Review Queue",
        ]
    )

    if not_started_sources:
        lines.extend([f"- {source_id}: not_started" for source_id in not_started_sources])
    else:
        lines.append("- None")

    lines.extend(
        [
            "",
            "## Policy Candidate Extraction",
            "",
            f"- Candidate source count: {candidate_result['sourceCount']}",
            f"- Total candidate count: {candidate_result['totalCandidateCount']}",
            "- All candidate reviewStatus values must remain needs_pdf_check at this stage.",
            "",
            "### Candidate Counts By Source",
        ]
    )
    if source_candidate_counts:
        lines.extend(
            [
                f"- {source_id}: {source_candidate_counts[source_id]}"
                for source_id in sorted(source_candidate_counts)
            ]
        )
    else:
        lines.append("- None")

    lines.extend(
        [
            "",
            "## Research Sources",
            "",
            f"- KOR source count: {research_result['korSourceCount']}",
            f"- RSR source count: {research_result['rsrSourceCount']}",
            f"- RSR-001 PDF stored: {'yes' if research_result['rsr001PdfExists'] else 'no'}",
            f"- RSR-001 processed status: {research_review_result['rsr001ExtractionStatus'] or 'unknown'}",
            "",
            "### URL-only Sources",
        ]
    )
    if research_result["urlOnlySources"]:
        lines.extend([f"- {source_id}" for source_id in research_result["urlOnlySources"]])
    else:
        lines.append("- None")

    lines.extend(
        [
            "",
            "## High Priority Review",
            "",
            f"- High priority candidate count: {high_priority_result['totalHighPriorityCandidates']}",
            f"- Queue JSON exists: {'yes' if high_priority_result['queueJsonExists'] else 'no'}",
            f"- Queue Markdown exists: {'yes' if high_priority_result['queueMarkdownExists'] else 'no'}",
            "- All high priority manualReviewStatus values must remain pending_manual_confirmation.",
            "",
            "### Source Review Files",
        ]
    )
    for filename, exists in high_priority_result["sourceReviewFilesExist"].items():
        lines.append(f"- {filename}: {'exists' if exists else 'missing'}")

    lines.extend(
        [
            "",
            "## Manual Confirmation",
            "",
            f"- Total high priority candidate count: {manual_confirmation_result['totalItems']}",
            f"- Pending: {manual_confirmation_result['pendingCount']}",
            f"- Confirmed: {manual_confirmation_result['confirmedCount']}",
            f"- Rejected: {manual_confirmation_result['rejectedCount']}",
            "- CLI: uv run python scripts/manual_confirm.py summary",
            "- CLI: uv run python scripts/manual_confirm.py list --source KOR-003",
        ]
    )
    if manual_confirmation_result["confirmedCount"] == 0:
        lines.append("- No confirmed candidates yet. Next step is manual PDF review, not policy card generation.")

    lines.extend(
        [
            "",
            "## Manual Review Pack",
            "",
            f"- Pack directory exists: {'yes' if manual_review_pack_result['packDirExists'] else 'no'}",
            f"- Execution plan exists: {'yes' if manual_review_pack_result['executionPlanExists'] else 'no'}",
            f"- Input guide exists: {'yes' if manual_review_pack_result['inputGuideExists'] else 'no'}",
            f"- Pending: {manual_review_pack_result['pendingCount']}",
            f"- Confirmed: {manual_review_pack_result['confirmedCount']}",
            f"- Rejected: {manual_review_pack_result['rejectedCount']}",
            "",
            "### Source Pack Files",
        ]
    )
    for filename, exists in manual_review_pack_result["sourcePackFilesExist"].items():
        lines.append(f"- {filename}: {'exists' if exists else 'missing'}")

    lines.extend(
        [
            "",
            "## Page Hints",
            "",
            f"- Total high priority candidate count: {page_hints_result['totalCandidates']}",
            f"- Found: {page_hints_result['hintSummary'].get('found', 0)}",
            f"- Partial: {page_hints_result['hintSummary'].get('partial', 0)}",
            f"- Not found: {page_hints_result['hintSummary'].get('notFound', 0)}",
            f"- Needs manual page search: {page_hints_result['hintSummary'].get('needsManualPageSearch', 0)}",
            "",
            "### Source Page Hint Summary",
        ]
    )
    for source_id, counts in page_hints_result["sourceSummary"].items():
        lines.append(
            f"- {source_id}: found={counts.get('found', 0)}, partial={counts.get('partial', 0)}, not_found={counts.get('not_found', 0)}, needs_manual_page_search={counts.get('needs_manual_page_search', 0)}"
        )

    lines.extend(
        [
            "",
            "## Policy Knowledge Cards",
            "",
            f"- Confirmed candidate count: {policy_cards_result['confirmedCandidateCount']}",
            f"- Generated card count: {policy_cards_result['cardCount']}",
            f"- JSONL exists: {'yes' if policy_cards_result['policyCardsExists'] else 'no'}",
            "- Only confirmed manual confirmation items may become policy knowledge cards.",
        ]
    )
    if policy_cards_result["confirmedCandidateCount"] == 0:
        lines.append("- No confirmed candidates yet. Policy card generation is not expected.")
    elif policy_cards_result["cardCount"] == 0:
        lines.append("- Confirmed candidates exist, but policy cards have not been generated yet.")

    lines.extend(
        [
            "",
            "## Policy Mapping Docs",
            "",
            f"- Policy mapping docs result: {'PASS' if policy_mapping_docs_result['passed'] else 'FAIL'}",
            f"- Mapping card count: {policy_mapping_docs_result['cardCount']}",
            f"- Missing docs: {len(policy_mapping_docs_result['missingDocs'])}",
            f"- Missing reports: {len(policy_mapping_docs_result['missingReports'])}",
        ]
    )

    lines.extend(
        [
            "",
            "## JSON Schemas",
            "",
            f"- Schema count: {json_schemas_result['schemaCount']}",
            f"- Missing schemas: {len(json_schemas_result['missingSchemas'])}",
            f"- Missing model files: {len(json_schemas_result['missingModelFiles'])}",
            f"- Policy card count: {json_schemas_result['policyCardCount']}",
        ]
    )

    lines.extend(
        [
            "",
            "## Contract Validation",
            "",
            f"- Contract type count: {len(contract_validation_result['contractTypes'])}",
            f"- Missing contract types: {len(contract_validation_result['missingContractTypes'])}",
            f"- Missing schema mappings: {len(contract_validation_result['missingSchemaMappings'])}",
            f"- Missing model mappings: {len(contract_validation_result['missingModelMappings'])}",
        ]
    )

    lines.extend(
        [
            "",
            "## Natural Language Input Plan",
            "",
            f"- Natural language plan result: {'PASS' if natural_language_plan_result['passed'] else 'FAIL'}",
            f"- Missing docs: {len(natural_language_plan_result['missingDocs'])}",
            f"- Policy card count: {natural_language_plan_result['policyCardCount']}",
        ]
    )

    lines.extend(
        [
            "",
            "## World Config Prompt Builder",
            "",
            f"- World Config prompt builder result: {'PASS' if world_config_prompt_builder_result['passed'] else 'FAIL'}",
            f"- Policy RAG chunk count: {world_config_prompt_builder_result['chunkCount']}",
            f"- Schema referenced: {'yes' if world_config_prompt_builder_result['schemaReferenced'] else 'no'}",
            f"- Prompt package builds: {'yes' if world_config_prompt_builder_result['promptPackageBuilds'] else 'no'}",
        ]
    )

    lines.extend(
        [
            "",
            "## World Config Generation Orchestrator",
            "",
            f"- World Config generation orchestrator result: {'PASS' if world_config_generation_orchestrator_result['passed'] else 'FAIL'}",
            f"- Policy card count: {world_config_generation_orchestrator_result['policyCardCount']}",
            f"- Policy RAG chunk count: {world_config_generation_orchestrator_result['ragChunkCount']}",
            f"- Route set unchanged: {'yes' if world_config_generation_orchestrator_result['routeSetUnchanged'] else 'no'}",
            f"- External SDK imports: {len(world_config_generation_orchestrator_result['externalSdkImports'])}",
            f"- Hardcoded secret warnings: {len(world_config_generation_orchestrator_result['hardcodedSecretWarnings'])}",
        ]
    )

    lines.extend(
        [
            "",
            "## API Shell",
            "",
            f"- API shell result: {'PASS' if api_shell_result['passed'] else 'FAIL'}",
            f"- Missing routes: {len(api_shell_result['missingRoutes'])}",
            f"- Forbidden routes: {len(api_shell_result['forbiddenRoutes'])}",
            f"- LLM call warnings: {len(api_shell_result['llmCallWarnings'])}",
        ]
    )

    lines.extend(
        [
            "",
            "## Generation Endpoint",
            "",
            f"- Generation endpoint result: {'PASS' if generation_endpoint_result['passed'] else 'FAIL'}",
            f"- Missing routes: {len(generation_endpoint_result['missingRoutes'])}",
            f"- Policy card count: {generation_endpoint_result['policyCardCount']}",
            f"- Policy RAG chunk count: {generation_endpoint_result['ragChunkCount']}",
            f"- External SDK imports: {len(generation_endpoint_result['externalSdkImports'])}",
            f"- Hardcoded secret warnings: {len(generation_endpoint_result['hardcodedSecretWarnings'])}",
        ]
    )

    lines.extend(
        [
            "",
            "## LLM Client Abstraction",
            "",
            f"- LLM client abstraction result: {'PASS' if llm_client_abstraction_result['passed'] else 'FAIL'}",
            f"- Providers: {', '.join(llm_client_abstraction_result['providers'])}",
            f"- Route set unchanged: {'yes' if llm_client_abstraction_result['routeCountUnchanged'] else 'no'}",
            f"- External SDK imports: {len(llm_client_abstraction_result['externalSdkImports'])}",
            f"- Hardcoded secret warnings: {len(llm_client_abstraction_result['hardcodedSecretWarnings'])}",
        ]
    )

    lines.extend(
        [
            "",
            "## LLM Provider Config",
            "",
            f"- LLM provider config result: {'PASS' if llm_provider_config_result['passed'] else 'FAIL'}",
            f"- OpenAI key empty in example: {'yes' if llm_provider_config_result['openaiApiKeyEmptyInExample'] else 'no'}",
            f"- External SDK imports: {len(llm_provider_config_result['externalSdkImports'])}",
            f"- Hardcoded secret warnings: {len(llm_provider_config_result['hardcodedSecretWarnings'])}",
        ]
    )

    lines.extend(
        [
            "",
            "## Ollama Provider",
            "",
            f"- Ollama provider result: {'PASS' if ollama_provider_result['passed'] else 'FAIL'}",
            f"- Factory returns Ollama client: {'yes' if ollama_provider_result['factoryReturnsOllamaClient'] else 'no'}",
            f"- OpenAI imports/calls: {len(ollama_provider_result['externalOpenAiImports'])}",
            f"- Hardcoded secret warnings: {len(ollama_provider_result['hardcodedSecretWarnings'])}",
        ]
    )

    lines.extend(
        [
            "",
            "## RAG Chunks",
            "",
            f"- RAG chunks result: {'PASS' if rag_chunks_result['passed'] else 'FAIL'}",
            f"- Policy card count: {rag_chunks_result['cardCount']}",
            f"- RAG chunk count: {rag_chunks_result['chunkCount']}",
            f"- Duplicate chunkIds: {len(rag_chunks_result['duplicateChunkIds'])}",
        ]
    )

    lines.extend(
        [
            "",
            "## RAG Retrieval",
            "",
            f"- RAG retrieval result: {'PASS' if rag_retrieval_result['passed'] else 'FAIL'}",
            f"- Chunk count: {rag_retrieval_result['chunkCount']}",
            f"- Keyword search results: {rag_retrieval_result['keywordSearchResultCount']}",
            f"- Action search results: {rag_retrieval_result['actionSearchResultCount']}",
            f"- Category search results: {rag_retrieval_result['categorySearchResultCount']}",
            f"- Parameter search results: {rag_retrieval_result['paramSearchResultCount']}",
        ]
    )

    lines.extend(
        [
            "",
            "## Policy Candidate Triage",
            "",
            f"- Total candidate count: {triage_result['totalCandidates']}",
            f"- High priority count: {triage_result['byPriority'].get('high', 0)}",
            f"- Medium priority count: {triage_result['byPriority'].get('medium', 0)}",
            f"- Low priority count: {triage_result['byPriority'].get('low', 0)}",
            "- All triage reviewStatus values remain needs_pdf_check.",
            "",
            "### High Priority By Source",
        ]
    )
    if triage_result["highPriorityBySource"]:
        lines.extend(
            [
                f"- {source_id}: {count}"
                for source_id, count in triage_result["highPriorityBySource"].items()
            ]
        )
    else:
        lines.append("- None")

    lines.extend(
        [
            "",
            "## Research Review Readiness",
            "",
            f"- Research review status entries: {research_review_result['sourceCount']}",
            f"- local_pdf sources: {', '.join(research_review_result['localPdfSources']) if research_review_result['localPdfSources'] else 'None'}",
            f"- url_only sources: {', '.join(research_review_result['urlOnlySources']) if research_review_result['urlOnlySources'] else 'None'}",
            f"- RSR-001 processed Markdown exists: {'yes' if research_review_result['rsr001ProcessedExists'] else 'no'}",
        ]
    )

    lines.extend(
        [
            "",
            "## Next Steps",
            "",
            "1. 사람이 후보 문장을 원본 PDF와 대조한다.",
            "2. page hint를 참고해 원본 PDF를 확인한다.",
            "3. manual review pack을 보고 원본 PDF와 대조한다.",
            "4. manual_confirmation_results.json에서 confirmed/rejected를 직접 입력한다.",
            "5. 하네스를 재실행한다.",
            "6. confirmed 후보만 policy knowledge card 생성 대상으로 사용한다.",
            "7. research source도 processed Markdown 또는 summary note로 검토한다.",
            "8. RSR-001 원본 PDF와 processed Markdown을 수동 대조한다.",
            "9. RSR-002~RSR-006 원문 확보 여부를 결정한다.",
            "10. reviewed 처리 전에는 RAG에 반영하지 않는다.",
        ]
    )

    return "\n".join(lines) + "\n"


def main() -> int:
    REPORTS_DIR.mkdir(parents=True, exist_ok=True)

    source_result = run_source_check()
    processed_result = run_processed_check()
    review_result = run_review_check()
    candidate_result = run_candidate_check()
    triage_result = run_triage_check()
    high_priority_result = run_high_priority_check()
    handoff_response_summary_result = run_handoff_response_summary_check()
    manual_confirmation_result = run_manual_confirmation_check()
    manual_review_pack_result = run_manual_review_pack_check()
    page_hints_result = run_page_hints_check()
    policy_cards_result = run_policy_cards_check()
    policy_mapping_docs_result = run_policy_mapping_docs_check()
    json_schemas_result = run_json_schemas_check()
    contract_validation_result = run_contract_validation_check()
    natural_language_plan_result = run_natural_language_plan_check()
    world_config_prompt_builder_result = run_world_config_prompt_builder_check()
    world_config_generation_orchestrator_result = run_world_config_generation_orchestrator_check()
    world_config_prompt_hardening_result = run_world_config_prompt_hardening_check()
    world_config_output_contract_result = run_world_config_output_contract_check()
    api_shell_result = run_api_shell_check()
    generation_endpoint_result = run_generation_endpoint_check()
    generic_obstacle_scenario_result = run_generic_obstacle_scenario_check()
    episode_spec_adapter_result = run_episode_spec_adapter_check()
    episode_spec_scenario_reflection_result = run_episode_spec_scenario_reflection_check()
    ue5_episode_spec_handoff_smoke_result = run_ue5_episode_spec_handoff_smoke_check()
    ue_team_handoff_package_result = run_ue_team_handoff_package_check()
    project_handoff_readiness_docs_result = run_project_handoff_readiness_docs_check()
    root_readme_result = run_root_readme_check()
    handoff_release_readiness_result = run_handoff_release_readiness_check()
    environment_parameter_spec_result = run_environment_parameter_spec_check()
    environment_sampler_result = run_environment_sampler_check()
    environment_sampler_generation_integration_result = run_environment_sampler_generation_integration_check()
    environment_sampling_handoff_result_docs_result = run_environment_sampling_handoff_result_docs_check()
    ue5_handoff_result = run_ue5_handoff_check()
    ue5_handoff_docs_and_export_result = run_ue5_handoff_docs_and_export_check()
    ue_episode_spec_guide_alignment_result = run_ue_episode_spec_guide_alignment_check()
    llm_client_abstraction_result = run_llm_client_abstraction_check()
    map_generation_data_sources_docs_result = run_map_generation_data_sources_docs_check()
    llm_provider_config_result = run_llm_provider_config_check()
    ollama_provider_result = run_ollama_provider_check()
    openai_provider_result = run_openai_provider_check()
    openai_first_handoff_docs_result = run_openai_first_handoff_docs_check()
    ollama_live_smoke_tooling_result = run_ollama_live_smoke_tooling_check()
    ollama_failure_diagnostics_result = run_ollama_failure_diagnostics_check()
    ollama_timeout_tuning_result = run_ollama_timeout_tuning_check()
    scenario_intent_and_reflection_result = run_scenario_intent_and_reflection_check()
    scenario_post_processing_result = run_scenario_post_processing_check()
    scenario_repair_prompt_result = run_scenario_repair_prompt_check()
    rag_chunks_result = run_rag_chunks_check()
    rag_retrieval_result = run_rag_retrieval_check()
    report_serialization_result = run_report_serialization_check()
    route_relative_placement_result = run_route_relative_placement_check()
    research_result = run_research_check()
    research_review_result = run_research_review_check()
    status_text = _overall_status(
        source_result,
        processed_result,
        review_result,
        candidate_result,
        triage_result,
        high_priority_result,
        handoff_response_summary_result,
        manual_confirmation_result,
        manual_review_pack_result,
        page_hints_result,
        policy_cards_result,
        policy_mapping_docs_result,
        json_schemas_result,
        contract_validation_result,
        natural_language_plan_result,
        world_config_prompt_builder_result,
        world_config_generation_orchestrator_result,
        world_config_prompt_hardening_result,
        world_config_output_contract_result,
        api_shell_result,
        generation_endpoint_result,
        generic_obstacle_scenario_result,
        episode_spec_adapter_result,
        episode_spec_scenario_reflection_result,
        ue5_episode_spec_handoff_smoke_result,
        ue_team_handoff_package_result,
        project_handoff_readiness_docs_result,
        root_readme_result,
        handoff_release_readiness_result,
        environment_parameter_spec_result,
        environment_sampler_result,
        environment_sampler_generation_integration_result,
        environment_sampling_handoff_result_docs_result,
        ue5_handoff_result,
        ue5_handoff_docs_and_export_result,
        ue_episode_spec_guide_alignment_result,
        llm_client_abstraction_result,
        map_generation_data_sources_docs_result,
        llm_provider_config_result,
        ollama_provider_result,
        openai_provider_result,
        openai_first_handoff_docs_result,
        ollama_live_smoke_tooling_result,
        ollama_failure_diagnostics_result,
        ollama_timeout_tuning_result,
        scenario_intent_and_reflection_result,
        scenario_post_processing_result,
        scenario_repair_prompt_result,
        rag_chunks_result,
        rag_retrieval_result,
        report_serialization_result,
        route_relative_placement_result,
        research_result,
        research_review_result,
    )
    summary_result = {
        "status": status_text,
        "passed": status_text in {"PASS", "PASS_WITH_WARNING"},
        "checks": {
            "sources": source_result,
            "processedSources": processed_result,
            "reviewReadiness": review_result,
            "policyCandidates": candidate_result,
            "policyTriage": triage_result,
            "highPriorityReview": high_priority_result,
            "handoffResponseSummary": handoff_response_summary_result,
            "manualConfirmation": manual_confirmation_result,
            "manualReviewPack": manual_review_pack_result,
            "pageHints": page_hints_result,
            "policyCards": policy_cards_result,
            "policyMappingDocs": policy_mapping_docs_result,
            "jsonSchemas": json_schemas_result,
            "contractValidation": contract_validation_result,
            "naturalLanguagePlan": natural_language_plan_result,
            "worldConfigPromptBuilder": world_config_prompt_builder_result,
            "worldConfigGenerationOrchestrator": world_config_generation_orchestrator_result,
            "worldConfigPromptHardening": world_config_prompt_hardening_result,
            "worldConfigOutputContract": world_config_output_contract_result,
            "apiShell": api_shell_result,
            "generationEndpoint": generation_endpoint_result,
            "genericObstacleScenario": generic_obstacle_scenario_result,
            "episodeSpecAdapter": episode_spec_adapter_result,
            "episodeSpecScenarioReflection": episode_spec_scenario_reflection_result,
            "ue5EpisodeSpecHandoffSmoke": ue5_episode_spec_handoff_smoke_result,
            "ueTeamHandoffPackage": ue_team_handoff_package_result,
            "projectHandoffReadinessDocs": project_handoff_readiness_docs_result,
            "rootReadme": root_readme_result,
            "handoffReleaseReadiness": handoff_release_readiness_result,
            "environmentParameterSpec": environment_parameter_spec_result,
            "environmentSampler": environment_sampler_result,
            "environmentSamplerGenerationIntegration": environment_sampler_generation_integration_result,
            "environmentSamplingHandoffResultDocs": environment_sampling_handoff_result_docs_result,
            "ue5Handoff": ue5_handoff_result,
            "ue5HandoffDocsAndExport": ue5_handoff_docs_and_export_result,
            "ueEpisodeSpecGuideAlignment": ue_episode_spec_guide_alignment_result,
            "llmClientAbstraction": llm_client_abstraction_result,
            "mapGenerationDataSourcesDocs": map_generation_data_sources_docs_result,
            "llmProviderConfig": llm_provider_config_result,
            "ollamaProvider": ollama_provider_result,
            "openaiProvider": openai_provider_result,
            "openaiFirstHandoffDocs": openai_first_handoff_docs_result,
            "ollamaLiveSmokeTooling": ollama_live_smoke_tooling_result,
            "ollamaFailureDiagnostics": ollama_failure_diagnostics_result,
            "ollamaTimeoutTuning": ollama_timeout_tuning_result,
            "scenarioIntentAndReflection": scenario_intent_and_reflection_result,
            "scenarioPostProcessing": scenario_post_processing_result,
            "scenarioRepairPrompt": scenario_repair_prompt_result,
            "ragChunks": rag_chunks_result,
            "ragRetrieval": rag_retrieval_result,
            "reportSerialization": report_serialization_result,
            "routeRelativePlacement": route_relative_placement_result,
            "researchSources": research_result,
            "researchReview": research_review_result,
        },
    }

    SOURCE_REPORT_PATH.write_text(
        json.dumps(source_result, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8-sig",
    )
    HARNESS_SUMMARY_JSON_PATH.write_text(
        json.dumps(summary_result, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8-sig",
    )
    SUMMARY_PATH.write_text(
        build_summary(
            source_result,
            processed_result,
            review_result,
            candidate_result,
            triage_result,
            high_priority_result,
            handoff_response_summary_result,
            manual_confirmation_result,
            manual_review_pack_result,
            page_hints_result,
            policy_cards_result,
            policy_mapping_docs_result,
            json_schemas_result,
            contract_validation_result,
            natural_language_plan_result,
            world_config_prompt_builder_result,
            world_config_generation_orchestrator_result,
            world_config_prompt_hardening_result,
            world_config_output_contract_result,
            api_shell_result,
            generation_endpoint_result,
            generic_obstacle_scenario_result,
            episode_spec_adapter_result,
            episode_spec_scenario_reflection_result,
            ue5_episode_spec_handoff_smoke_result,
            ue_team_handoff_package_result,
            project_handoff_readiness_docs_result,
            root_readme_result,
            handoff_release_readiness_result,
            environment_parameter_spec_result,
            environment_sampler_result,
            environment_sampler_generation_integration_result,
            environment_sampling_handoff_result_docs_result,
            ue5_handoff_result,
            ue5_handoff_docs_and_export_result,
            ue_episode_spec_guide_alignment_result,
            llm_client_abstraction_result,
            map_generation_data_sources_docs_result,
            llm_provider_config_result,
            ollama_provider_result,
            openai_provider_result,
            openai_first_handoff_docs_result,
            ollama_live_smoke_tooling_result,
            ollama_failure_diagnostics_result,
            ollama_timeout_tuning_result,
            scenario_intent_and_reflection_result,
            scenario_post_processing_result,
            scenario_repair_prompt_result,
            rag_chunks_result,
            rag_retrieval_result,
            report_serialization_result,
            route_relative_placement_result,
            research_result,
            research_review_result,
            status_text,
        ),
        encoding="utf-8-sig",
    )

    print(f"Source registry check: {'PASS' if source_result['passed'] else 'FAIL'}")
    print(f"Processed source check: {'PASS' if processed_result['passed'] else 'FAIL'}")
    print(f"Review readiness check: {'PASS' if review_result['passed'] else 'FAIL'}")
    print(f"Policy candidate check: {'PASS' if candidate_result['passed'] else 'FAIL'}")
    print(f"Policy triage check: {'PASS' if triage_result['passed'] else 'FAIL'}")
    print(f"High priority review check: {'PASS' if high_priority_result['passed'] else 'FAIL'}")
    print(f"Handoff response summary check: {'PASS' if handoff_response_summary_result['passed'] else 'FAIL'}")
    print(f"Manual confirmation check: {'PASS' if manual_confirmation_result['passed'] else 'FAIL'}")
    print(f"Manual review pack check: {'PASS' if manual_review_pack_result['passed'] else 'FAIL'}")
    print(f"Page hints check: {'PASS' if page_hints_result['passed'] else 'FAIL'}")
    print(f"Policy cards check: {'PASS' if policy_cards_result['passed'] else 'FAIL'}")
    print(f"Policy mapping docs check: {'PASS' if policy_mapping_docs_result['passed'] else 'FAIL'}")
    print(f"JSON schemas check: {'PASS' if json_schemas_result['passed'] else 'FAIL'}")
    print(f"Contract validation check: {'PASS' if contract_validation_result['passed'] else 'FAIL'}")
    print(f"Natural language plan check: {'PASS' if natural_language_plan_result['passed'] else 'FAIL'}")
    print(f"World Config prompt builder check: {'PASS' if world_config_prompt_builder_result['passed'] else 'FAIL'}")
    print(f"World Config generation orchestrator check: {'PASS' if world_config_generation_orchestrator_result['passed'] else 'FAIL'}")
    print(f"World Config prompt hardening check: {'PASS' if world_config_prompt_hardening_result['passed'] else 'FAIL'}")
    print(f"World Config output contract check: {'PASS' if world_config_output_contract_result['passed'] else 'FAIL'}")
    print(f"API shell check: {'PASS' if api_shell_result['passed'] else 'FAIL'}")
    print(f"Generation endpoint check: {'PASS' if generation_endpoint_result['passed'] else 'FAIL'}")
    print(f"Generic obstacle scenario check: {'PASS' if generic_obstacle_scenario_result['passed'] else 'FAIL'}")
    print(f"EpisodeSpec adapter check: {'PASS' if episode_spec_adapter_result['passed'] else 'FAIL'}")
    print(f"EpisodeSpec scenario reflection check: {'PASS' if episode_spec_scenario_reflection_result['passed'] else 'FAIL'}")
    print(f"UE5 EpisodeSpec handoff smoke check: {'PASS' if ue5_episode_spec_handoff_smoke_result['passed'] else 'FAIL'}")
    print(f"UE team handoff package check: {'PASS' if ue_team_handoff_package_result['passed'] else 'FAIL'}")
    print(f"Project handoff readiness docs check: {'PASS' if project_handoff_readiness_docs_result['passed'] else 'FAIL'}")
    print(f"Root README check: {'PASS' if root_readme_result['passed'] else 'FAIL'}")
    print(f"Handoff release readiness check: {'PASS' if handoff_release_readiness_result['passed'] else 'FAIL'}")
    print(f"Environment parameter spec check: {'PASS' if environment_parameter_spec_result['passed'] else 'FAIL'}")
    print(f"Environment sampler check: {'PASS' if environment_sampler_result['passed'] else 'FAIL'}")
    print(f"Environment sampler generation integration check: {'PASS' if environment_sampler_generation_integration_result['passed'] else 'FAIL'}")
    print(f"Environment sampling handoff result docs check: {'PASS' if environment_sampling_handoff_result_docs_result['passed'] else 'FAIL'}")
    print(f"UE5 handoff check: {'PASS' if ue5_handoff_result['passed'] else 'FAIL'}")
    print(f"UE5 handoff docs/export check: {'PASS' if ue5_handoff_docs_and_export_result['passed'] else 'FAIL'}")
    print(f"UE EpisodeSpec guide alignment check: {'PASS' if ue_episode_spec_guide_alignment_result['passed'] else 'FAIL'}")
    print(f"LLM client abstraction check: {'PASS' if llm_client_abstraction_result['passed'] else 'FAIL'}")
    print(f"Map generation data sources docs check: {'PASS' if map_generation_data_sources_docs_result['passed'] else 'FAIL'}")
    print(f"LLM provider config check: {'PASS' if llm_provider_config_result['passed'] else 'FAIL'}")
    print(f"Ollama provider check: {'PASS' if ollama_provider_result['passed'] else 'FAIL'}")
    print(f"OpenAI provider check: {'PASS' if openai_provider_result['passed'] else 'FAIL'}")
    print(f"OpenAI-first handoff docs check: {'PASS' if openai_first_handoff_docs_result['passed'] else 'FAIL'}")
    print(f"Ollama live smoke tooling check: {'PASS' if ollama_live_smoke_tooling_result['passed'] else 'FAIL'}")
    print(f"Ollama failure diagnostics check: {'PASS' if ollama_failure_diagnostics_result['passed'] else 'FAIL'}")
    print(f"Ollama timeout tuning check: {'PASS' if ollama_timeout_tuning_result['passed'] else 'FAIL'}")
    print(f"Scenario intent/reflection check: {'PASS' if scenario_intent_and_reflection_result['passed'] else 'FAIL'}")
    print(f"Scenario post-processing check: {'PASS' if scenario_post_processing_result['passed'] else 'FAIL'}")
    print(f"Scenario repair prompt check: {'PASS' if scenario_repair_prompt_result['passed'] else 'FAIL'}")
    print(f"RAG chunks check: {'PASS' if rag_chunks_result['passed'] else 'FAIL'}")
    print(f"RAG retrieval check: {'PASS' if rag_retrieval_result['passed'] else 'FAIL'}")
    print(f"Report serialization check: {'PASS' if report_serialization_result['passed'] else 'FAIL'}")
    print(f"Route-relative placement check: {'PASS' if route_relative_placement_result['passed'] else 'FAIL'}")
    print(f"Research source check: {'PASS' if research_result['passed'] else 'FAIL'}")
    print(f"Research review check: {'PASS' if research_review_result['passed'] else 'FAIL'}")
    if processed_result.get("warning"):
        print("Processed source warning: manual review required")
    if review_result.get("warning"):
        print("Review readiness warning: sources are waiting for manual review")
    if candidate_result.get("warning"):
        print("Policy candidate warning: candidate extraction requires review")
    if triage_result.get("warning"):
        print("Policy triage warning: triage requires manual review")
    if high_priority_result.get("warning"):
        print("High priority review warning: manual confirmation required")
    if handoff_response_summary_result.get("warning"):
        print("Handoff response summary warning: smoke reporting artifacts require attention")
    if manual_confirmation_result.get("warning"):
        print("Manual confirmation warning: review state requires attention")
    if manual_review_pack_result.get("warning"):
        print("Manual review pack warning: review pack requires attention")
    if page_hints_result.get("warning"):
        print("Page hints warning: hints require manual PDF verification")
    if policy_cards_result.get("warning"):
        print("Policy cards warning: confirmed candidates need policy card generation")
    if policy_mapping_docs_result.get("warning"):
        print("Policy mapping docs warning: mapping docs require attention")
    if json_schemas_result.get("warning"):
        print("JSON schemas warning: schema/model artifacts require attention")
    if contract_validation_result.get("warning"):
        print("Contract validation warning: validation layer requires attention")
    if natural_language_plan_result.get("warning"):
        print("Natural language plan warning: natural language docs require attention")
    if world_config_prompt_builder_result.get("warning"):
        print("World Config prompt builder warning: prompt builder artifacts require attention")
    if world_config_generation_orchestrator_result.get("warning"):
        print("World Config generation orchestrator warning: orchestrator artifacts require attention")
    if world_config_prompt_hardening_result.get("warning"):
        print("World Config prompt hardening warning: prompt hardening artifacts require attention")
    if world_config_output_contract_result.get("warning"):
        print("World Config output contract warning: output contract artifacts require attention")
    if api_shell_result.get("warning"):
        print("API shell warning: API shell artifacts require attention")
    if generation_endpoint_result.get("warning"):
        print("Generation endpoint warning: endpoint artifacts require attention")
    if generic_obstacle_scenario_result.get("warning"):
        print("Generic obstacle scenario warning: scenario handling artifacts require attention")
    if episode_spec_adapter_result.get("warning"):
        print("EpisodeSpec adapter warning: adapter artifacts require attention")
    if episode_spec_scenario_reflection_result.get("warning"):
        print("EpisodeSpec scenario reflection warning: reflection artifacts require attention")
    if ue5_episode_spec_handoff_smoke_result.get("warning"):
        print("UE5 EpisodeSpec handoff smoke warning: smoke report requires attention")
    if ue_team_handoff_package_result.get("warning"):
        print("UE team handoff package warning: docs require attention")
    if project_handoff_readiness_docs_result.get("warning"):
        print("Project handoff readiness docs warning: docs require attention")
    if root_readme_result.get("warning"):
        print("Root README warning: README entrypoint requires attention")
    if handoff_release_readiness_result.get("warning"):
        print("Handoff release readiness warning: release handoff docs require attention")
    if environment_parameter_spec_result.get("warning"):
        print("Environment parameter spec warning: parameter docs require attention")
    if environment_sampler_result.get("warning"):
        print("Environment sampler warning: sampler artifacts require attention")
    if environment_sampler_generation_integration_result.get("warning"):
        print("Environment sampler generation integration warning: integration artifacts require attention")
    if environment_sampling_handoff_result_docs_result.get("warning"):
        print("Environment sampling handoff result docs warning: result docs require attention")
    if ue5_handoff_result.get("warning"):
        print("UE5 handoff warning: handoff artifacts require attention")
    if ue5_handoff_docs_and_export_result.get("warning"):
        print("UE5 handoff docs/export warning: docs or export artifacts require attention")
    if ue_episode_spec_guide_alignment_result.get("warning"):
        print("UE EpisodeSpec guide alignment warning: guide alignment artifacts require attention")
    if llm_client_abstraction_result.get("warning"):
        print("LLM client abstraction warning: LLM abstraction artifacts require attention")
    if map_generation_data_sources_docs_result.get("warning"):
        print("Map generation data sources docs warning: data source docs require attention")
    if llm_provider_config_result.get("warning"):
        print("LLM provider config warning: provider config artifacts require attention")
    if ollama_provider_result.get("warning"):
        print("Ollama provider warning: Ollama provider artifacts require attention")
    if openai_provider_result.get("warning"):
        print("OpenAI provider warning: OpenAI provider artifacts require attention")
    if openai_first_handoff_docs_result.get("warning"):
        print("OpenAI-first handoff docs warning: handoff result docs require attention")
    if ollama_live_smoke_tooling_result.get("warning"):
        print("Ollama live smoke tooling warning: manual smoke tooling requires attention")
    if ollama_failure_diagnostics_result.get("warning"):
        print("Ollama failure diagnostics warning: diagnostic artifacts require attention")
    if ollama_timeout_tuning_result.get("warning"):
        print("Ollama timeout tuning warning: timeout tuning artifacts require attention")
    if scenario_intent_and_reflection_result.get("warning"):
        print("Scenario intent/reflection warning: scenario semantic validation artifacts require attention")
    if scenario_post_processing_result.get("warning"):
        print("Scenario post-processing warning: scenario post-processing artifacts require attention")
    if scenario_repair_prompt_result.get("warning"):
        print("Scenario repair prompt warning: scenario repair artifacts require attention")
    if rag_chunks_result.get("warning"):
        print("RAG chunks warning: chunk artifacts require attention")
    if rag_retrieval_result.get("warning"):
        print("RAG retrieval warning: retrieval layer requires attention")
    if report_serialization_result.get("warning"):
        print("Report serialization warning: report serialization artifacts require attention")
    if route_relative_placement_result.get("warning"):
        print("Route-relative placement warning: route placement artifacts require attention")
    if research_result.get("warning"):
        print("Research source warning: research source collection requires review")
    if research_review_result.get("warning"):
        print("Research review warning: research review requires follow-up")
    print(f"Harness summary: {status_text}")
    print(f"Report written to: {SOURCE_REPORT_PATH}")
    print(f"Summary written to: {SUMMARY_PATH}")

    return 0 if summary_result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
