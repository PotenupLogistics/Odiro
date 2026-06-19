package api

import (
	"encoding/json"
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
	templatesDir := filepath.Join(root, "project-templates")
	runDefaultsDir := filepath.Join(root, "run-defaults")
	writeAPITemplate(t, filepath.Join(templatesDir, "blank"))
	writeAPIRunDefaults(t, runDefaultsDir)

	router := NewRouter(
		workspace.NewService(templatesDir, runDefaultsDir),
		bridgeprocess.NewManager(""),
	)

	listResponse := dispatch(router, protocol.Request{
		Version: protocol.Version,
		ID:      "list",
		Method:  "workspace.listProjectTemplates",
	})
	if !listResponse.OK {
		t.Fatalf("list response error = %#v", listResponse.Error)
	}

	projectPath := filepath.Join(root, "project")
	createResponse := dispatch(router, protocol.Request{
		Version: protocol.Version,
		ID:      "create-project",
		Method:  "workspace.createProject",
		Params:  mustJSON(t, createProjectParams{ProjectPath: projectPath, TemplateID: "blank"}),
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

// writeAPITemplate creates a minimal valid project template.
func writeAPITemplate(t *testing.T, templateDir string) {
	t.Helper()
	if err := os.MkdirAll(filepath.Join(templateDir, "policy"), 0755); err != nil {
		t.Fatalf("MkdirAll() error = %v", err)
	}
	writeAPIFile(t, filepath.Join(templateDir, "setting.json"), `{"schema":"project_setting","version":1}`)
	writeAPIFile(t, filepath.Join(templateDir, "profile.json"), `{"schema":"simulation_profile","version":1}`)
	writeAPIFile(t, filepath.Join(templateDir, "scenario.json"), `{"schema":"scenario","version":1}`)
	writeAPIFile(t, filepath.Join(templateDir, "policy", "__init__.py"), "def create_policy():\n    return None\n")
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
