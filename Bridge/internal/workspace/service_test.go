package workspace

import (
	"errors"
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"testing"
)

// TestServiceCreatesProjectAndRun covers preset copy and run snapshot creation.
func TestServiceCreatesProjectAndRun(t *testing.T) {
	root := t.TempDir()
	presetsDir := filepath.Join(root, "templates")
	runDefaultsDir := filepath.Join(root, "run-defaults")
	writeWorkspacePresets(t, presetsDir)
	writeRunDefaults(t, runDefaultsDir)

	service := NewService(presetsDir, runDefaultsDir)
	list, err := service.ListProjectPresets()
	if err != nil {
		t.Fatalf("ListProjectPresets() error = %v", err)
	}
	if !reflect.DeepEqual(list.ScenarioPresetIDs, []string{"blank"}) ||
		!reflect.DeepEqual(list.ProfilePresetIDs, []string{"basic"}) ||
		!reflect.DeepEqual(list.PolicyPresetIDs, []string{"blank"}) {
		t.Fatalf("Preset catalog = %#v, want blank/basic/blank", list)
	}

	projectPath := filepath.Join(root, "created")
	selection := ProjectPresetSelection{
		ScenarioPresetID: "blank",
		ProfilePresetID:  "basic",
		PolicyPresetID:   "blank",
	}
	created, err := service.CreateProject(projectPath, selection)
	if err != nil {
		t.Fatalf("CreateProject() error = %v", err)
	}
	if created.Project.PresetSelection == nil || *created.Project.PresetSelection != selection {
		t.Fatalf("PresetSelection = %#v, want %#v", created.Project.PresetSelection, selection)
	}
	wantCreated := []string{
		"policy/__init__.py",
		"profile.json",
		"scenario.json",
		"setting.json",
	}
	if !reflect.DeepEqual(created.CreatedPaths, wantCreated) {
		t.Fatalf("CreatedPaths = %#v, want %#v", created.CreatedPaths, wantCreated)
	}
	if _, err := os.Stat(filepath.Join(projectPath, "runs")); err != nil {
		t.Fatalf("runs directory missing: %v", err)
	}
	if _, err := service.ValidateProject(projectPath); err != nil {
		t.Fatalf("ValidateProject() error = %v", err)
	}

	run, err := service.CreateRun(projectPath)
	if err != nil {
		t.Fatalf("CreateRun() error = %v", err)
	}
	if run.RunID != "000001" {
		t.Fatalf("RunID = %q, want 000001", run.RunID)
	}
	for _, path := range []string{
		filepath.Join(run.SnapshotPath, "setting.json"),
		filepath.Join(run.SnapshotPath, "profile.json"),
		filepath.Join(run.SnapshotPath, "scenario.json"),
		filepath.Join(run.SnapshotPath, "policy", "__init__.py"),
		run.ReviewPath,
		run.EpisodesPath,
	} {
		if _, err := os.Stat(path); err != nil {
			t.Fatalf("expected path missing %s: %v", path, err)
		}
	}
	for _, path := range []string{
		filepath.Join(run.ReviewPath, ".gitkeep"),
		filepath.Join(run.EpisodesPath, ".gitkeep"),
		run.StatusPath,
		run.SummaryPath,
	} {
		if _, err := os.Stat(path); !os.IsNotExist(err) {
			t.Fatalf("unexpected generated path %s, stat error = %v", path, err)
		}
	}
	if _, err := ValidateRunSnapshot(projectPath, run.RunID); err != nil {
		t.Fatalf("ValidateRunSnapshot() error = %v", err)
	}
}

// TestCreateProjectRejectsNonEmptyTarget covers PROJECT_EXISTS.
func TestCreateProjectRejectsNonEmptyTarget(t *testing.T) {
	root := t.TempDir()
	presetsDir := filepath.Join(root, "templates")
	runDefaultsDir := filepath.Join(root, "run-defaults")
	writeWorkspacePresets(t, presetsDir)
	writeRunDefaults(t, runDefaultsDir)

	target := filepath.Join(root, "target")
	if err := os.MkdirAll(target, 0755); err != nil {
		t.Fatalf("MkdirAll() error = %v", err)
	}
	if err := os.WriteFile(filepath.Join(target, "existing.txt"), []byte("x"), 0644); err != nil {
		t.Fatalf("WriteFile() error = %v", err)
	}

	service := NewService(presetsDir, runDefaultsDir)
	_, err := service.CreateProject(target, ProjectPresetSelection{ScenarioPresetID: "blank", ProfilePresetID: "basic", PolicyPresetID: "blank"})
	var workspaceErr *Error
	if !errors.As(err, &workspaceErr) || workspaceErr.Code() != "PROJECT_EXISTS" {
		t.Fatalf("CreateProject() error = %v, want PROJECT_EXISTS", err)
	}
}

