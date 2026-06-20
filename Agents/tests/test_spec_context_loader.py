from __future__ import annotations

from importlib import import_module
from pathlib import Path

import pytest


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def _loader_module():
    try:
        return import_module("app.agents.common.spec_context_loader")
    except ModuleNotFoundError as exc:
        pytest.fail(f"spec context loader module should exist: {exc}")


def test_spec_context_loader_reads_allowlisted_docs() -> None:
    module = _loader_module()
    loader = module.SpecContextLoader(repo_root=_repo_root())

    context = loader.build_prompt_block()

    assert "공식 문서 우선 적용" in context
    assert "SPEC_CONTEXT와 기존 bundled prompt가 충돌하면 SPEC_CONTEXT를 우선 적용한다." in context
    assert "<SPEC_CONTEXT>" in context
    assert "</SPEC_CONTEXT>" in context
    for relative_path in module.SPEC_CONTEXT_ALLOWLIST:
        assert f"# SPEC FILE: {relative_path}" in context
    assert "# SPEC FILE: docs/specs/simulation-interface.md" in context
    assert "# SPEC FILE: contracts/specs/user-project-data.md" in context
    assert "# SPEC FILE: Agents/docs/environment/environment-catalog.md" in context


def test_spec_context_loader_raises_clear_error_for_missing_allowlisted_file(tmp_path: Path) -> None:
    module = _loader_module()
    loader = module.SpecContextLoader(repo_root=tmp_path, allowlist=("missing/spec.md",))

    with pytest.raises(module.SpecContextError, match="missing/spec.md"):
        loader.build_prompt_block()


def test_spec_context_allowlist_excludes_archive_legacy_and_client_catalog() -> None:
    module = _loader_module()
    allowlist = tuple(module.SPEC_CONTEXT_ALLOWLIST)

    disallowed_fragments = (
        "Agents/docs/archive",
        "archive/deprecated",
        "archive/previous_episode_spec",
        "Client/Json/environment-catalog.md",
        "EpisodeSetup.json.md",
        "DeliveryBotSetup.json.md",
        "RunQueue.json.md",
        "EpisodeEvaluationReport.json.md",
    )
    for fragment in disallowed_fragments:
        assert not any(fragment in relative_path for relative_path in allowlist)
    assert "Agents/docs/environment/environment-catalog.md" in allowlist
