from __future__ import annotations

import os
import shutil
import subprocess
from pathlib import Path

import pytest


ROOT = Path(__file__).resolve().parents[1]
POWERSHELL = shutil.which("powershell.exe") or shutil.which("pwsh")


def _write_fake_uv(bin_dir: Path) -> Path:
    uv_path = bin_dir / "uv.cmd"
    uv_path.write_text(
        """@echo off
echo %*>>"%ODIRO_FAKE_UV_LOG%"
if defined ODIRO_EXPECTED_OPENAI_KEY if "%OPENAI_API_KEY%"=="%ODIRO_EXPECTED_OPENAI_KEY%" echo OPENAI_KEY_MATCHED_EXPECTED>>"%ODIRO_FAKE_UV_LOG%"
if defined ODIRO_UNEXPECTED_OPENAI_KEY if "%OPENAI_API_KEY%"=="%ODIRO_UNEXPECTED_OPENAI_KEY%" echo OPENAI_KEY_MATCHED_UNEXPECTED>>"%ODIRO_FAKE_UV_LOG%"
if "%1"=="sync" exit /b 0
if "%1"=="run" if "%2"=="python" if "%3"=="scripts/build_pdf_rag_index.py" if "%4"=="--check-only" (
  if "%ODIRO_FAKE_INDEX_STATUS%"=="up-to-date" (
    echo [PDF RAG] Chroma index is up to date.
    exit /b 0
  )
  if "%ODIRO_FAKE_INDEX_STATUS%"=="stale" (
    echo [PDF RAG] Chroma index is stale.
    exit /b 11
  )
  echo [PDF RAG] Chroma index missing.
  exit /b 10
)
if "%1"=="run" if "%2"=="python" if "%3"=="scripts/build_pdf_rag_index.py" (
  echo [PDF RAG] fake build completed.
  exit /b 0
)
exit /b 0
""",
        encoding="utf-8",
    )
    return uv_path


def _copy_agents_tools(tmp_path: Path) -> Path:
    agents_root = tmp_path / "Agents"
    tools_dir = agents_root / "tools"
    tools_dir.mkdir(parents=True)
    (agents_root / "pyproject.toml").write_text("[project]\nname='fake-agents'\nversion='0.0.0'\n", encoding="utf-8")
    for name in ("common.ps1", "install.ps1"):
        shutil.copy2(ROOT / "tools" / name, tools_dir / name)
    return agents_root


def _run_install(
    tmp_path: Path,
    *,
    env_overrides: dict[str, str | None],
    propagate_last_exit_code: bool = False,
    env_file_contents: str | None = None,
) -> subprocess.CompletedProcess[str]:
    if POWERSHELL is None:
        pytest.skip("PowerShell is required for install flow tests")
    agents_root = _copy_agents_tools(tmp_path)
    if env_file_contents is not None:
        (agents_root / ".env").write_text(env_file_contents, encoding="utf-8")
    bin_dir = tmp_path / "bin"
    bin_dir.mkdir()
    _write_fake_uv(bin_dir)
    log_path = tmp_path / "uv.log"
    env = os.environ.copy()
    env["PATH"] = str(bin_dir) + os.pathsep + env.get("PATH", "")
    env["ODIRO_FAKE_UV_LOG"] = str(log_path)
    env.setdefault("ODIRO_FAKE_INDEX_STATUS", "missing")
    for key, value in env_overrides.items():
        if value is None:
            env.pop(key, None)
        else:
            env[key] = value
    command = [
        POWERSHELL,
        "-NoProfile",
        "-ExecutionPolicy",
        "Bypass",
    ]
    if propagate_last_exit_code:
        command.extend(
            [
                "-Command",
                f"& '{agents_root / 'tools' / 'install.ps1'}'; exit $LASTEXITCODE",
            ]
        )
    else:
        command.extend(
            [
                "-File",
                str(agents_root / "tools" / "install.ps1"),
            ]
        )
    completed = subprocess.run(
        command,
        cwd=agents_root,
        text=True,
        capture_output=True,
        env=env,
    )
    completed.fake_uv_log = log_path.read_text(encoding="utf-8") if log_path.exists() else ""  # type: ignore[attr-defined]
    return completed


def test_install_resets_last_exit_code_after_nonfatal_missing_index(tmp_path: Path) -> None:
    completed = _run_install(
        tmp_path,
        env_overrides={
            "OPENAI_API_KEY": None,
        },
        propagate_last_exit_code=True,
    )

    assert completed.returncode == 0
    assert "OPENAI_API_KEY is not set" in completed.stdout


