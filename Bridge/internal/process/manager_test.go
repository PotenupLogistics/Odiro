package process

import (
	"errors"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"

	"odiro/bridge/internal/workspace"
)

// TestMain lets process lifecycle tests reuse the test binary as a child process.
func TestMain(m *testing.M) {
	if os.Getenv("ODIRO_BRIDGE_TEST_CHILD") == "1" {
		os.Exit(0)
	}
	os.Exit(m.Run())
}

// TestStartSimulatorRequiresExecutable verifies the configured executable gate.
func TestStartSimulatorRequiresExecutable(t *testing.T) {
	projectPath, runID, statusPath := createProcessTestRun(t)

	manager := NewManager("")
	_, err := manager.StartSimulator(projectPath, runID, 0)
	var processErr *Error
	if !errors.As(err, &processErr) || processErr.Code() != "SIMULATOR_EXECUTABLE_REQUIRED" {
		t.Fatalf("StartSimulator() error = %v, want SIMULATOR_EXECUTABLE_REQUIRED", err)
	}
	if _, err := os.Stat(statusPath); !os.IsNotExist(err) {
		t.Fatalf("status.json should not exist, stat error = %v", err)
	}
}

// TestGetRunStatusRejectsUntrackedRun verifies tracked-process ownership.
func TestGetRunStatusRejectsUntrackedRun(t *testing.T) {
	projectPath, _, _ := createProcessTestRun(t)

	manager := NewManager("")
	_, err := manager.GetRunStatus(projectPath, "000001")
	var processErr *Error
	if !errors.As(err, &processErr) || processErr.Code() != "RUN_NOT_TRACKED" {
		t.Fatalf("GetRunStatus() error = %v, want RUN_NOT_TRACKED", err)
	}
}

// TestStartSimulatorTracksProcessAndWritesExitStatus covers status lifecycle.
func TestStartSimulatorTracksProcessAndWritesExitStatus(t *testing.T) {
	t.Setenv("ODIRO_BRIDGE_TEST_CHILD", "1")
	projectPath, runID, statusPath := createProcessTestRun(t)
	executable, err := os.Executable()
	if err != nil {
		t.Fatalf("Executable() error = %v", err)
	}

	manager := NewManager(executable)
	status, err := manager.StartSimulator(projectPath, runID, 18124)
	if err != nil {
		t.Fatalf("StartSimulator() error = %v", err)
	}
	if status.State != "running" {
		t.Fatalf("State = %q, want running", status.State)
	}
	if status.ProcessID == 0 {
		t.Fatal("ProcessID = 0, want child process id")
	}

	deadline := time.Now().Add(5 * time.Second)
	for time.Now().Before(deadline) {
		status, err = manager.GetRunStatus(projectPath, runID)
		if err != nil {
			t.Fatalf("GetRunStatus() error = %v", err)
		}
		if status.State == "exited" {
			break
		}
		time.Sleep(25 * time.Millisecond)
	}
	if status.State != "exited" {
		t.Fatalf("State = %q, want exited", status.State)
	}
	if status.ExitCode == nil || *status.ExitCode != 0 {
		t.Fatalf("ExitCode = %#v, want 0", status.ExitCode)
	}

	payload, err := os.ReadFile(statusPath)
	if err != nil {
		t.Fatalf("ReadFile(status.json) error = %v", err)
	}
	statusJSON := string(payload)
	for _, want := range []string{`"schema": "run_status"`, `"state": "exited"`, `"policy_port": 18124`} {
		if !strings.Contains(statusJSON, want) {
			t.Fatalf("status.json missing %s: %s", want, statusJSON)
		}
	}
}

// createProcessTestRun creates a valid project and one run snapshot.
func createProcessTestRun(t *testing.T) (string, string, string) {
	t.Helper()
	root := t.TempDir()
	presetsDir := filepath.Join(root, "templates")
	runDefaultsDir := filepath.Join(root, "run-defaults")
	writeProcessPresets(t, presetsDir)
	writeProcessRunDefaults(t, runDefaultsDir)

	service := workspace.NewService(presetsDir, runDefaultsDir)
	projectPath := filepath.Join(root, "project")
	if _, err := service.CreateProject(
		projectPath,
		workspace.ProjectPresetSelection{ScenarioPresetID: "blank", ProfilePresetID: "basic", PolicyPresetID: "blank"}); err != nil {
		t.Fatalf("CreateProject() error = %v", err)
	}
	run, err := service.CreateRun(projectPath)
	if err != nil {
		t.Fatalf("CreateRun() error = %v", err)
	}
	return projectPath, run.RunID, run.StatusPath
}

// writeProcessPresets creates minimal valid project presets.
func writeProcessPresets(t *testing.T, presetsDir string) {
	t.Helper()
	if err := os.MkdirAll(filepath.Join(presetsDir, "policy", "blank"), 0755); err != nil {
		t.Fatalf("MkdirAll() error = %v", err)
	}
	writeProcessFile(t, filepath.Join(presetsDir, "setting.json"), `{"schema":"project_setting","version":1}`)
	writeProcessFile(t, filepath.Join(presetsDir, "profile", "basic.json"), `{"schema":"simulation_profile","version":1}`)
	writeProcessFile(t, filepath.Join(presetsDir, "scenario", "blank.json"), `{"schema":"scenario","version":1}`)
	writeProcessFile(t, filepath.Join(presetsDir, "policy", "blank", "__init__.py"), "def create_policy():\n    return None\n")
}

// writeProcessRunDefaults creates the static run default folder shape.
func writeProcessRunDefaults(t *testing.T, runDefaultsDir string) {
	t.Helper()
	for _, name := range []string{"review", "episodes"} {
		path := filepath.Join(runDefaultsDir, name)
		if err := os.MkdirAll(path, 0755); err != nil {
			t.Fatalf("MkdirAll() error = %v", err)
		}
		writeProcessFile(t, filepath.Join(path, ".gitkeep"), "")
	}
}

// writeProcessFile writes test fixture content.
func writeProcessFile(t *testing.T, path string, content string) {
	t.Helper()
	if err := os.MkdirAll(filepath.Dir(path), 0755); err != nil {
		t.Fatalf("MkdirAll() error = %v", err)
	}
	if err := os.WriteFile(path, []byte(content), 0644); err != nil {
		t.Fatalf("WriteFile() error = %v", err)
	}
}
