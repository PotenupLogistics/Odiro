package process

import (
	"encoding/json"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"sync"
	"time"

	"odiro/bridge/internal/workspace"
)

const (
	stateStarting = "starting"
	stateRunning  = "running"
	stateStopping = "stopping"
	stateExited   = "exited"
	stateFailed   = "failed"
)

// Manager starts and tracks simulator processes for created runs.
type Manager struct {
	SimulatorExecutable string

	mu   sync.Mutex
	runs map[string]*trackedRun
}

// RunStatus is the IPC result for simulator process state.
type RunStatus struct {
	RunID       string `json:"runId"`
	ProjectPath string `json:"projectPath"`
	Executable  string `json:"executable"`
	ProcessID   int    `json:"processId,omitempty"`
	PolicyPort  int    `json:"policyPort,omitempty"`
	StatusPath  string `json:"statusPath"`
	State       string `json:"state"`
	StartedAt   string `json:"startedAt,omitempty"`
	UpdatedAt   string `json:"updatedAt,omitempty"`
	ExitedAt    string `json:"exitedAt,omitempty"`
	ExitCode    *int   `json:"exitCode,omitempty"`
	Error       string `json:"error,omitempty"`
}

// trackedRun owns one child process and its latest status.
type trackedRun struct {
	cmd    *exec.Cmd
	status RunStatus
}

// NewManager creates a process manager for one simulator executable.
func NewManager(simulatorExecutable string) *Manager {
	return &Manager{
		SimulatorExecutable: strings.TrimSpace(simulatorExecutable),
		runs:                map[string]*trackedRun{},
	}
}

// StartSimulator validates an existing run snapshot and starts the simulator.
func (manager *Manager) StartSimulator(projectPath string, runID string, policyPort int) (RunStatus, error) {
	snapshot, err := workspace.ValidateRunSnapshot(projectPath, runID)
	if err != nil {
		return RunStatus{}, err
	}
	executable, err := resolveExecutable(manager.SimulatorExecutable)
	if err != nil {
		return RunStatus{}, err
	}
	key := runKey(snapshot.ProjectPath, runID)

	manager.mu.Lock()
	if _, exists := manager.runs[key]; exists {
		manager.mu.Unlock()
		return RunStatus{}, NewError("RUN_ALREADY_TRACKED", "run is already tracked")
	}
	manager.mu.Unlock()

	now := utcNow()
	status := RunStatus{
		RunID:       runID,
		ProjectPath: snapshot.ProjectPath,
		Executable:  executable,
		PolicyPort:  policyPort,
		StatusPath:  snapshot.StatusPath,
		State:       stateStarting,
		StartedAt:   now,
		UpdatedAt:   now,
	}
	if err := writeStatusFile(status); err != nil {
		return RunStatus{}, err
	}

	args := []string{
		"-OdiroProject=" + snapshot.ProjectPath,
		"-RunId=" + runID,
	}
	if policyPort > 0 {
		args = append(args, fmt.Sprintf("-PolicyPort=%d", policyPort))
	}
	cmd := exec.Command(executable, args...)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr

	if err := cmd.Start(); err != nil {
		status.State = stateFailed
		status.UpdatedAt = utcNow()
		status.ExitedAt = status.UpdatedAt
		status.Error = err.Error()
		_ = writeStatusFile(status)
		return RunStatus{}, NewError("PROCESS_START_FAILED", err.Error())
	}

	status.State = stateRunning
	status.ProcessID = cmd.Process.Pid
	status.UpdatedAt = utcNow()
	if err := writeStatusFile(status); err != nil {
		_ = cmd.Process.Kill()
		return RunStatus{}, err
	}

	manager.mu.Lock()
	manager.runs[key] = &trackedRun{cmd: cmd, status: status}
	manager.mu.Unlock()

	go manager.waitForExit(key, cmd)
	return status, nil
}

// GetRunStatus returns the latest status for a Bridge-tracked run.
func (manager *Manager) GetRunStatus(projectPath string, runID string) (RunStatus, error) {
	key, err := manager.checkedRunKey(projectPath, runID)
	if err != nil {
		return RunStatus{}, err
	}
	manager.mu.Lock()
	defer manager.mu.Unlock()
	run, exists := manager.runs[key]
	if !exists {
		return RunStatus{}, NewError("RUN_NOT_TRACKED", "run is not tracked")
	}
	return run.status, nil
}

// StopSimulator requests termination of a Bridge-tracked simulator process.
func (manager *Manager) StopSimulator(projectPath string, runID string) (RunStatus, error) {
	key, err := manager.checkedRunKey(projectPath, runID)
	if err != nil {
		return RunStatus{}, err
	}
	manager.mu.Lock()
	run, exists := manager.runs[key]
	if !exists {
		manager.mu.Unlock()
		return RunStatus{}, NewError("RUN_NOT_TRACKED", "run is not tracked")
	}
	if run.cmd.Process == nil || run.status.State == stateExited || run.status.State == stateFailed {
		status := run.status
		manager.mu.Unlock()
		return status, nil
	}
	run.status.State = stateStopping
	run.status.UpdatedAt = utcNow()
	status := run.status
	manager.mu.Unlock()

	if err := writeStatusFile(status); err != nil {
		return RunStatus{}, err
	}
	if err := run.cmd.Process.Kill(); err != nil {
		status.State = stateFailed
		status.UpdatedAt = utcNow()
		status.ExitedAt = status.UpdatedAt
		status.Error = err.Error()
		_ = writeStatusFile(status)
		return RunStatus{}, NewError("PROCESS_START_FAILED", err.Error())
	}
	return status, nil
}

