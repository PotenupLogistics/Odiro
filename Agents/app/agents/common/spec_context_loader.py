from __future__ import annotations

from collections.abc import Iterable
from pathlib import Path, PurePosixPath, PureWindowsPath

# Ordered markdown files shared by v2 Agent LLM prompt context.
SPEC_CONTEXT_BASE_ALLOWLIST: tuple[str, ...] = (
    "docs/specs/simulation-interface.md",
    "contracts/specs/user-project-data.md",
    "Agents/docs/api/V2_AGENT_APIS.md",
    "Agents/docs/agents/V2_AGENT_ARCHITECTURE.md",
    "Agents/docs/agents/V2_LANGGRAPH_DESIGN.md",
)

# Ordered markdown files approved for common v2 Agent LLM prompt context.
SPEC_CONTEXT_ALLOWLIST: tuple[str, ...] = (
    *SPEC_CONTEXT_BASE_ALLOWLIST,
    "Client/Json/environment-catalog.md",
)

# Ordered markdown files approved for scenario generation authoring prompts.
SCENARIO_GENERATION_SPEC_CONTEXT_ALLOWLIST: tuple[str, ...] = (
    "docs/specs/simulation-interface.md",
    "Agents/docs/api/V2_AGENT_APIS.md",
    "Agents/docs/agents/V2_AGENT_ARCHITECTURE.md",
    "Agents/docs/agents/V2_LANGGRAPH_DESIGN.md",
    "Client/Json/Schema/scenario.json.md",
    "Client/Json/environment-catalog.md",
)

# Prompt-level precedence notice injected before the loaded spec bundle.
SPEC_CONTEXT_PRIORITY_NOTICE = "\n".join(
    [
        "공식 문서 우선 적용 안내",
        "1. SPEC_CONTEXT의 공식 문서 내용을 최우선 기준으로 적용한다.",
        "2. API/schema/validator 요구사항은 SPEC_CONTEXT 다음 우선순위로 적용한다.",
        "3. 기존 bundled prompt는 위 기준을 보조하는 지침으로만 사용한다.",
        "4. 사용자 자연어 요청은 공식 문서와 validator 경계 안에서 해석한다.",
        "SPEC_CONTEXT와 기존 bundled prompt가 충돌하면 SPEC_CONTEXT를 우선 적용한다.",
    ]
)


class SpecContextError(RuntimeError):
    """Reports an invalid or unavailable v2 Agent spec context source."""


class SpecContextLoader:
    """Loads allowlisted markdown specs for v2 Agent LLM user prompts."""

    def __init__(self, *, repo_root: Path | None = None, allowlist: Iterable[str] | None = None) -> None:
        """Keep prompt context ownership explicit and independent from agent logic."""
        # Repository root used to resolve allowlisted relative paths.
        self.repo_root = (repo_root or self._discover_repo_root()).resolve()
        # Ordered markdown files that are safe to expose to v2 LLM calls.
        self.allowlist = tuple(allowlist or SPEC_CONTEXT_ALLOWLIST)
        # Cached prompt block avoids repeated filesystem reads inside one agent instance.
        self._cached_prompt_block: str | None = None

    def build_prompt_block(self) -> str:
        """Return the priority notice plus the wrapped spec context block."""
        if self._cached_prompt_block is None:
            self._cached_prompt_block = f"{SPEC_CONTEXT_PRIORITY_NOTICE}\n\n{self.load_context()}"
        return self._cached_prompt_block

    def load_context(self) -> str:
        """Read every allowlisted markdown file and wrap it with source markers."""
        sections: list[str] = []
        for relative_path in self.allowlist:
            file_path = self._resolve_allowlisted_path(relative_path)
            if not file_path.is_file():
                raise SpecContextError(f"Spec context file not found: {relative_path}")
            content = file_path.read_text(encoding="utf-8-sig").strip()
            sections.append(f"# SPEC FILE: {relative_path}\n\n{content}")
        return "\n\n".join(["<SPEC_CONTEXT>", *sections, "</SPEC_CONTEXT>"])

    def _resolve_allowlisted_path(self, relative_path: str) -> Path:
        """Resolve a safe repository-relative allowlist entry."""
        if not relative_path.strip():
            raise SpecContextError("Spec context allowlist entry must not be empty.")
        posix_path = PurePosixPath(relative_path)
        windows_path = PureWindowsPath(relative_path)
        if posix_path.is_absolute() or windows_path.is_absolute() or windows_path.drive:
            raise SpecContextError(f"Spec context path must be repository-relative: {relative_path}")
        if ".." in posix_path.parts or "\\" in relative_path:
            raise SpecContextError(f"Spec context path must stay inside the repository: {relative_path}")

        file_path = self.repo_root.joinpath(*posix_path.parts).resolve()
        try:
            file_path.relative_to(self.repo_root)
        except ValueError as exc:
            raise SpecContextError(f"Spec context path must stay inside the repository: {relative_path}") from exc
        return file_path

    @classmethod
    def _discover_repo_root(cls) -> Path:
        """Find the monorepo root from this module without hardcoded absolute paths."""
        for candidate in Path(__file__).resolve().parents:
            if (
                (candidate / "Agents").is_dir()
                and (candidate / "contracts").is_dir()
                and (candidate / "docs").is_dir()
            ):
                return candidate
        raise SpecContextError("Unable to locate repository root for v2 Agent spec context.")
