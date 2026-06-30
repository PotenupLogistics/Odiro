"""Wrap static-obstacle mesh props in Blueprint actor classes and bind the prop catalog.

Run from Unreal Editor after the C++ fields in FScenarioStaticObstaclePropEntry are available:
UnrealEditor-Cmd.exe OdiroSim.uproject -run=pythonscript -script=Tools/Editor/MigrateStaticObstaclePropsToActorBlueprints.py -unattended -nop4
"""

import re

import unreal


CATALOG_PATH = "/Game/Data/Scenario/DA_ScenarioStaticObstaclePropCatalog"
DESTINATION_DIR = "/Game/Blueprints/Scenario/Obstacles"
PARENT_CLASS_PATH = "/Script/OdiroSim.ScenarioStaticObstacle"


def get_editor_property(obj, names, default=None):
    """Return the first reflected property value that exists on obj."""
    for name in names:
        try:
            return obj.get_editor_property(name)
        except Exception:
            continue
    return default


def set_editor_property(obj, names, value):
    """Set the first reflected property that exists on obj."""
    for name in names:
        try:
            obj.set_editor_property(name, value)
            return True
        except Exception:
            continue
    return False


def require_editor_property(obj, names, value, description):
    """Set a reflected property and fail when none of the candidate names works."""
    if not set_editor_property(obj, names, value):
        raise RuntimeError(f"Failed to set {description}. Tried: {', '.join(names)}")


def load_soft_asset(value):
    """Resolve a soft object property, object path, or already-loaded object."""
    if value is None:
        return None
    if isinstance(value, unreal.Object):
        return value

    to_soft_object_path = getattr(value, "to_soft_object_path", None)
    if callable(to_soft_object_path):
        value = to_soft_object_path()

    path = str(value)
    if not path or path.lower() == "none":
        return None

    quoted_path_match = re.search(r"'([^']+)'", path)
    if quoted_path_match:
        path = quoted_path_match.group(1)
    path = path.strip()
    if "." in path:
        return unreal.load_asset(path)
    return unreal.load_asset(path)


def vector_component(vector_value, name):
    """Read a Vector component from native or Pythonized property names."""
    return float(
        getattr(
            vector_value,
            name,
            getattr(vector_value, name.lower(), 0.0),
        )
    )


def make_asset_name(prop_id):
    """Create a stable Blueprint asset name from a prop id."""
    raw_name = prop_id.split(".")[-1]
    parts = [part for part in re.split(r"[^A-Za-z0-9]+", raw_name) if part]
    return "BP_SO_" + "_".join(part[:1].upper() + part[1:] for part in parts)


def ensure_destination_dir():
    """Create the Blueprint destination directory when missing."""
    if not unreal.EditorAssetLibrary.does_directory_exist(DESTINATION_DIR):
        unreal.EditorAssetLibrary.make_directory(DESTINATION_DIR)


def load_parent_class():
    """Load the native parent class used by wrapped obstacle Blueprints."""
    parent_class = unreal.load_class(None, PARENT_CLASS_PATH)
    if not parent_class:
        raise RuntimeError(f"Failed to load parent class: {PARENT_CLASS_PATH}")
    return parent_class


def create_or_load_blueprint(asset_name, parent_class):
    """Create the obstacle Blueprint when absent, otherwise load the existing asset."""
    asset_path = f"{DESTINATION_DIR}/{asset_name}"
    existing_asset = unreal.load_asset(asset_path)
    if existing_asset:
        return existing_asset

    factory = unreal.BlueprintFactory()
    if not set_editor_property(factory, ["ParentClass", "parent_class"], parent_class):
        raise RuntimeError("BlueprintFactory.ParentClass property is unavailable.")

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    blueprint = asset_tools.create_asset(asset_name, DESTINATION_DIR, unreal.Blueprint, factory)
    if not blueprint:
        raise RuntimeError(f"Failed to create Blueprint: {asset_path}")
    return blueprint


def get_blueprint_generated_class(blueprint, asset_name):
    """Resolve the generated class for a Blueprint asset."""
    generated_class = get_editor_property(blueprint, ["GeneratedClass", "generated_class"])
    if generated_class:
        return generated_class

    class_path = f"{DESTINATION_DIR}/{asset_name}.{asset_name}_C"
    generated_class = unreal.load_class(None, class_path)
    if not generated_class:
        raise RuntimeError(f"Failed to load generated class: {class_path}")
    return generated_class


