package workspace

import (
	"encoding/json"
	"fmt"
	"io/fs"
	"os"
	"path/filepath"
	"regexp"
	"sort"
	"strings"
)

const (
	schemaProjectSetting    = "project_setting"
	schemaSimulationProfile = "simulation_profile"
	schemaScenario          = "scenario"
	maxRunID                = 999999
)

var safeSegmentPattern = regexp.MustCompile(`^[A-Za-z0-9][A-Za-z0-9._-]*$`)
var runIDPattern = regexp.MustCompile(`^\d{6}$`)

// Service owns user project file and static resource operations.
type Service struct {
	ProjectPresetsDir string
	RunDefaultsDir    string
}

// ProjectPresetSelection describes selected file-level presets.
type ProjectPresetSelection struct {
	ScenarioPresetID string `json:"scenarioPresetId"`
	ProfilePresetID  string `json:"profilePresetId"`
	PolicyPresetID   string `json:"policyPresetId"`
}

// ProjectPresetCatalog is the IPC result for preset discovery.
type ProjectPresetCatalog struct {
	ScenarioPresetIDs []string `json:"scenarioPresetIds"`
	ProfilePresetIDs  []string `json:"profilePresetIds"`
	PolicyPresetIDs   []string `json:"policyPresetIds"`
}

// ProjectInfo describes validated project paths.
type ProjectInfo struct {
	PresetSelection *ProjectPresetSelection `json:"presetSelection,omitempty"`
	ProjectPath     string                  `json:"projectPath"`
	SettingPath     string                  `json:"settingPath"`
	ProfilePath     string                  `json:"profilePath"`
	ScenarioPath    string                  `json:"scenarioPath"`
	PolicyPath      string                  `json:"policyPath"`
	RunsPath        string                  `json:"runsPath"`
}

// ValidateProjectResult is the IPC result for project validation.
type ValidateProjectResult struct {
	ProjectPath  string `json:"projectPath"`
	SettingPath  string `json:"settingPath"`
	ProfilePath  string `json:"profilePath"`
	ScenarioPath string `json:"scenarioPath"`
	PolicyPath   string `json:"policyPath"`
	RunsPath     string `json:"runsPath"`
}

// CreateProjectResult is the IPC result for project creation.
type CreateProjectResult struct {
	Project      ProjectInfo `json:"project"`
	CreatedPaths []string    `json:"createdPaths"`
}

// CreateRunResult is the IPC result for run creation.
type CreateRunResult struct {
	ProjectPath   string   `json:"projectPath"`
	RunID         string   `json:"runId"`
	RunPath       string   `json:"runPath"`
	SnapshotPath  string   `json:"snapshotPath"`
	StatusPath    string   `json:"statusPath"`
	SummaryPath   string   `json:"summaryPath"`
	ReviewPath    string   `json:"reviewPath"`
	EpisodesPath  string   `json:"episodesPath"`
	SnapshotPaths []string `json:"snapshotPaths"`
}

// RunSnapshotInfo describes a created run snapshot ready for execution.
type RunSnapshotInfo struct {
	ProjectPath  string `json:"projectPath"`
	RunID        string `json:"runId"`
	RunPath      string `json:"runPath"`
	SnapshotPath string `json:"snapshotPath"`
	StatusPath   string `json:"statusPath"`
	SummaryPath  string `json:"summaryPath"`
	ReviewPath   string `json:"reviewPath"`
	EpisodesPath string `json:"episodesPath"`
}

// NewService creates a workspace service from explicit resource directories.
func NewService(projectPresetsDir string, runDefaultsDir string) *Service {
	return &Service{
		ProjectPresetsDir: filepath.Clean(projectPresetsDir),
		RunDefaultsDir:    filepath.Clean(runDefaultsDir),
	}
}

// NewDefaultService locates development static resources or release resources.
func NewDefaultService() (*Service, error) {
	root, mode, err := findResourceRoot()
	if err != nil {
		return nil, err
	}
	switch mode {
	case "static":
		return NewService(
			filepath.Join(root, "static", "templates"),
			filepath.Join(root, "static", "run-defaults"),
		), nil
	default:
		return NewService(
			filepath.Join(root, "resources", "templates"),
			filepath.Join(root, "resources", "run-defaults"),
		), nil
	}
}