def test_install_skips_pdf_rag_index_when_opt_out_is_set(tmp_path: Path) -> None:
    completed = _run_install(
        tmp_path,
        env_overrides={
            "ODIRO_SKIP_PDF_RAG_INDEX": "1",
            "OPENAI_API_KEY": None,
        },
    )

    assert completed.returncode == 0
    assert "Skipping PDF RAG Chroma index setup" in completed.stdout
    assert "build_pdf_rag_index.py" not in completed.fake_uv_log  # type: ignore[attr-defined]


def test_install_warns_and_continues_when_index_missing_without_openai_key(tmp_path: Path) -> None:
    completed = _run_install(
        tmp_path,
        env_overrides={
            "OPENAI_API_KEY": None,
        },
    )

    assert completed.returncode == 0
    assert "OPENAI_API_KEY is not set" in completed.stdout
    assert "Setup will continue" in completed.stdout
    assert "--check-only" in completed.fake_uv_log  # type: ignore[attr-defined]
    assert "scripts/build_pdf_rag_index.py" not in "\n".join(
        line for line in completed.fake_uv_log.splitlines() if "--check-only" not in line  # type: ignore[attr-defined]
    )


def test_install_builds_pdf_rag_index_when_missing_with_openai_key(tmp_path: Path) -> None:
    completed = _run_install(
        tmp_path,
        env_overrides={
            "OPENAI_API_KEY": "test-key-is-not-printed",
        },
    )

    assert completed.returncode == 0
    assert "Building local Chroma index" in completed.stdout
    assert "Chroma index build completed" in completed.stdout
    assert "test-key-is-not-printed" not in completed.stdout
    assert "--check-only" in completed.fake_uv_log  # type: ignore[attr-defined]
    assert any(
        "scripts/build_pdf_rag_index.py" in line and "--check-only" not in line
        for line in completed.fake_uv_log.splitlines()  # type: ignore[attr-defined]
    )


def test_install_loads_openai_key_from_agents_env_for_pdf_rag_build(tmp_path: Path) -> None:
    dotenv_key = "dotenv-key-is-not-printed"
    completed = _run_install(
        tmp_path,
        env_overrides={
            "OPENAI_API_KEY": None,
            "ODIRO_EXPECTED_OPENAI_KEY": dotenv_key,
        },
        env_file_contents=f"OPENAI_API_KEY={dotenv_key}\n",
    )

    assert completed.returncode == 0
    assert "Building local Chroma index" in completed.stdout
    assert "Chroma index build completed" in completed.stdout
    assert "OPENAI_KEY_MATCHED_EXPECTED" in completed.fake_uv_log  # type: ignore[attr-defined]
    assert dotenv_key not in completed.stdout
    assert dotenv_key not in completed.stderr
    assert dotenv_key not in completed.fake_uv_log  # type: ignore[attr-defined]


def test_install_prefers_process_openai_key_over_agents_env(tmp_path: Path) -> None:
    process_key = "process-key-is-not-printed"
    dotenv_key = "dotenv-key-is-not-used"
    completed = _run_install(
        tmp_path,
        env_overrides={
            "OPENAI_API_KEY": process_key,
            "ODIRO_EXPECTED_OPENAI_KEY": process_key,
            "ODIRO_UNEXPECTED_OPENAI_KEY": dotenv_key,
        },
        env_file_contents=f"OPENAI_API_KEY={dotenv_key}\n",
    )

    assert completed.returncode == 0
    assert "OPENAI_KEY_MATCHED_EXPECTED" in completed.fake_uv_log  # type: ignore[attr-defined]
    assert "OPENAI_KEY_MATCHED_UNEXPECTED" not in completed.fake_uv_log  # type: ignore[attr-defined]
    assert process_key not in completed.stdout
    assert dotenv_key not in completed.stdout
    assert process_key not in completed.stderr
    assert dotenv_key not in completed.stderr
    assert process_key not in completed.fake_uv_log  # type: ignore[attr-defined]
    assert dotenv_key not in completed.fake_uv_log  # type: ignore[attr-defined]


def test_install_strict_mode_fails_when_index_missing_without_openai_key(tmp_path: Path) -> None:
    completed = _run_install(
        tmp_path,
        env_overrides={
            "OPENAI_API_KEY": None,
            "ODIRO_REQUIRE_PDF_RAG_INDEX": "1",
        },
    )

    assert completed.returncode != 0
    assert "OPENAI_API_KEY is not set" in completed.stdout
    assert "PDF RAG Chroma index is required" in completed.stdout