// TestCreateProjectRejectsGeneratedPythonCache covers preset source hygiene.
func TestCreateProjectRejectsGeneratedPythonCache(t *testing.T) {
	root := t.TempDir()
	presetsDir := filepath.Join(root, "templates")
	runDefaultsDir := filepath.Join(root, "run-defaults")
	writeWorkspacePresets(t, presetsDir)
	writeRunDefaults(t, runDefaultsDir)
	writeFile(t, filepath.Join(presetsDir, "policy", "blank", "__pycache__", "__init__.cpython-314.pyc"), "cache")

	service := NewService(presetsDir, runDefaultsDir)
	_, err := service.CreateProject(
		filepath.Join(root, "target"),
		ProjectPresetSelection{ScenarioPresetID: "blank", ProfilePresetID: "basic", PolicyPresetID: "blank"})
	var workspaceErr *Error
	if !errors.As(err, &workspaceErr) || workspaceErr.Code() != "PROJECT_PRESET_INVALID" {
		t.Fatalf("CreateProject() error = %v, want PROJECT_PRESET_INVALID", err)
	}
}

// TestDefaultStaticResourcesCreateProjectsAndRuns covers repository presets.
func TestDefaultStaticResourcesCreateProjectsAndRuns(t *testing.T) {
	service, err := NewDefaultService()
	if err != nil {
		t.Fatalf("NewDefaultService() error = %v", err)
	}

	presets, err := service.ListProjectPresets()
	if err != nil {
		t.Fatalf("ListProjectPresets() error = %v", err)
	}
	if len(presets.ScenarioPresetIDs) == 0 || len(presets.ProfilePresetIDs) == 0 || len(presets.PolicyPresetIDs) == 0 {
		t.Fatal("ListProjectPresets() returned an empty category")
	}

	root := t.TempDir()
	for _, scenarioPresetID := range presets.ScenarioPresetIDs {
		scenarioPresetID := scenarioPresetID
		t.Run(scenarioPresetID, func(t *testing.T) {
			selection := ProjectPresetSelection{
				ScenarioPresetID: scenarioPresetID,
				ProfilePresetID:  presets.ProfilePresetIDs[0],
				PolicyPresetID:   presets.PolicyPresetIDs[0],
			}
			projectPath := filepath.Join(root, scenarioPresetID)
			if _, err := service.CreateProject(projectPath, selection); err != nil {
				t.Fatalf("CreateProject(%q) error = %v", scenarioPresetID, err)
			}

			run, err := service.CreateRun(projectPath)
			if err != nil {
				t.Fatalf("CreateRun(%q) error = %v", scenarioPresetID, err)
			}
			if _, err := ValidateRunSnapshot(projectPath, run.RunID); err != nil {
				t.Fatalf("ValidateRunSnapshot(%q) error = %v", scenarioPresetID, err)
			}
			for _, snapshotPath := range run.SnapshotPaths {
				if strings.Contains(snapshotPath, "__pycache__") ||
					strings.HasSuffix(snapshotPath, ".pyc") ||
					strings.HasSuffix(snapshotPath, ".pyo") ||
					strings.HasSuffix(snapshotPath, ".gitkeep") {
					t.Fatalf("snapshot copied source-only/generated path: %s", snapshotPath)
				}
			}
		})
	}
}

// writeWorkspacePresets creates minimal valid project presets.
func writeWorkspacePresets(t *testing.T, presetsDir string) {
	t.Helper()
	if err := os.MkdirAll(filepath.Join(presetsDir, "policy", "blank"), 0755); err != nil {
		t.Fatalf("MkdirAll() error = %v", err)
	}
	writeFile(t, filepath.Join(presetsDir, "setting.json"), `{"schema":"project_setting","version":1}`)
	writeFile(t, filepath.Join(presetsDir, "profile", "basic.json"), `{"schema":"simulation_profile","version":1}`)
	writeFile(t, filepath.Join(presetsDir, "scenario", "blank.json"), `{"schema":"scenario","version":1}`)
	writeFile(t, filepath.Join(presetsDir, "policy", "blank", "__init__.py"), "def create_policy():\n    return None\n")
}

// writeRunDefaults creates the static run default folder shape.
func writeRunDefaults(t *testing.T, runDefaultsDir string) {
	t.Helper()
	for _, name := range []string{"review", "episodes"} {
		path := filepath.Join(runDefaultsDir, name)
		if err := os.MkdirAll(path, 0755); err != nil {
			t.Fatalf("MkdirAll() error = %v", err)
		}
		writeFile(t, filepath.Join(path, ".gitkeep"), "")
	}
}

// writeFile writes test fixture content.
func writeFile(t *testing.T, path string, content string) {
	t.Helper()
	if err := os.MkdirAll(filepath.Dir(path), 0755); err != nil {
		t.Fatalf("MkdirAll() error = %v", err)
	}
	if err := os.WriteFile(path, []byte(content), 0644); err != nil {
		t.Fatalf("WriteFile() error = %v", err)
	}
}