// ListProjectPresets returns safe preset IDs from the resource template folders.
func (service *Service) ListProjectPresets() (ProjectPresetCatalog, error) {
	scenarioPresetIDs, err := listPresetJSONIDs(filepath.Join(service.ProjectPresetsDir, "scenario"))
	if err != nil {
		return ProjectPresetCatalog{}, err
	}
	profilePresetIDs, err := listPresetJSONIDs(filepath.Join(service.ProjectPresetsDir, "profile"))
	if err != nil {
		return ProjectPresetCatalog{}, err
	}
	policyPresetIDs, err := listPresetIDs(filepath.Join(service.ProjectPresetsDir, "policy"))
	if err != nil {
		return ProjectPresetCatalog{}, err
	}
	return ProjectPresetCatalog{
		ScenarioPresetIDs: scenarioPresetIDs,
		ProfilePresetIDs:  profilePresetIDs,
		PolicyPresetIDs:   policyPresetIDs,
	}, nil
}

// listPresetIDs returns safe direct child directory names.
func listPresetIDs(categoryDir string) ([]string, error) {
	entries, err := os.ReadDir(categoryDir)
	if err != nil {
		return nil, NewError("PROJECT_PRESET_INVALID", err.Error())
	}
	presetIDs := []string{}
	for _, entry := range entries {
		if !entry.IsDir() {
			continue
		}
		presetID := entry.Name()
		if !isSafeSegment(presetID) {
			continue
		}
		presetIDs = append(presetIDs, presetID)
	}
	sort.Strings(presetIDs)
	return presetIDs, nil
}

// listPresetJSONIDs returns safe direct child JSON file base names.
func listPresetJSONIDs(categoryDir string) ([]string, error) {
	entries, err := os.ReadDir(categoryDir)
	if err != nil {
		return nil, NewError("PROJECT_PRESET_INVALID", err.Error())
	}
	presetIDs := []string{}
	for _, entry := range entries {
		if entry.IsDir() || !strings.EqualFold(filepath.Ext(entry.Name()), ".json") {
			continue
		}
		presetID := strings.TrimSuffix(entry.Name(), filepath.Ext(entry.Name()))
		if !isSafeSegment(presetID) {
			continue
		}
		presetIDs = append(presetIDs, presetID)
	}
	sort.Strings(presetIDs)
	return presetIDs, nil
}

// ValidateProject validates a user project root without mutating it.
func (service *Service) ValidateProject(projectPath string) (ValidateProjectResult, error) {
	info, err := service.validateProjectInfo(projectPath)
	if err != nil {
		return ValidateProjectResult{}, err
	}
	return ValidateProjectResult{
		ProjectPath:  info.ProjectPath,
		SettingPath:  info.SettingPath,
		ProfilePath:  info.ProfilePath,
		ScenarioPath: info.ScenarioPath,
		PolicyPath:   info.PolicyPath,
		RunsPath:     info.RunsPath,
	}, nil
}

