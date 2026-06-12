# Assets And Config

Covers: `Content`, `Config`, map and Blueprint integration points

## Entry Points

- `Config/Default*.ini`: project, engine, game, input, and gameplay tag settings
- `Content/Maps`: `MainMenuMap`, `ScenarioEditorMap`, `ScenarioSimulationMap`, `PedestrianTestMap`, `Test`
- `Content/Data/Scenario`: scenario catalogs (`DA_ScenarioAssetPaletteCatalog`, `DA_ScenarioStaticObstaclePropCatalog`)
- `Content/Blueprints`: project Blueprint assets (MainMenu GameMode 등)
- `Content/Characters`: pedestrian character/animation assets
- `Content/Models/Delivery`, `Content/Vehicles`: delivery robot meshes and vehicle assets
- `Content/Widgets`: UMG widget Blueprint assets

## Notes

- Blueprint classes and data assets bind C++ properties into maps and test workflows
- Catalog default soft paths are defined in C++ (`MakeDefaultCatalogReference`) and must match `/Game/Data/Scenario/` asset locations
