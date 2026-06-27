import asyncio
import json
import logging
import os
import sys
from typing import Dict, Any, List, Optional

# Setup logging
logging.basicConfig(level=logging.INFO, stream=sys.stderr)
logger = logging.getLogger("UmgMcpSkills")

# Import mcp_config from parent dir
CURRENT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.append(CURRENT_DIR)
try:
    from mcp_config import UNREAL_HOST, UNREAL_PORT
except ImportError:
    UNREAL_HOST = "127.0.0.1"
    UNREAL_PORT = 7999

class UnrealConnection:
    """Connection helper for Skill mode."""
    async def send_command(self, command: str, params: Dict[str, Any] = None) -> Dict[str, Any]:
        try:
            reader, writer = await asyncio.open_connection(UNREAL_HOST, UNREAL_PORT)
            payload = json.dumps({"command": command, "params": params or {}})
            writer.write(payload.encode('utf-8') + b'\0')
            await writer.drain()

            if writer.can_write_eof():
                writer.write_eof()

            data = await reader.read(1024 * 1024) # 1MB buffer
            response_str = data.decode('utf-8').split('\0')[0]
            writer.close()
            await writer.wait_closed()
            return json.loads(response_str)
        except Exception as e:
            return {"status": "error", "error": str(e)}

_conn = UnrealConnection()

UMG_DESIGN_MUTATION_TOOLS = {
    "create_widget",
    "set_widget_properties",
    "delete_widget",
    "reparent_widget",
    "apply_layout",
    "create_animation",
    "delete_animation",
    "set_property_keys",
    "remove_property_track",
    "remove_keys",
    "animation_append_widget_tracks",
    "animation_append_time_slice",
    "animation_delete_widget_keys",
}

UMG_DESIGN_REQUIRED_SEQUENCE = [
    "get_widget_tree",
    "compile_blueprint",
    "get_layout_data",
    "check_widget_overlap",
]

UMG_DESIGN_VISUAL_OPTIONS = [
    "capture_slate_window",
    "dump_runtime_widget_geometry",
]

umg_design_verification_pending = False
pending_umg_design_mutation_command = ""
completed_umg_design_verification_steps = set()

def run_async(coro):
    """Helper to run async in sync skill context."""
    return asyncio.run(coro)

def _is_successful_response(response: Dict[str, Any]) -> bool:
    """Return true when a response represents a successful Unreal command result."""
    if not isinstance(response, dict):
        return False
    if response.get("status") == "error":
        return False
    if response.get("success") is False:
        return False
    return response.get("status") == "success" or response.get("success") is True

def _completed_umg_design_verification_steps() -> List[str]:
    """Return completed UMG design verification steps in required order."""
    return [
        step for step in UMG_DESIGN_REQUIRED_SEQUENCE
        if step in completed_umg_design_verification_steps
    ]

def _missing_umg_design_verification_steps() -> List[str]:
    """Return missing UMG design verification steps in required order."""
    return [
        step for step in UMG_DESIGN_REQUIRED_SEQUENCE
        if step not in completed_umg_design_verification_steps
    ]

def _build_umg_design_verification_requirement(trigger_command: str) -> Dict[str, Any]:
    """Build the response payload for mandatory UMG design verification progress."""
    return {
        "required": True,
        "trigger_command": trigger_command,
        "reason": "UMG visual or layout state changed; verify tree, compile result, layout geometry, overlap, and visual/runtime capture before reporting completion.",
        "required_sequence": UMG_DESIGN_REQUIRED_SEQUENCE,
        "completed_sequence": _completed_umg_design_verification_steps(),
        "missing_sequence": _missing_umg_design_verification_steps(),
        "visual_verification_options": UMG_DESIGN_VISUAL_OPTIONS,
    }

def _mark_umg_design_verification_pending(command_name: str) -> None:
    """Start a new pending UMG design verification checklist."""
    global umg_design_verification_pending, pending_umg_design_mutation_command
    umg_design_verification_pending = True
    pending_umg_design_mutation_command = command_name
    completed_umg_design_verification_steps.clear()

def _mark_umg_design_verification_step(command_name: str, response: Dict[str, Any]) -> Dict[str, Any]:
    """Record a successful verification step while preserving no-geometry layout failures."""
    if not umg_design_verification_pending or command_name not in UMG_DESIGN_REQUIRED_SEQUENCE:
        return response
    if not _is_successful_response(response):
        return response
    if command_name == "get_layout_data" and not response.get("layout_data"):
        return response

    completed_umg_design_verification_steps.add(command_name)
    return response

def require_umg_design_verification(command_name: str, response: Dict[str, Any]) -> Dict[str, Any]:
    """Attach the mandatory post-edit UMG design verification checklist."""
    if command_name not in UMG_DESIGN_MUTATION_TOOLS or not _is_successful_response(response):
        return response

    _mark_umg_design_verification_pending(command_name)
    response["design_verification_required"] = _build_umg_design_verification_requirement(command_name)
    return response

def run_skill_command(command_name: str, kwargs: Dict[str, Any]) -> Dict[str, Any]:
    """Run a skill command and enforce the UMG design verification save gate."""
    global umg_design_verification_pending, pending_umg_design_mutation_command

    if command_name == "save_asset" and umg_design_verification_pending and _missing_umg_design_verification_steps():
        return {
            "status": "error",
            "error": "UMG design verification is pending. Run the missing design verification steps before save_asset.",
            "design_verification_required": _build_umg_design_verification_requirement(pending_umg_design_mutation_command),
        }

    response = run_async(_conn.send_command(command_name, kwargs))

    if command_name in UMG_DESIGN_REQUIRED_SEQUENCE:
        return _mark_umg_design_verification_step(command_name, response)
    if command_name == "save_asset" and _is_successful_response(response):
        umg_design_verification_pending = False
        pending_umg_design_mutation_command = ""
        completed_umg_design_verification_steps.clear()
        return response
    return require_umg_design_verification(command_name, response)

# =============================================================================
#  Dynamic Skill Loading from prompts.json
# =============================================================================

def load_skills():
    """Reads prompts.json and registers functions to the global namespace."""
    json_path = os.path.abspath(os.path.join(CURRENT_DIR, "..", "prompts.json"))
    if not os.path.exists(json_path):
        logger.warning(f"prompts.json not found at {json_path}")
        return

    try:
        with open(json_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
            tools = data.get("tools", [])

            for tool in tools:
                if not tool.get("enabled", True):
                    continue

                name = tool["name"]
                desc = tool.get("description", "No description provided.")

                # Create a wrapper function
                # We use a closure to capture the correct tool name
                def create_wrapper(command_name):
                    def skill_wrapper(**kwargs):
                        return run_skill_command(command_name, kwargs)
                    return skill_wrapper

                func = create_wrapper(name)
                func.__name__ = name
                func.__doc__ = desc

                # Register in globals so Gemini CLI can see it as an export
                globals()[name] = func
                logger.info(f"Registered Skill: {name}")

    except Exception as e:
        logger.error(f"Error loading skills: {e}")

# Load all skills on module import
load_skills()