// CreateProject copies selected presets into a new or empty project root.
func (service *Service) CreateProject(projectPath string, selection ProjectPresetSelection) (CreateProjectResult, error) {
	selection = normalizePresetSelection(selection)
	if !isSafeSegment(selection.ScenarioPresetID) {
		return CreateProjectResult{}, NewError("INVALID_REQUEST", "scenarioPresetId must be a safe path segment")
	}
	if !isSafeSegment(selection.ProfilePresetID) {
		return CreateProjectResult{}, NewError("INVALID_REQUEST", "profilePresetId must be a safe path segment")
	}
	if !isSafeSegment(selection.PolicyPresetID) {
		return CreateProjectResult{}, NewError("INVALID_REQUEST", "policyPresetId must be a safe path segment")
	}

	sources := projectPresetSourcePaths{
		SettingPath:  filepath.Join(service.ProjectPresetsDir, "setting.json"),
		ScenarioPath: filepath.Join(service.ProjectPresetsDir, "scenario", selection.ScenarioPresetID+".json"),
		ProfilePath:  filepath.Join(service.ProjectPresetsDir, "profile", selection.ProfilePresetID+".json"),
		PolicyPath:   filepath.Join(service.ProjectPresetsDir, "policy", selection.PolicyPresetID),
	}
	if err := service.validateProjectPresets(sources); err != nil {
		return CreateProjectResult{}, err
	}
	projectAbs, err := filepath.Abs(projectPath)
	if err != nil {
		return CreateProjectResult{}, NewError("INVALID_REQUEST", err.Error())
	}
	if err := ensureNotRelatedPath(projectAbs, service.ProjectPresetsDir); err != nil {
		return CreateProjectResult{}, err
	}
	if err := ensureEmptyOrMissingDirectory(projectAbs); err != nil {
		return CreateProjectResult{}, err
	}
	if err := os.MkdirAll(projectAbs, 0755); err != nil {
		return CreateProjectResult{}, NewError("PROJECT_INVALID", err.Error())
	}
	createdPaths := []string{}
	for _, item := range []struct {
		Source string
		Target string
	}{
		{Source: sources.SettingPath, Target: filepath.Join(projectAbs, "setting.json")},
		{Source: sources.ProfilePath, Target: filepath.Join(projectAbs, "profile.json")},
		{Source: sources.ScenarioPath, Target: filepath.Join(projectAbs, "scenario.json")},
	} {
		if err := copyFile(item.Source, item.Target); err != nil {
			return CreateProjectResult{}, err
		}
		createdPaths = append(createdPaths, filepath.ToSlash(strings.TrimPrefix(item.Target, projectAbs+string(filepath.Separator))))
	}
	policyPaths, err := copyTree(sources.PolicyPath, filepath.Join(projectAbs, "policy"), copyOptions{SkipGitKeep: false})
	if err != nil {
		return CreateProjectResult{}, err
	}
	for _, relativePath := range policyPaths {
		createdPaths = append(createdPaths, filepath.ToSlash(filepath.Join("policy", relativePath)))
	}
	if err := os.MkdirAll(filepath.Join(projectAbs, "runs"), 0755); err != nil {
		return CreateProjectResult{}, NewError("PROJECT_INVALID", err.Error())
	}
	sort.Strings(createdPaths)

	info, err := service.validateProjectInfo(projectAbs)
	if err != nil {
		return CreateProjectResult{}, err
	}
	info.PresetSelection = &selection
	return CreateProjectResult{
		Project:      info,
		CreatedPaths: createdPaths,
	}, nil
}

// CreateRun creates a new run directory and snapshots current project inputs.
func (service *Service) CreateRun(projectPath string) (CreateRunResult, error) {
	project, err := service.validateProjectInfo(projectPath)
	if err != nil {
		return CreateRunResult{}, err
	}
	runID, err := nextRunID(project.RunsPath)
	if err != nil {
		return CreateRunResult{}, err
	}
	runPath := filepath.Join(project.RunsPath, runID)
	if err := os.Mkdir(runPath, 0755); err != nil {
		return CreateRunResult{}, NewError("PROJECT_INVALID", err.Error())
	}
	if _, err := copyTree(service.RunDefaultsDir, runPath, copyOptions{SkipGitKeep: true}); err != nil {
		return CreateRunResult{}, err
	}
	snapshotPath := filepath.Join(runPath, "snapshot")
	if err := os.MkdirAll(snapshotPath, 0755); err != nil {
		return CreateRunResult{}, NewError("PROJECT_INVALID", err.Error())
	}

	snapshotPaths := []string{}
	for _, name := range []string{"setting.json", "profile.json", "scenario.json"} {
		sourcePath := filepath.Join(project.ProjectPath, name)
		targetPath := filepath.Join(snapshotPath, name)
		if err := copyFile(sourcePath, targetPath); err != nil {
			return CreateRunResult{}, err
		}
		snapshotPaths = append(snapshotPaths, targetPath)
	}
	policyPaths, err := copyTree(project.PolicyPath, filepath.Join(snapshotPath, "policy"), copyOptions{SkipGitKeep: false})
	if err != nil {
		return CreateRunResult{}, err
	}
	for _, relativePath := range policyPaths {
		snapshotPaths = append(snapshotPaths, filepath.Join(snapshotPath, "policy", relativePath))
	}
	sort.Strings(snapshotPaths)

	return CreateRunResult{
		ProjectPath:   project.ProjectPath,
		RunID:         runID,
		RunPath:       runPath,
		SnapshotPath:  snapshotPath,
		StatusPath:    filepath.Join(runPath, "status.json"),
		SummaryPath:   filepath.Join(runPath, "summary.json"),
		ReviewPath:    filepath.Join(runPath, "review"),
		EpisodesPath:  filepath.Join(runPath, "episodes"),
		SnapshotPaths: snapshotPaths,
	}, nil
}

