package workspace

import (
	"errors"
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"testing"
)

// TestServiceCreatesProjectAndRun covers template copy and run snapshot creation.
func TestServiceCreatesProjectAndRun(t *testing.T) {
	root := t.TempDir()
	templatesDir := filepath.Join(root, "project-templates")
	runDefaultsDir := filepath.Join(root, "run-defaults")
	writeWorkspaceTemplate(t, filepath.Join(templatesDir, "blank"))
	writeRunDefaults(t, runDefaultsDir)

	service := NewService(templatesDir, runDefaultsDir)
	list, err := service.ListProjectTemplates()
	if err != nil {
		t.Fatalf("ListProjectTemplates() error = %v", err)
	}
	if len(list.Templates) != 1 || list.Templates[0].TemplateID != "blank" {
		t.Fatalf("Templates = %#v, want blank", list.Templates)
	}

	projectPath := filepath.Join(root, "created")
	created, err := service.CreateProject(projectPath, "blank")
	if err != nil {
		t.Fatalf("CreateProject() error = %v", err)
	}
	if created.Project.TemplateID != "blank" {
		t.Fatalf("TemplateID = %q, want blank", created.Project.TemplateID)
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
	templatesDir := filepath.Join(root, "project-templates")
	runDefaultsDir := filepath.Join(root, "run-defaults")
	writeWorkspaceTemplate(t, filepath.Join(templatesDir, "blank"))
	writeRunDefaults(t, runDefaultsDir)

	target := filepath.Join(root, "target")
	if err := os.MkdirAll(target, 0755); err != nil {
		t.Fatalf("MkdirAll() error = %v", err)
	}
	if err := os.WriteFile(filepath.Join(target, "existing.txt"), []byte("x"), 0644); err != nil {
		t.Fatalf("WriteFile() error = %v", err)
	}

	service := NewService(templatesDir, runDefaultsDir)
	_, err := service.CreateProject(target, "blank")
	var workspaceErr *Error
	if !errors.As(err, &workspaceErr) || workspaceErr.Code() != "PROJECT_EXISTS" {
		t.Fatalf("CreateProject() error = %v, want PROJECT_EXISTS", err)
	}
}

// TestCreateProjectRejectsGeneratedPythonCache covers template source hygiene.
func TestCreateProjectRejectsGeneratedPythonCache(t *testing.T) {
	root := t.TempDir()
	templatesDir := filepath.Join(root, "project-templates")
	runDefaultsDir := filepath.Join(root, "run-defaults")
	templateDir := filepath.Join(templatesDir, "blank")
	writeWorkspaceTemplate(t, templateDir)
	writeRunDefaults(t, runDefaultsDir)
	writeFile(t, filepath.Join(templateDir, "policy", "__pycache__", "__init__.cpython-314.pyc"), "cache")

	service := NewService(templatesDir, runDefaultsDir)
	_, err := service.CreateProject(filepath.Join(root, "target"), "blank")
	var workspaceErr *Error
	if !errors.As(err, &workspaceErr) || workspaceErr.Code() != "PROJECT_TEMPLATE_INVALID" {
		t.Fatalf("CreateProject() error = %v, want PROJECT_TEMPLATE_INVALID", err)
	}
}

// TestDefaultStaticResourcesCreateProjectsAndRuns covers repository templates.
func TestDefaultStaticResourcesCreateProjectsAndRuns(t *testing.T) {
	service, err := NewDefaultService()
	if err != nil {
		t.Fatalf("NewDefaultService() error = %v", err)
	}

	templates, err := service.ListProjectTemplates()
	if err != nil {
		t.Fatalf("ListProjectTemplates() error = %v", err)
	}
	if len(templates.Templates) == 0 {
		t.Fatal("ListProjectTemplates() returned no templates")
	}

	root := t.TempDir()
	for _, template := range templates.Templates {
		template := template
		t.Run(template.TemplateID, func(t *testing.T) {
			projectPath := filepath.Join(root, template.TemplateID)
			if _, err := service.CreateProject(projectPath, template.TemplateID); err != nil {
				t.Fatalf("CreateProject(%q) error = %v", template.TemplateID, err)
			}

			run, err := service.CreateRun(projectPath)
			if err != nil {
				t.Fatalf("CreateRun(%q) error = %v", template.TemplateID, err)
			}
			if _, err := ValidateRunSnapshot(projectPath, run.RunID); err != nil {
				t.Fatalf("ValidateRunSnapshot(%q) error = %v", template.TemplateID, err)
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

// writeWorkspaceTemplate creates a minimal valid project template.
func writeWorkspaceTemplate(t *testing.T, templateDir string) {
	t.Helper()
	if err := os.MkdirAll(filepath.Join(templateDir, "policy"), 0755); err != nil {
		t.Fatalf("MkdirAll() error = %v", err)
	}
	writeFile(t, filepath.Join(templateDir, "setting.json"), `{"schema":"project_setting","version":1}`)
	writeFile(t, filepath.Join(templateDir, "profile.json"), `{"schema":"simulation_profile","version":1}`)
	writeFile(t, filepath.Join(templateDir, "scenario.json"), `{"schema":"scenario","version":1}`)
	writeFile(t, filepath.Join(templateDir, "policy", "__init__.py"), "def create_policy():\n    return None\n")
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