def configure_blueprint_defaults(blueprint, asset_name, mesh, bounds_size_m, bounds_center_offset_m):
    """Configure the wrapped Blueprint CDO to render the legacy mesh with the migrated bounds."""
    generated_class = get_blueprint_generated_class(blueprint, asset_name)
    class_default_object = unreal.get_default_object(generated_class)
    if not class_default_object:
        raise RuntimeError(f"Failed to get CDO for {asset_name}")

    require_editor_property(class_default_object, ["StaticMeshAsset", "static_mesh_asset"], mesh, "Blueprint StaticMeshAsset")
    require_editor_property(
        class_default_object,
        ["BoundsSizeMeters", "bounds_size_meters"],
        bounds_size_m,
        "Blueprint BoundsSizeMeters",
    )
    require_editor_property(
        class_default_object,
        ["BoundsCenterOffsetMeters", "bounds_center_offset_meters"],
        bounds_center_offset_m,
        "Blueprint BoundsCenterOffsetMeters",
    )
    require_editor_property(
        class_default_object,
        ["bUseMeshSimpleCollision", "b_use_mesh_simple_collision", "use_mesh_simple_collision"],
        False,
        "Blueprint bUseMeshSimpleCollision",
    )
    require_editor_property(
        class_default_object,
        ["bUseFallbackBoxCollision", "b_use_fallback_box_collision", "use_fallback_box_collision"],
        True,
        "Blueprint bUseFallbackBoxCollision",
    )

    mesh_root = get_editor_property(class_default_object, ["MeshRoot", "mesh_root"])
    if mesh_root and mesh:
        if hasattr(mesh_root, "set_static_mesh"):
            mesh_root.set_static_mesh(mesh)
        else:
            set_editor_property(mesh_root, ["StaticMesh", "static_mesh"], mesh)

    if hasattr(unreal, "KismetEditorUtilities"):
        unreal.KismetEditorUtilities.compile_blueprint(blueprint)

    unreal.EditorAssetLibrary.save_loaded_asset(blueprint)


def migrate_entry(entry, parent_class):
    """Migrate one catalog entry and return True when it was changed."""
    prop_id = str(get_editor_property(entry, ["PropId", "prop_id"], ""))
    if not prop_id or prop_id.lower() == "none":
        return False

    mesh_ref = get_editor_property(entry, ["StaticMeshAsset", "static_mesh_asset"])
    mesh = load_soft_asset(mesh_ref)
    if not mesh:
        unreal.log_warning(f"Skipping {prop_id}: StaticMeshAsset is empty or failed to load.")
        return False

    fallback_extent_cm = get_editor_property(entry, ["FallbackBoxExtent", "fallback_box_extent"])
    if not fallback_extent_cm:
        unreal.log_warning(f"Skipping {prop_id}: FallbackBoxExtent is unavailable.")
        return False

    extent_x = max(vector_component(fallback_extent_cm, "X"), 0.0)
    extent_y = max(vector_component(fallback_extent_cm, "Y"), 0.0)
    extent_z = max(vector_component(fallback_extent_cm, "Z"), 0.0)
    bounds_size_m = unreal.Vector(extent_x * 0.02, extent_y * 0.02, extent_z * 0.02)
    bounds_center_offset_m = unreal.Vector(0.0, 0.0, extent_z * 0.01)

    asset_name = make_asset_name(prop_id)
    blueprint = create_or_load_blueprint(asset_name, parent_class)
    configure_blueprint_defaults(blueprint, asset_name, mesh, bounds_size_m, bounds_center_offset_m)

    generated_class = get_blueprint_generated_class(blueprint, asset_name)
    require_editor_property(entry, ["ObstacleActorClass", "obstacle_actor_class"], generated_class, "catalog ObstacleActorClass")
    require_editor_property(entry, ["BoundsSizeMeters", "bounds_size_meters"], bounds_size_m, "catalog BoundsSizeMeters")
    require_editor_property(
        entry,
        ["BoundsCenterOffsetMeters", "bounds_center_offset_meters"],
        bounds_center_offset_m,
        "catalog BoundsCenterOffsetMeters",
    )
    require_editor_property(
        entry,
        ["bUseMeshSimpleCollision", "b_use_mesh_simple_collision", "use_mesh_simple_collision"],
        False,
        "catalog bUseMeshSimpleCollision",
    )
    require_editor_property(
        entry,
        ["bUseFallbackBoxCollision", "b_use_fallback_box_collision", "use_fallback_box_collision"],
        True,
        "catalog bUseFallbackBoxCollision",
    )

    unreal.log(f"Migrated {prop_id} -> {DESTINATION_DIR}/{asset_name}")
    return True


def main():
    """Migrate all legacy mesh entries in DA_ScenarioStaticObstaclePropCatalog."""
    ensure_destination_dir()
    parent_class = load_parent_class()
    catalog = unreal.load_asset(CATALOG_PATH)
    if not catalog:
        raise RuntimeError(f"Failed to load catalog: {CATALOG_PATH}")

    entries = list(get_editor_property(catalog, ["Entries", "entries"], []))
    changed_count = 0
    for entry in entries:
        if migrate_entry(entry, parent_class):
            changed_count += 1

    if changed_count:
        if not set_editor_property(catalog, ["Entries", "entries"], entries):
            raise RuntimeError("Failed to write migrated Entries back to the catalog.")
        unreal.EditorAssetLibrary.save_loaded_asset(catalog)

    unreal.log(f"Static obstacle prop migration complete. Updated entries: {changed_count}")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        unreal.log_error(str(exc))
        raise