// IsSafeRunID reports whether a path segment is a 6-digit run ID.
func IsSafeRunID(runID string) bool {
	return runIDPattern.MatchString(runID)
}

// ValidateRunSnapshot validates the run-local execution inputs.
func ValidateRunSnapshot(projectPath string, runID string) (RunSnapshotInfo, error) {
	if !IsSafeRunID(runID) {
		return RunSnapshotInfo{}, NewError("INVALID_REQUEST", "runId must be a 6-digit decimal string")
	}
	projectAbs, err := filepath.Abs(projectPath)
	if err != nil {
		return RunSnapshotInfo{}, NewError("INVALID_REQUEST", err.Error())
	}
	if err := requireDir(projectAbs, "PROJECT_INVALID"); err != nil {
		return RunSnapshotInfo{}, err
	}

	runPath := filepath.Join(projectAbs, "runs", runID)
	snapshotPath := filepath.Join(runPath, "snapshot")
	reviewPath := filepath.Join(runPath, "review")
	episodesPath := filepath.Join(runPath, "episodes")

	for _, path := range []string{runPath, snapshotPath, reviewPath, episodesPath} {
		if err := requireDir(path, "PROJECT_INVALID"); err != nil {
			return RunSnapshotInfo{}, err
		}
	}
	for _, item := range []struct {
		Path   string
		Schema string
	}{
		{Path: filepath.Join(snapshotPath, "setting.json"), Schema: schemaProjectSetting},
		{Path: filepath.Join(snapshotPath, "profile.json"), Schema: schemaSimulationProfile},
		{Path: filepath.Join(snapshotPath, "scenario.json"), Schema: schemaScenario},
	} {
		if err := validateJSONSchema(item.Path, item.Schema, "PROJECT_INVALID"); err != nil {
			return RunSnapshotInfo{}, err
		}
	}
	if err := validatePolicyPackage(filepath.Join(snapshotPath, "policy"), "PROJECT_INVALID"); err != nil {
		return RunSnapshotInfo{}, err
	}

	return RunSnapshotInfo{
		ProjectPath:  projectAbs,
		RunID:        runID,
		RunPath:      runPath,
		SnapshotPath: snapshotPath,
		StatusPath:   filepath.Join(runPath, "status.json"),
		SummaryPath:  filepath.Join(runPath, "summary.json"),
		ReviewPath:   reviewPath,
		EpisodesPath: episodesPath,
	}, nil
}

// projectPresetSourcePaths contains resolved source paths for project creation.
type projectPresetSourcePaths struct {
	SettingPath  string
	ProfilePath  string
	ScenarioPath string
	PolicyPath   string
}

// normalizePresetSelection applies the default create-project preset choices.
func normalizePresetSelection(selection ProjectPresetSelection) ProjectPresetSelection {
	if strings.TrimSpace(selection.ScenarioPresetID) == "" {
		selection.ScenarioPresetID = "blank"
	}
	if strings.TrimSpace(selection.ProfilePresetID) == "" {
		selection.ProfilePresetID = "basic"
	}
	if strings.TrimSpace(selection.PolicyPresetID) == "" {
		selection.PolicyPresetID = "blank"
	}
	return selection
}

