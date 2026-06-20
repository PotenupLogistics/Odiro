"""Policy candidate artifact creation for analysis review sessions."""

from __future__ import annotations

import ast
import re
import shutil
from dataclasses import dataclass
from pathlib import Path

from app.agents.result_analysis_v2.review_text import POLICY_CANDIDATE_BLOCK, POLICY_CANDIDATE_MARKER


# Conservative upper bounds for known path-following policy parameters.
PARAMETER_LIMITS = {
    "follow_speed_kmh": 3.5,
    "max_path_error_m": 0.8,
    "look_ahead_distance_m": 1.0,
    "path_smoothing_distance_m": 0.25,
    "max_steering_delta": 0.06,
}

# Runtime cap assignments inserted into configure_from_start on review copies.
RUNTIME_CAP_PARAMETERS = (
    ("followSpeedKmh", "follow_speed_kmh"),
    ("maxPathErrorM", "max_path_error_m"),
    ("lookAheadDistanceM", "look_ahead_distance_m"),
    ("pathSmoothingDistanceM", "path_smoothing_distance_m"),
    ("maxSteeringDelta", "max_steering_delta"),
)

# Review-copy marker used to keep runtime cap insertion idempotent.
CAP_BLOCK_MARKER = "POLICY_REVIEW_CONSERVATIVE_CAPS_APPLIED"


@dataclass(frozen=True)
class CandidateWriteResult:
    """Describes files and warnings produced by a candidate writer."""

    # Whether the candidate artifact was created.
    generated: bool
    # Project-relative path to the artifact directory or file.
    path: str | None
    # Project-relative generated files that belong in manifest.generated_files.
    generated_files: list[str]
    # Non-fatal artifact warnings to preserve in recommendations.json.
    warnings: list[str]


