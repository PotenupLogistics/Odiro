from __future__ import annotations

import json
from pathlib import Path

from app.agents.common.llm_json_client import AgentLlmClient, AgentLlmJsonClient
from app.core.settings import Settings
from app.agents.scenario_generation_v2.intent_parser import IntentParser
from app.agents.scenario_generation_v2.repair_handler import RepairHandler
from app.agents.scenario_generation_v2.request_normalizer import RequestNormalizer
from app.agents.scenario_generation_v2.response_builder import ResponseBuilder
from app.agents.scenario_generation_v2.scenario_type_selector import ScenarioTypeSelector
from app.agents.scenario_generation_v2.template_json_writer import TemplateJsonWriter
from app.agents.scenario_generation_v2.template_planner import TemplatePlanner
from app.agents.scenario_generation_v2.template_validator import TemplateValidator
from app.models.scenario_generation_v2 import ScenarioGenerateV2Request, ScenarioGenerateV2Response, V2ValidationIssue


class ScenarioGenerationV2Agent:
    def __init__(
        self,
        *,
        settings: Settings | None = None,
        llm_client: AgentLlmClient | None = None,
    ) -> None:
        self.settings = settings or Settings()
        self.llm_client = llm_client
        self.normalizer = RequestNormalizer()
        self.intent_parser = IntentParser()
        self.type_selector = ScenarioTypeSelector()
        self.planner = TemplatePlanner()
        self.writer = TemplateJsonWriter()
        self.validator = TemplateValidator()
        self.repair_handler = RepairHandler()
        self.response_builder = ResponseBuilder()

    def generate(self, request: ScenarioGenerateV2Request) -> ScenarioGenerateV2Response:
        normalized = self.normalizer.normalize(request.prompt)
        intent = self.intent_parser.parse(normalized.normalized_prompt)
        scenario_type = self.type_selector.select(intent)
        plan = self.planner.plan(intent, scenario_type)

        if self.settings.v2AgentLlmEnabled:
            response = self._generate_with_llm(normalized.normalized_prompt, plan.summary, plan.assumptions)
            if response is not None:
                return response
            return self._generate_deterministic(
                plan,
                generation_mode="fallback",
                fallback_warning=V2ValidationIssue(
                    field="scenario_template",
                    message="LLM output validation failed; deterministic fallback template was used.",
                ),
            )

        return self._generate_deterministic(plan, generation_mode="deterministic")

    def _generate_deterministic(
        self,
        plan,
        *,
        generation_mode: str,
        fallback_warning: V2ValidationIssue | None = None,
    ) -> ScenarioGenerateV2Response:
        template = self.writer.write(plan)
        template = self.repair_handler.repair(template)
        validation = self.validator.validate(template)
        if fallback_warning is not None:
            validation.warnings.append(fallback_warning)

        if not validation.valid:
            return self.response_builder.failed(
                summary="시나리오 템플릿 생성에 실패했습니다.",
                validation=validation,
            )

        return self.response_builder.success(
            scenario_id=template["scenario_id"],
            summary=plan.summary,
            scenario_template=template,
            validation=validation,
            assumptions=plan.assumptions,
            generation_mode=generation_mode,
        )

    def _generate_with_llm(
        self,
        prompt: str,
        fallback_summary: str,
        assumptions: list[str],
    ) -> ScenarioGenerateV2Response | None:
        client = self.llm_client or AgentLlmJsonClient(settings=self.settings)
        try:
            template = client.generate_json(
                system_prompt=self._read_prompt("system_prompt.md"),
                user_prompt=self._template_user_prompt(prompt),
                response_name="scenario_template_v2",
            )
            validation = self.validator.validate(template)
            if validation.valid:
                return self.response_builder.success(
                    scenario_id=template["scenario_id"],
                    summary=self._summary_from_template(template, fallback_summary),
                    scenario_template=template,
                    validation=validation,
                    assumptions=assumptions,
                    generation_mode="llm",
                )
        except Exception:
            validation = None
            template = None

        if self.settings.v2AgentLlmRepairEnabled and self.settings.v2AgentLlmMaxRepairAttempts > 0:
            try:
                repaired = client.generate_json(
                    system_prompt=self._read_prompt("system_prompt.md"),
                    user_prompt=self._repair_user_prompt(prompt, template, validation),
                    response_name="scenario_template_v2_repair",
                )
                repaired_validation = self.validator.validate(repaired)
                if repaired_validation.valid:
                    return self.response_builder.success(
                        scenario_id=repaired["scenario_id"],
                        summary=self._summary_from_template(repaired, fallback_summary),
                        scenario_template=repaired,
                        validation=repaired_validation,
                        assumptions=assumptions,
                        generation_mode="llm_repaired",
                    )
            except Exception:
                return None
        return None

    def _template_user_prompt(self, prompt: str) -> str:
        return "\n\n".join(
            [
                self._read_prompt("template_writer_prompt.md"),
                "scenario.template.json과 scenario.json은 같은 구조를 공유하지만 template은 범위값을 가질 수 있다.",
                "실행용 scenario sample, seed, 실행 개수, RunQueue는 생성하지 않는다.",
                "최소 필드: schema 또는 version, scenario_id, intent.summary, ground_model, robot, 장애물 또는 보행자 조건.",
                f"사용자 prompt:\n{prompt}",
            ]
        )

    def _repair_user_prompt(self, prompt: str, template: object, validation: object) -> str:
        return "\n\n".join(
            [
                self._read_prompt("repair_prompt.md"),
                f"사용자 prompt:\n{prompt}",
                f"검증 결과:\n{validation}",
                f"수정 대상 JSON:\n{json.dumps(template, ensure_ascii=False, default=str)}",
            ]
        )

    def _read_prompt(self, filename: str) -> str:
        return (Path(__file__).parent / "prompts" / filename).read_text(encoding="utf-8")

    def _summary_from_template(self, template: dict, fallback: str) -> str:
        summary = template.get("summary")
        if isinstance(summary, str) and summary:
            return summary
        intent = template.get("intent")
        if isinstance(intent, dict) and isinstance(intent.get("summary"), str) and intent["summary"]:
            return intent["summary"]
        return fallback