// validateProjectInfo returns canonical paths after contract validation.
func (service *Service) validateProjectInfo(projectPath string) (ProjectInfo, error) {
	projectAbs, err := filepath.Abs(projectPath)
	if err != nil {
		return ProjectInfo{}, NewError("INVALID_REQUEST", err.Error())
	}
	if err := rejectSymlink(projectAbs); err != nil {
		return ProjectInfo{}, err
	}
	if err := requireDir(projectAbs, "PROJECT_INVALID"); err != nil {
		return ProjectInfo{}, err
	}
	settingPath := filepath.Join(projectAbs, "setting.json")
	profilePath := filepath.Join(projectAbs, "profile.json")
	scenarioPath := filepath.Join(projectAbs, "scenario.json")
	policyPath := filepath.Join(projectAbs, "policy")
	runsPath := filepath.Join(projectAbs, "runs")

	for _, item := range []struct {
		Path   string
		Schema string
	}{
		{Path: settingPath, Schema: schemaProjectSetting},
		{Path: profilePath, Schema: schemaSimulationProfile},
		{Path: scenarioPath, Schema: schemaScenario},
	} {
		if err := validateJSONSchema(item.Path, item.Schema, "PROJECT_INVALID"); err != nil {
			return ProjectInfo{}, err
		}
	}
	if err := validatePolicyPackage(policyPath, "PROJECT_INVALID"); err != nil {
		return ProjectInfo{}, err
	}
	if err := requireDir(runsPath, "PROJECT_INVALID"); err != nil {
		return ProjectInfo{}, err
	}
	return ProjectInfo{
		ProjectPath:  projectAbs,
		SettingPath:  settingPath,
		ProfilePath:  profilePath,
		ScenarioPath: scenarioPath,
		PolicyPath:   policyPath,
		RunsPath:     runsPath,
	}, nil
}

// validateProjectPresets checks selected preset content before it is copied.
func (service *Service) validateProjectPresets(sources projectPresetSourcePaths) error {
	for _, item := range []struct {
		Path   string
		Schema string
	}{
		{Path: sources.SettingPath, Schema: schemaProjectSetting},
		{Path: sources.ProfilePath, Schema: schemaSimulationProfile},
		{Path: sources.ScenarioPath, Schema: schemaScenario},
	} {
		if err := validateJSONSchema(item.Path, item.Schema, "PROJECT_PRESET_INVALID"); err != nil {
			return err
		}
	}
	if err := validatePolicyPackage(sources.PolicyPath, "PROJECT_PRESET_INVALID"); err != nil {
		return err
	}
	return filepath.WalkDir(sources.PolicyPath, func(path string, entry fs.DirEntry, walkErr error) error {
		if walkErr != nil {
			return NewError("PROJECT_PRESET_INVALID", walkErr.Error())
		}
		name := entry.Name()
		if name == "." {
			return nil
		}
		if entry.Type()&fs.ModeSymlink != 0 {
			return NewError("PROJECT_PRESET_INVALID", fmt.Sprintf("symlink is not allowed: %s", path))
		}
		if isGeneratedPythonCache(entry) {
			return NewError("PROJECT_PRESET_INVALID", fmt.Sprintf("generated Python cache is not allowed: %s", path))
		}
		if name == "policy-runtime.py" || name == "server.py" {
			return NewError("PROJECT_PRESET_INVALID", fmt.Sprintf("forbidden preset file: %s", name))
		}
		return nil
	})
}

// findResourceRoot searches upward for development static or release resources.
func findResourceRoot() (string, string, error) {
	starts := []string{}
	if cwd, err := os.Getwd(); err == nil {
		starts = append(starts, cwd)
	}
	if executable, err := os.Executable(); err == nil {
		starts = append(starts, filepath.Dir(executable))
	}
	for _, start := range starts {
		root, mode, ok := searchResourceRoot(start)
		if ok {
			return root, mode, nil
		}
	}
	return "", "", NewError("PROJECT_PRESET_INVALID", "project presets resource not found")
}

