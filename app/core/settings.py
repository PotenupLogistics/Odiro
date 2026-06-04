from __future__ import annotations

from typing import Annotated

from pydantic import AliasChoices, Field, field_validator
from pydantic_settings import BaseSettings, NoDecode, SettingsConfigDict

from app.models.llm import LlmProvider


class Settings(BaseSettings):
    model_config = SettingsConfigDict(
        env_file=".env",
        env_file_encoding="utf-8",
        extra="ignore",
        populate_by_name=True,
    )

    llmProvider: LlmProvider = Field(
        default=LlmProvider.openai,
        validation_alias=AliasChoices("LLM_PROVIDER", "llmProvider"),
    )
    llmProviderChain: Annotated[list[str], NoDecode] = Field(
        default_factory=lambda: ["openai", "ollama"],
        validation_alias=AliasChoices("LLM_PROVIDER_CHAIN", "llmProviderChain"),
    )
    openaiApiKey: str = Field(
        default="",
        validation_alias=AliasChoices("OPENAI_API_KEY", "openaiApiKey"),
    )
    openaiModel: str = Field(
        default="gpt-4o-mini",
        validation_alias=AliasChoices("OPENAI_MODEL", "openaiModel"),
    )
    openaiTimeoutSec: int = Field(
        default=30,
        validation_alias=AliasChoices("OPENAI_TIMEOUT_SEC", "openaiTimeoutSec"),
    )
    openaiMaxTokens: int = Field(
        default=1200,
        validation_alias=AliasChoices("OPENAI_MAX_TOKENS", "openaiMaxTokens"),
    )
    openaiTemperature: float = Field(
        default=0.1,
        validation_alias=AliasChoices("OPENAI_TEMPERATURE", "openaiTemperature"),
    )
    openaiMaxRepairAttempts: int = Field(
        default=1,
        validation_alias=AliasChoices("OPENAI_MAX_REPAIR_ATTEMPTS", "openaiMaxRepairAttempts"),
    )
    openaiDailyRequestLimit: int = Field(
        default=30,
        validation_alias=AliasChoices("OPENAI_DAILY_REQUEST_LIMIT", "openaiDailyRequestLimit"),
    )
    ollamaBaseUrl: str = Field(
        default="http://localhost:11434",
        validation_alias=AliasChoices("OLLAMA_BASE_URL", "ollamaBaseUrl"),
    )
    ollamaModel: str = Field(
        default="llama3.1:8b",
        validation_alias=AliasChoices("OLLAMA_MODEL", "ollamaModel"),
    )
    ollamaTimeoutSec: int = Field(
        default=180,
        validation_alias=AliasChoices("OLLAMA_TIMEOUT_SEC", "ollamaTimeoutSec"),
    )
    ollamaMaxTokens: int = Field(
        default=1200,
        validation_alias=AliasChoices("OLLAMA_MAX_TOKENS", "ollamaMaxTokens"),
    )
    ollamaTemperature: float = Field(
        default=0.1,
        validation_alias=AliasChoices("OLLAMA_TEMPERATURE", "ollamaTemperature"),
    )
    llmAllowOpenaiFallback: bool = Field(
        default=True,
        validation_alias=AliasChoices("LLM_ALLOW_OPENAI_FALLBACK", "llmAllowOpenaiFallback"),
    )
    llmMaxTotalAttempts: int = Field(
        default=3,
        validation_alias=AliasChoices("LLM_MAX_TOTAL_ATTEMPTS", "llmMaxTotalAttempts"),
    )
    scenarioEpisodeDefaultCount: int = Field(
        default=5,
        validation_alias=AliasChoices("SCENARIO_EPISODE_DEFAULT_COUNT", "scenarioEpisodeDefaultCount"),
    )
    scenarioEpisodeMaxCount: int = Field(
        default=20,
        validation_alias=AliasChoices("SCENARIO_EPISODE_MAX_COUNT", "scenarioEpisodeMaxCount"),
    )
    runQueueExportBaseDir: str = Field(
        default="data/run_queue_exports",
        validation_alias=AliasChoices("RUN_QUEUE_EXPORT_BASE_DIR", "runQueueExportBaseDir"),
    )

    @field_validator("llmProviderChain", mode="before")
    @classmethod
    def parse_provider_chain(cls, value: object) -> list[str]:
        if isinstance(value, str):
            return [item.strip() for item in value.split(",") if item.strip()]
        if isinstance(value, list):
            return [str(item) for item in value]
        return ["openai", "ollama"]