// checkedRunKey validates request identity before accessing the tracker.
func (manager *Manager) checkedRunKey(projectPath string, runID string) (string, error) {
	if !workspace.IsSafeRunID(runID) {
		return "", NewError("INVALID_REQUEST", "runId must be a 6-digit decimal string")
	}
	projectAbs, err := filepath.Abs(projectPath)
	if err != nil {
		return "", NewError("INVALID_REQUEST", err.Error())
	}
	return runKey(projectAbs, runID), nil
}

// waitForExit updates status after the child process exits.
func (manager *Manager) waitForExit(key string, cmd *exec.Cmd) {
	err := cmd.Wait()
	exitCode := cmd.ProcessState.ExitCode()
	now := utcNow()

	manager.mu.Lock()
	run, exists := manager.runs[key]
	if !exists {
		manager.mu.Unlock()
		return
	}
	run.status.ExitCode = &exitCode
	run.status.UpdatedAt = now
	run.status.ExitedAt = now
	if err == nil && exitCode == 0 {
		run.status.State = stateExited
		run.status.Error = ""
	} else {
		run.status.State = stateFailed
		if err != nil {
			run.status.Error = err.Error()
		} else {
			run.status.Error = fmt.Sprintf("process exited with code %d", exitCode)
		}
	}
	status := run.status
	manager.mu.Unlock()

	_ = writeStatusFile(status)
}

// resolveExecutable validates the configured simulator executable.
func resolveExecutable(executable string) (string, error) {
	if strings.TrimSpace(executable) == "" {
		return "", NewError("SIMULATOR_EXECUTABLE_REQUIRED", "simulator executable is required")
	}
	executableAbs, err := filepath.Abs(executable)
	if err != nil {
		return "", NewError("SIMULATOR_EXECUTABLE_REQUIRED", err.Error())
	}
	info, err := os.Stat(executableAbs)
	if err != nil {
		return "", NewError("SIMULATOR_EXECUTABLE_REQUIRED", err.Error())
	}
	if info.IsDir() {
		return "", NewError("SIMULATOR_EXECUTABLE_REQUIRED", "simulator executable is a directory")
	}
	return executableAbs, nil
}

// runKey creates the in-memory tracker key.
func runKey(projectPath string, runID string) string {
	return filepath.Clean(projectPath) + "\x00" + runID
}

// utcNow returns a stable UTC timestamp for IPC and status files.
func utcNow() string {
	return time.Now().UTC().Format(time.RFC3339)
}

// statusFile is the persisted run_status JSON shape.
type statusFile struct {
	Schema    string        `json:"schema"`
	Version   int           `json:"version"`
	Run       statusRun     `json:"run"`
	Process   statusProcess `json:"process"`
	State     string        `json:"state"`
	StartedAt string        `json:"started_at,omitempty"`
	UpdatedAt string        `json:"updated_at"`
	ExitedAt  string        `json:"exited_at,omitempty"`
	Error     string        `json:"error,omitempty"`
}

// statusRun contains run identity in status.json.
type statusRun struct {
	ProjectPath string `json:"project_path"`
	RunID       string `json:"run_id"`
	StatusPath  string `json:"status_path"`
}

// statusProcess contains child process metadata in status.json.
type statusProcess struct {
	Executable string `json:"executable"`
	ProcessID  int    `json:"process_id,omitempty"`
	PolicyPort int    `json:"policy_port,omitempty"`
	ExitCode   *int   `json:"exit_code,omitempty"`
}

// writeStatusFile persists the current Bridge-owned run status.
func writeStatusFile(status RunStatus) error {
	payload := statusFile{
		Schema:  "run_status",
		Version: 1,
		Run: statusRun{
			ProjectPath: status.ProjectPath,
			RunID:       status.RunID,
			StatusPath:  status.StatusPath,
		},
		Process: statusProcess{
			Executable: status.Executable,
			ProcessID:  status.ProcessID,
			PolicyPort: status.PolicyPort,
			ExitCode:   status.ExitCode,
		},
		State:     status.State,
		StartedAt: status.StartedAt,
		UpdatedAt: status.UpdatedAt,
		ExitedAt:  status.ExitedAt,
		Error:     status.Error,
	}
	encoded, err := json.MarshalIndent(payload, "", "  ")
	if err != nil {
		return NewError("PROJECT_INVALID", err.Error())
	}
	if err := os.WriteFile(status.StatusPath, append(encoded, '\n'), 0644); err != nil {
		return NewError("PROJECT_INVALID", err.Error())
	}
	return nil
}