// searchResourceRoot searches upward from one starting directory.
func searchResourceRoot(start string) (string, string, bool) {
	current := filepath.Clean(start)
	for {
		if dirExists(filepath.Join(current, "static", "templates")) {
			return current, "static", true
		}
		if dirExists(filepath.Join(current, "resources", "templates")) {
			return current, "resources", true
		}
		parent := filepath.Dir(current)
		if parent == current {
			return "", "", false
		}
		current = parent
	}
}

// validateJSONSchema checks root object schema and version fields.
func validateJSONSchema(path string, expectedSchema string, code string) error {
	if err := rejectSymlink(path); err != nil {
		return NewError(code, err.Error())
	}
	payload, err := os.ReadFile(path)
	if err != nil {
		return NewError(code, err.Error())
	}
	var root map[string]any
	if err := json.Unmarshal(payload, &root); err != nil {
		return NewError(code, err.Error())
	}
	if root["schema"] != expectedSchema {
		return NewError(code, fmt.Sprintf("%s schema must be %q", path, expectedSchema))
	}
	if root["version"] != float64(1) {
		return NewError(code, fmt.Sprintf("%s version must be 1", path))
	}
	return nil
}

// validatePolicyPackage checks the project policy entrypoint.
func validatePolicyPackage(policyPath string, code string) error {
	if err := requireDir(policyPath, code); err != nil {
		return err
	}
	entrypoint := filepath.Join(policyPath, "__init__.py")
	if err := rejectSymlink(entrypoint); err != nil {
		return NewError(code, err.Error())
	}
	payload, err := os.ReadFile(entrypoint)
	if err != nil {
		return NewError(code, err.Error())
	}
	if !strings.Contains(string(payload), "create_policy") {
		return NewError(code, "policy/__init__.py must expose create_policy")
	}
	return nil
}

// nextRunID returns the next 6-digit run ID.
func nextRunID(runsPath string) (string, error) {
	entries, err := os.ReadDir(runsPath)
	if err != nil {
		return "", NewError("PROJECT_INVALID", err.Error())
	}
	maxID := 0
	for _, entry := range entries {
		if !entry.IsDir() || !runIDPattern.MatchString(entry.Name()) {
			continue
		}
		var value int
		if _, err := fmt.Sscanf(entry.Name(), "%06d", &value); err != nil {
			continue
		}
		if value > maxID {
			maxID = value
		}
	}
	if maxID >= maxRunID {
		return "", NewError("PROJECT_INVALID", "run id overflow")
	}
	return fmt.Sprintf("%06d", maxID+1), nil
}

// copyOptions controls static copy filtering.
type copyOptions struct {
	SkipGitKeep bool
}

// copyTree recursively copies files while excluding generated Python cache.
func copyTree(sourceRoot string, targetRoot string, options copyOptions) ([]string, error) {
	if err := requireDir(sourceRoot, "PROJECT_INVALID"); err != nil {
		return nil, err
	}
	createdFiles := []string{}
	err := filepath.WalkDir(sourceRoot, func(path string, entry fs.DirEntry, walkErr error) error {
		if walkErr != nil {
			return NewError("PROJECT_INVALID", walkErr.Error())
		}
		name := entry.Name()
		if name == "__pycache__" && entry.IsDir() {
			return filepath.SkipDir
		}
		if entry.Type()&fs.ModeSymlink != 0 {
			return NewError("PROJECT_INVALID", fmt.Sprintf("symlink is not allowed: %s", path))
		}
		relative, err := filepath.Rel(sourceRoot, path)
		if err != nil {
			return NewError("PROJECT_INVALID", err.Error())
		}
		if relative == "." {
			return nil
		}
		targetPath := filepath.Join(targetRoot, relative)
		if entry.IsDir() {
			return os.MkdirAll(targetPath, 0755)
		}
		if shouldSkipFile(name, options) {
			return nil
		}
		if err := copyFile(path, targetPath); err != nil {
			return err
		}
		createdFiles = append(createdFiles, filepath.ToSlash(relative))
		return nil
	})
	if err != nil {
		return nil, err
	}
	return createdFiles, nil
}

