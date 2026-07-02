package api

import (
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"testing"

	bridgeprocess "odiro/bridge/internal/process"
	"odiro/bridge/internal/protocol"
	"odiro/bridge/internal/workspace"
)

// TestRouterCreatesProjectRunAndReportsSimulatorConfigError covers T04 API flow.
func TestRouterCreatesProjectRunAndReportsSimulatorConfigError(t *testing.T) {
	root := t.TempDir()
	presetsDir := filepath.Join(root, "presets")
	runDefaultsDir := filepath.Join(root, "run-defaults")
	writeAPIPresets(t, presetsDir)
	writeAPIRunDefaults(t, runDefaultsDir)

	router := NewRouter(
		workspace.NewService(presetsDir, runDefaultsDir),
		bridgeprocess.NewManager(""),
	)

	listResponse := dispatch(router, protocol.Request{
		Version: protocol.Version,
		ID:      "list",
		Method:  "workspace.listProjectPresets",
	})
	if !listResponse.OK {
		t.Fatalf("list response error = %#v", listResponse.Error)
	}
	catalog, ok := listResponse.Result.(workspace.ProjectPresetCatalog)
	if !ok || len(catalog.ProfilePresets) != 1 || catalog.ProfilePresets[0].Title != "Basic Profile" {
		t.Fatalf("list response result = %#v, want profile manifest metadata", listResponse.Result)
	}

	projectPath := filepath.Join(root, "project")
	createResponse := dispatch(router, protocol.Request{
		Version: protocol.Version,
		ID:      "create-project",
		Method:  "workspace.createProject",
		Params: mustJSON(t, createProjectParams{
			ProjectPath: projectPath,
			PresetSelection: &workspace.ProjectPresetSelection{
				ScenarioPresetID: "blank",
				ProfilePresetID:  "basic",
				PolicyPresetID:   "blank",
			},
		}),
	})
	if !createResponse.OK {
		t.Fatalf("createProject response error = %#v", createResponse.Error)
	}

	runResponse := dispatch(router, protocol.Request{
		Version: protocol.Version,
		ID:      "create-run",
		Method:  "workspace.createRun",
		Params:  mustJSON(t, createRunParams{ProjectPath: projectPath}),
	})
	if !runResponse.OK {
		t.Fatalf("createRun response error = %#v", runResponse.Error)
	}
	run := runResponse.Result.(workspace.CreateRunResult)

	startResponse := dispatch(router, protocol.Request{
		Version: protocol.Version,
		ID:      "start",
		Method:  "process.startSimulator",
		Params:  mustJSON(t, runProcessParams{ProjectPath: projectPath, RunID: run.RunID}),
	})
	if startResponse.OK {
		t.Fatal("startSimulator response OK = true, want false")
	}
	if startResponse.Error == nil || startResponse.Error.Code != "SIMULATOR_EXECUTABLE_REQUIRED" {
		t.Fatalf("startSimulator error = %#v, want SIMULATOR_EXECUTABLE_REQUIRED", startResponse.Error)
	}
}

// dispatch applies protocol validation before API routing.
func dispatch(router *Router, request protocol.Request) protocol.Response {
	return protocol.HandleWith(request, router.Handle)
}

// mustJSON marshals test params into RawMessage.
func mustJSON(t *testing.T, value any) json.RawMessage {
	t.Helper()
	payload, err := json.Marshal(value)
	if err != nil {
		t.Fatalf("Marshal() error = %v", err)
	}
	return payload
}

// writeAPIPresets creates minimal valid project presets.
func writeAPIPresets(t *testing.T, presetsDir string) {
	t.Helper()
	writeAPIFile(t, filepath.Join(presetsDir, "setting.json"), `{"schema":"project_setting","version":1}`)
	writeAPIManifest(t, filepath.Join(presetsDir, "scenario", "blank"), "scenario", "blank", "Blank Scenario", 20)
	writeAPIManifest(t, filepath.Join(presetsDir, "profile", "basic"), "profile", "basic", "Basic Profile", 10)
	writeAPIManifest(t, filepath.Join(presetsDir, "policy", "blank"), "policy", "blank", "Blank Policy", 30)
	writeAPIFile(t, filepath.Join(presetsDir, "scenario", "blank", "scenario.json"), `{"schema":"scenario","version":1}`)
	writeAPIFile(t, filepath.Join(presetsDir, "profile", "basic", "profile.json"), `{"schema":"simulation_profile","version":1}`)
	writeAPIFile(t, filepath.Join(presetsDir, "policy", "blank", "policy", "__init__.py"), "def create_policy():\n    return None\n")
}

// writeAPIManifest creates one valid preset manifest and thumbnail.
func writeAPIManifest(t *testing.T, presetRoot string, kind string, id string, title string, sortOrder int) {
	t.Helper()
	writeAPIFile(
		t,
		filepath.Join(presetRoot, "manifest.json"),
		fmt.Sprintf(
			`{"schema":"project_preset_manifest","version":1,"id":%q,"kind":%q,"title":%q,"sort_order":%d}`,
			id,
			kind,
			title,
			sortOrder,
		),
	)
	writeAPIFile(t, filepath.Join(presetRoot, "thumbnail.png"), "png")
}

// writeAPIRunDefaults creates the static run default folder shape.
func writeAPIRunDefaults(t *testing.T, runDefaultsDir string) {
	t.Helper()
	for _, name := range []string{"review", "episodes"} {
		path := filepath.Join(runDefaultsDir, name)
		if err := os.MkdirAll(path, 0755); err != nil {
			t.Fatalf("MkdirAll() error = %v", err)
		}
		writeAPIFile(t, filepath.Join(path, ".gitkeep"), "")
	}
}

// writeAPIFile writes test fixture content.
func writeAPIFile(t *testing.T, path string, content string) {
	t.Helper()
	if err := os.MkdirAll(filepath.Dir(path), 0755); err != nil {
		t.Fatalf("MkdirAll() error = %v", err)
	}
	if err := os.WriteFile(path, []byte(content), 0644); err != nil {
		t.Fatalf("WriteFile() error = %v", err)
	}
}