class PolicyCandidateWriter:
    """Copies the user policy package and edits only the review copy."""

    def write(self, *, project_path: Path, review_dir: Path) -> CandidateWriteResult:
        """Create a policy review candidate from <project_path>/policy when possible."""
        source_dir = project_path / "policy"
        target_dir = review_dir / "policy"
        if not source_dir.is_dir():
            return CandidateWriteResult(
                generated=False,
                path=None,
                generated_files=[],
                warnings=[f"policy source directory does not exist: {source_dir}"],
            )

        warnings = self._copy_policy_tree(source_dir=source_dir, target_dir=target_dir)
        modified_path, changed_parameters = self._apply_candidate_changes(target_dir)
        if modified_path is None:
            warnings.append("No Python policy file was available for conservative parameter changes.")
        elif not changed_parameters:
            warnings.append(
                "Policy candidate was copied but no supported policy parameters were found "
                "for conservative candidate changes."
            )
        if modified_path is not None:
            warnings.extend(self._compile_python_files(target_dir))

        generated_files = [
            path.relative_to(project_path).as_posix()
            for path in sorted(target_dir.rglob("*"))
            if path.is_file()
        ]
        return CandidateWriteResult(
            generated=True,
            path=target_dir.relative_to(project_path).as_posix(),
            generated_files=generated_files,
            warnings=warnings,
        )

    def _copy_policy_tree(self, *, source_dir: Path, target_dir: Path) -> list[str]:
        """Copy policy files while excluding runtime caches and symlinks."""
        warnings: list[str] = []
        target_dir.mkdir(parents=True, exist_ok=True)
        for source_path in sorted(source_dir.rglob("*")):
            relative = source_path.relative_to(source_dir)
            if self._should_skip(relative):
                continue
            target_path = target_dir / relative
            if source_path.is_symlink():
                warnings.append(f"Skipped symlink in policy copy: {relative.as_posix()}")
                continue
            if source_path.is_dir():
                target_path.mkdir(parents=True, exist_ok=True)
                continue
            target_path.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source_path, target_path)
        return warnings

    def _should_skip(self, relative: Path) -> bool:
        """Return whether a policy path is a runtime/cache file."""
        parts = set(relative.parts)
        return "__pycache__" in parts or relative.name == ".DS_Store" or relative.suffix == ".pyc"

    def _apply_candidate_changes(self, policy_dir: Path) -> tuple[Path | None, list[str]]:
        """Lower supported path-following parameters in the best Python policy file."""
        candidates = self._candidate_python_files(policy_dir)
        if not candidates:
            return None, []
        target = candidates[0]
        text = target.read_text(encoding="utf-8-sig")
        updated, changed_parameters = self._rewrite_supported_parameters(text)
        if changed_parameters and POLICY_CANDIDATE_MARKER not in updated:
            updated = f"{updated.rstrip()}\n{POLICY_CANDIDATE_BLOCK}"
        if updated != text:
            target.write_text(updated, encoding="utf-8")
        return target, changed_parameters

    def _candidate_python_files(self, policy_dir: Path) -> list[Path]:
        """Prefer policy/policies/path_follower.py, then policy/path_follower.py, then other Python files."""
        files = sorted(path for path in policy_dir.rglob("*.py") if path.is_file())
        nested_path_follower = [
            path
            for path in files
            if path.name == "path_follower.py" and path.parent.name == "policies"
        ]
        root_path_follower = [
            path
            for path in files
            if path.name == "path_follower.py" and path.parent == policy_dir
        ]
        any_path_follower = [path for path in files if path.name == "path_follower.py"]
        regular = [path for path in files if path.name != "__init__.py"]
        return nested_path_follower or root_path_follower or any_path_follower or regular or files

    def _rewrite_supported_parameters(self, source: str) -> tuple[str, list[str]]:
        """Return source with supported numeric parameter values lowered."""
        try:
            tree = ast.parse(source)
        except SyntaxError:
            return source, []

        edits: list[tuple[int, int, str, str]] = []
        line_starts = self._line_starts(source)
        for node in ast.walk(tree):
            edits.extend(self._assignment_edits(node, line_starts=line_starts))
            edits.extend(self._dict_edits(node, line_starts=line_starts))
            edits.extend(self._keyword_edits(node, line_starts=line_starts))
            edits.extend(self._function_default_edits(node, line_starts=line_starts))

        changed_parameters: list[str] = []
        updated = source
        for start, end, replacement, parameter_name in sorted(edits, key=lambda item: item[0], reverse=True):
            updated = f"{updated[:start]}{replacement}{updated[end:]}"
            if parameter_name not in changed_parameters:
                changed_parameters.append(parameter_name)
        updated, cap_parameters = self._insert_configure_runtime_caps(updated)
        for parameter_name in cap_parameters:
            if parameter_name not in changed_parameters:
                changed_parameters.append(parameter_name)
        return updated, sorted(changed_parameters)

    def _assignment_edits(self, node: ast.AST, *, line_starts: list[int]) -> list[tuple[int, int, str, str]]:
        """Return edits for numeric assignments to supported policy parameters."""
        targets: list[ast.AST] = []
        value: ast.AST | None = None
        if isinstance(node, ast.Assign):
            targets = list(node.targets)
            value = node.value
        elif isinstance(node, ast.AnnAssign):
            targets = [node.target]
            value = node.value
        if value is None:
            return []

        edits: list[tuple[int, int, str, str]] = []
        for target in targets:
            parameter_name = self._target_parameter_name(target)
            edit = self._numeric_value_edit(value, parameter_name=parameter_name, line_starts=line_starts)
            if edit is not None:
                edits.append(edit)
        return edits

    def _dict_edits(self, node: ast.AST, *, line_starts: list[int]) -> list[tuple[int, int, str, str]]:
        """Return edits for numeric values in configuration dictionaries."""
        if not isinstance(node, ast.Dict):
            return []
        edits: list[tuple[int, int, str, str]] = []
        for key, value in zip(node.keys, node.values, strict=False):
            if isinstance(key, ast.Constant) and isinstance(key.value, str):
                edit = self._numeric_value_edit(value, parameter_name=key.value, line_starts=line_starts)
                if edit is not None:
                    edits.append(edit)
        return edits

    def _keyword_edits(self, node: ast.AST, *, line_starts: list[int]) -> list[tuple[int, int, str, str]]:
        """Return edits for constructor keyword parameters with numeric values."""
        if not isinstance(node, ast.keyword) or node.arg is None:
            return []
        edit = self._numeric_value_edit(node.value, parameter_name=node.arg, line_starts=line_starts)
        return [edit] if edit is not None else []

    def _function_default_edits(self, node: ast.AST, *, line_starts: list[int]) -> list[tuple[int, int, str, str]]:
        """Return edits for supported numeric function parameter defaults."""
        if not isinstance(node, ast.FunctionDef):
            return []

        edits: list[tuple[int, int, str, str]] = []
        positional_args = node.args.args[-len(node.args.defaults) :] if node.args.defaults else []
        for arg, default in zip(positional_args, node.args.defaults, strict=False):
            edit = self._numeric_value_edit(default, parameter_name=arg.arg, line_starts=line_starts)
            if edit is not None:
                edits.append(edit)
        for arg, default in zip(node.args.kwonlyargs, node.args.kw_defaults, strict=False):
            if default is None:
                continue
            edit = self._numeric_value_edit(default, parameter_name=arg.arg, line_starts=line_starts)
            if edit is not None:
                edits.append(edit)
        return edits

    def _insert_configure_runtime_caps(self, source: str) -> tuple[str, list[str]]:
        """Insert idempotent conservative caps into configure_from_start when it exists."""
        try:
            tree = ast.parse(source)
        except SyntaxError:
            return source, []

        function = self._configure_from_start_function(tree)
        if function is None:
            return source, []

        changed_parameters: list[str] = []
        cap_lines: list[str] = []
        for attribute_name, limit_name in RUNTIME_CAP_PARAMETERS:
            limit = self._format_number(PARAMETER_LIMITS[limit_name])
            assignment = f"self.{attribute_name} = min(self.{attribute_name}, {limit})"
            if assignment in source:
                continue
            cap_lines.append(assignment)
            changed_parameters.append(limit_name)
        if not cap_lines:
            return source, []

        line_starts = self._line_starts(source)
        indent = self._body_indent(function)
        insert_offset, append_after_statement = self._configure_insert_offset(function, line_starts, source)
        block_lines = []
        if CAP_BLOCK_MARKER not in source:
            block_lines.append(f"{indent}# {CAP_BLOCK_MARKER}: cap review-copy policy parameters.")
        block_lines.extend(f"{indent}{line}" for line in cap_lines)
        block = "\n".join(block_lines) + "\n"
        if append_after_statement and insert_offset > 0 and not source[:insert_offset].endswith("\n"):
            block = f"\n{block}"
        return f"{source[:insert_offset]}{block}{source[insert_offset:]}", changed_parameters

    def _configure_from_start_function(self, tree: ast.AST) -> ast.FunctionDef | None:
        """Find the configure_from_start function or method in a candidate policy module."""
        for node in ast.walk(tree):
            if isinstance(node, ast.FunctionDef) and node.name == "configure_from_start":
                return node
        return None

    def _body_indent(self, function: ast.FunctionDef) -> str:
        """Return indentation that matches the configure_from_start body."""
        if function.body:
            return " " * function.body[0].col_offset
        return " " * (function.col_offset + 4)

    def _configure_insert_offset(
        self,
        function: ast.FunctionDef,
        line_starts: list[int],
        source: str,
    ) -> tuple[int, bool]:
        """Return the insertion point, preferring the line before a final top-level return."""
        if function.body and isinstance(function.body[-1], ast.Return):
            return line_starts[function.body[-1].lineno - 1], False
        if not function.body:
            return line_starts[function.end_lineno - 1] if function.end_lineno else len(source), False
        last_statement = function.body[-1]
        if last_statement.end_lineno and last_statement.end_lineno < len(line_starts):
            return line_starts[last_statement.end_lineno], True
        return len(source), True

    def _numeric_value_edit(
        self,
        value: ast.AST,
        *,
        parameter_name: str | None,
        line_starts: list[int],
    ) -> tuple[int, int, str, str] | None:
        """Create one text edit when a numeric parameter exceeds its conservative bound."""
        limit_name = self._limit_name(parameter_name)
        if limit_name is None or not isinstance(value, ast.Constant) or not isinstance(value.value, int | float):
            return None
        limit = PARAMETER_LIMITS[limit_name]
        if float(value.value) <= limit:
            return None
        if value.lineno != value.end_lineno:
            return None
        start = line_starts[value.lineno - 1] + value.col_offset
        end = line_starts[value.end_lineno - 1] + value.end_col_offset
        return start, end, self._format_number(limit), limit_name

    def _compile_python_files(self, policy_dir: Path) -> list[str]:
        """Validate copied Python policies without importing user policy code."""
        warnings: list[str] = []
        for path in sorted(policy_dir.rglob("*.py")):
            warnings.extend(self._compile_python(path))
        return warnings

    def _compile_python(self, path: Path) -> list[str]:
        """Validate modified Python syntax without importing user policy code."""
        try:
            compile(path.read_text(encoding="utf-8-sig"), str(path), "exec")
        except SyntaxError as exc:
            return [f"Policy candidate syntax check failed for {path.name}: {exc.msg}"]
        return []

    def _target_parameter_name(self, target: ast.AST) -> str | None:
        """Extract a candidate parameter name from assignment targets."""
        if isinstance(target, ast.Name):
            return target.id
        if isinstance(target, ast.Attribute):
            return target.attr
        if isinstance(target, ast.Subscript) and isinstance(target.slice, ast.Constant):
            value = target.slice.value
            return value if isinstance(value, str) else None
        return None

    def _limit_name(self, parameter_name: str | None) -> str | None:
        """Map exact and similar policy parameter names to a conservative bound key."""
        if parameter_name is None:
            return None
        normalized = re.sub(r"[^a-z0-9]", "", parameter_name.casefold())
        if normalized in {"followspeedkmh", "pathfollowspeedkmh", "maxfollowspeedkmh"}:
            return "follow_speed_kmh"
        if "follow" in normalized and "speed" in normalized and "kmh" in normalized:
            return "follow_speed_kmh"
        if normalized in {"maxpatherrorm", "patherrorm", "patherrorlimitm"}:
            return "max_path_error_m"
        if "patherror" in normalized and normalized.endswith("m"):
            return "max_path_error_m"
        if "lookahead" in normalized and ("distance" in normalized or normalized.endswith("m")):
            return "look_ahead_distance_m"
        if "pathsmoothing" in normalized and ("distance" in normalized or normalized.endswith("m")):
            return "path_smoothing_distance_m"
        if normalized in {"maxsteeringdelta", "steeringdeltamax"} or "steeringdelta" in normalized:
            return "max_steering_delta"
        return None

    def _line_starts(self, source: str) -> list[int]:
        """Return character offsets for the start of each source line."""
        starts = [0]
        for index, character in enumerate(source):
            if character == "\n":
                starts.append(index + 1)
        return starts

    def _format_number(self, value: float) -> str:
        """Render conservative numeric bounds without noisy trailing zeros."""
        return f"{value:g}"