// copyFile copies one regular file and creates its parent directory.
func copyFile(sourcePath string, targetPath string) error {
	if err := rejectSymlink(sourcePath); err != nil {
		return NewError("PROJECT_INVALID", err.Error())
	}
	payload, err := os.ReadFile(sourcePath)
	if err != nil {
		return NewError("PROJECT_INVALID", err.Error())
	}
	if err := os.MkdirAll(filepath.Dir(targetPath), 0755); err != nil {
		return NewError("PROJECT_INVALID", err.Error())
	}
	if err := os.WriteFile(targetPath, payload, 0644); err != nil {
		return NewError("PROJECT_INVALID", err.Error())
	}
	return nil
}

// shouldSkipFile reports generated or source-only marker files.
func shouldSkipFile(name string, options copyOptions) bool {
	if options.SkipGitKeep && name == ".gitkeep" {
		return true
	}
	return strings.HasSuffix(name, ".pyc") || strings.HasSuffix(name, ".pyo")
}

// isGeneratedPythonCache reports files and folders generated by Python import.
func isGeneratedPythonCache(entry fs.DirEntry) bool {
	name := entry.Name()
	return name == "__pycache__" || strings.HasSuffix(name, ".pyc") || strings.HasSuffix(name, ".pyo")
}

// ensureEmptyOrMissingDirectory rejects existing non-empty targets.
func ensureEmptyOrMissingDirectory(path string) error {
	info, err := os.Stat(path)
	if os.IsNotExist(err) {
		return nil
	}
	if err != nil {
		return NewError("PROJECT_INVALID", err.Error())
	}
	if !info.IsDir() {
		return NewError("PROJECT_EXISTS", "target project path is not a directory")
	}
	entries, err := os.ReadDir(path)
	if err != nil {
		return NewError("PROJECT_INVALID", err.Error())
	}
	if len(entries) > 0 {
		return NewError("PROJECT_EXISTS", "target project directory is not empty")
	}
	return nil
}

// ensureNotRelatedPath rejects copying preset resources into themselves.
func ensureNotRelatedPath(projectPath string, presetPath string) error {
	projectAbs, _ := filepath.Abs(projectPath)
	presetAbs, _ := filepath.Abs(presetPath)
	if sameOrChild(projectAbs, presetAbs) || sameOrChild(presetAbs, projectAbs) {
		return NewError("INVALID_REQUEST", "projectPath and preset source must not overlap")
	}
	return nil
}

// sameOrChild reports whether child is base or below base.
func sameOrChild(child string, base string) bool {
	relative, err := filepath.Rel(base, child)
	if err != nil {
		return false
	}
	return relative == "." || (!strings.HasPrefix(relative, "..") && !filepath.IsAbs(relative))
}

// rejectSymlink rejects symlink paths at the checked target.
func rejectSymlink(path string) error {
	info, err := os.Lstat(path)
	if err != nil {
		return err
	}
	if info.Mode()&fs.ModeSymlink != 0 {
		return NewError("PROJECT_INVALID", fmt.Sprintf("symlink is not allowed: %s", path))
	}
	return nil
}

// requireDir validates an existing directory.
func requireDir(path string, code string) error {
	if err := rejectSymlink(path); err != nil {
		return NewError(code, err.Error())
	}
	info, err := os.Stat(path)
	if err != nil {
		return NewError(code, err.Error())
	}
	if !info.IsDir() {
		return NewError(code, fmt.Sprintf("not a directory: %s", path))
	}
	return nil
}

// dirExists reports whether a path is an existing directory.
func dirExists(path string) bool {
	info, err := os.Stat(path)
	return err == nil && info.IsDir()
}

// isSafeSegment validates a single path segment used as an ID.
func isSafeSegment(value string) bool {
	if value == "." || value == ".." || filepath.IsAbs(value) {
		return false
	}
	if strings.ContainsAny(value, `/\`) {
		return false
	}
	return safeSegmentPattern.MatchString(value)
}
