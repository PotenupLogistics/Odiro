from __future__ import annotations

import importlib.util
from pathlib import Path
from types import ModuleType
from typing import Any

from deliverybot_policy.bot_policy_contract import BotPolicy


REQUIRED_BOT_POLICY_METHODS = ("setConfig", "setContext", "initialize", "decide")


class BotPolicyLoadError(RuntimeError):
    pass


def loadBotPolicyFromScript(scriptPath: str | Path, className: str = "BotPolicy") -> BotPolicy:
    path = Path(scriptPath).expanduser().resolve()
    if not path.exists() or not path.is_file():
        raise BotPolicyLoadError(f"policy script not found: {path}")
    if path.suffix.lower() != ".py":
        raise BotPolicyLoadError(f"policy script must be a .py file: {path}")

    module = loadModuleFromPath(path)
    policy_class = getattr(module, className, None)
    if policy_class is None:
        raise BotPolicyLoadError(f"policy script must export {className} class")
    if not isinstance(policy_class, type):
        raise BotPolicyLoadError(f"{className} export must be a class")

    instance = policy_class()
    validateBotPolicyInstance(instance, className)
    return instance


def loadModuleFromPath(path: Path) -> ModuleType:
    module_name = f"deliverybot_user_policy_{abs(hash(str(path)))}"
    spec = importlib.util.spec_from_file_location(module_name, path)
    if spec is None or spec.loader is None:
        raise BotPolicyLoadError(f"failed to create import spec for policy script: {path}")

    module = importlib.util.module_from_spec(spec)
    try:
        spec.loader.exec_module(module)
    except Exception as error:
        raise BotPolicyLoadError(f"failed to import policy script: {error}") from error

    return module


def validateBotPolicyInstance(instance: Any, className: str = "BotPolicy") -> None:
    for method_name in REQUIRED_BOT_POLICY_METHODS:
        method = getattr(instance, method_name, None)
        if not callable(method):
            raise BotPolicyLoadError(f"{className}.{method_name} is required and must be callable")

