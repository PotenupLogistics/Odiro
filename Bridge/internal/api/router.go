package api

import (
	"encoding/json"
	"fmt"
	"strings"

	bridgeprocess "odiro/bridge/internal/process"
	"odiro/bridge/internal/protocol"
	"odiro/bridge/internal/workspace"
)

// Router dispatches validated protocol requests to Bridge services.
type Router struct {
	Workspace *workspace.Service
	Process   *bridgeprocess.Manager
}

// NewRouter creates a router from explicit service instances.
func NewRouter(workspaceService *workspace.Service, processManager *bridgeprocess.Manager) *Router {
	return &Router{
		Workspace: workspaceService,
		Process:   processManager,
	}
}

// NewDefaultRouter creates a router using development or release resources.
func NewDefaultRouter(simulatorExecutable string) (*Router, error) {
	workspaceService, err := workspace.NewDefaultService()
	if err != nil {
		return nil, err
	}
	return NewRouter(workspaceService, bridgeprocess.NewManager(simulatorExecutable)), nil
}

// Handle dispatches one protocol request and returns a normalized response.
func (router *Router) Handle(request protocol.Request) protocol.Response {
	switch request.Method {
	case "ping":
		return protocol.SuccessResponse(request.ID, protocol.PingResult{Status: "ok"})
	case "workspace.validateProject":
		return router.handleValidateProject(request)
	case "workspace.listProjectPresets":
		return router.handleListProjectPresets(request)
	case "workspace.createProject":
		return router.handleCreateProject(request)
	case "workspace.createRun":
		return router.handleCreateRun(request)
	case "process.startSimulator":
		return router.handleStartSimulator(request)
	case "process.getRunStatus":
		return router.handleGetRunStatus(request)
	case "process.stopSimulator":
		return router.handleStopSimulator(request)
	default:
		return protocol.ErrorResponse(request.ID, "UNKNOWN_METHOD", fmt.Sprintf("unknown method %q", request.Method))
	}
}

// validateProjectParams is the request payload for workspace.validateProject.
type validateProjectParams struct {
	ProjectPath string `json:"projectPath"`
}

// createProjectParams is the request payload for workspace.createProject.
type createProjectParams struct {
	ProjectPath     string                            `json:"projectPath"`
	PresetSelection *workspace.ProjectPresetSelection `json:"presetSelection"`
}

// createRunParams is the request payload for workspace.createRun.
type createRunParams struct {
	ProjectPath string `json:"projectPath"`
}

// runProcessParams is the request payload for process lifecycle methods.
type runProcessParams struct {
	ProjectPath string `json:"projectPath"`
	RunID       string `json:"runId"`
	PolicyPort  int    `json:"policyPort,omitempty"`
}

// codedError exposes stable IPC error codes from service packages.
type codedError interface {
	Code() string
	Error() string
}

// handleValidateProject validates a project root.
func (router *Router) handleValidateProject(request protocol.Request) protocol.Response {
	params, err := decodeParams[validateProjectParams](request.Params)
	if err != nil {
		return errorResponse(request.ID, err)
	}
	if err := requireString("projectPath", params.ProjectPath); err != nil {
		return errorResponse(request.ID, err)
	}
	result, err := router.Workspace.ValidateProject(params.ProjectPath)
	if err != nil {
		return errorResponse(request.ID, err)
	}
	return protocol.SuccessResponse(request.ID, result)
}

// handleListProjectPresets returns available preset IDs.
func (router *Router) handleListProjectPresets(request protocol.Request) protocol.Response {
	result, err := router.Workspace.ListProjectPresets()
	if err != nil {
		return errorResponse(request.ID, err)
	}
	return protocol.SuccessResponse(request.ID, result)
}

// handleCreateProject copies selected presets to a project root.
func (router *Router) handleCreateProject(request protocol.Request) protocol.Response {
	params, err := decodeParams[createProjectParams](request.Params)
	if err != nil {
		return errorResponse(request.ID, err)
	}
	if err := requireString("projectPath", params.ProjectPath); err != nil {
		return errorResponse(request.ID, err)
	}
	if params.PresetSelection == nil {
		return errorResponse(request.ID, workspace.NewError("INVALID_REQUEST", "presetSelection is required"))
	}
	if err := requireString("presetSelection.scenarioPresetId", params.PresetSelection.ScenarioPresetID); err != nil {
		return errorResponse(request.ID, err)
	}
	if err := requireString("presetSelection.profilePresetId", params.PresetSelection.ProfilePresetID); err != nil {
		return errorResponse(request.ID, err)
	}
	if err := requireString("presetSelection.policyPresetId", params.PresetSelection.PolicyPresetID); err != nil {
		return errorResponse(request.ID, err)
	}
	result, err := router.Workspace.CreateProject(params.ProjectPath, *params.PresetSelection)
	if err != nil {
		return errorResponse(request.ID, err)
	}
	return protocol.SuccessResponse(request.ID, result)
}

// handleCreateRun creates a run directory and input snapshot.
func (router *Router) handleCreateRun(request protocol.Request) protocol.Response {
	params, err := decodeParams[createRunParams](request.Params)
	if err != nil {
		return errorResponse(request.ID, err)
	}
	if err := requireString("projectPath", params.ProjectPath); err != nil {
		return errorResponse(request.ID, err)
	}
	result, err := router.Workspace.CreateRun(params.ProjectPath)
	if err != nil {
		return errorResponse(request.ID, err)
	}
	return protocol.SuccessResponse(request.ID, result)
}

// handleStartSimulator starts a simulator process for an existing run.
func (router *Router) handleStartSimulator(request protocol.Request) protocol.Response {
	params, err := decodeParams[runProcessParams](request.Params)
	if err != nil {
		return errorResponse(request.ID, err)
	}
	if err := requireRunParams(params); err != nil {
		return errorResponse(request.ID, err)
	}
	result, err := router.Process.StartSimulator(params.ProjectPath, params.RunID, params.PolicyPort)
	if err != nil {
		return errorResponse(request.ID, err)
	}
	return protocol.SuccessResponse(request.ID, result)
}

// handleGetRunStatus returns Bridge-tracked process status.
func (router *Router) handleGetRunStatus(request protocol.Request) protocol.Response {
	params, err := decodeParams[runProcessParams](request.Params)
	if err != nil {
		return errorResponse(request.ID, err)
	}
	if err := requireRunParams(params); err != nil {
		return errorResponse(request.ID, err)
	}
	result, err := router.Process.GetRunStatus(params.ProjectPath, params.RunID)
	if err != nil {
		return errorResponse(request.ID, err)
	}
	return protocol.SuccessResponse(request.ID, result)
}

// handleStopSimulator requests termination of a Bridge-tracked process.
func (router *Router) handleStopSimulator(request protocol.Request) protocol.Response {
	params, err := decodeParams[runProcessParams](request.Params)
	if err != nil {
		return errorResponse(request.ID, err)
	}
	if err := requireRunParams(params); err != nil {
		return errorResponse(request.ID, err)
	}
	result, err := router.Process.StopSimulator(params.ProjectPath, params.RunID)
	if err != nil {
		return errorResponse(request.ID, err)
	}
	return protocol.SuccessResponse(request.ID, result)
}

// decodeParams parses a method params object.
func decodeParams[T any](raw json.RawMessage) (T, error) {
	var params T
	if len(raw) == 0 || string(raw) == "null" {
		return params, nil
	}
	if err := json.Unmarshal(raw, &params); err != nil {
		return params, workspace.NewError("INVALID_REQUEST", err.Error())
	}
	return params, nil
}

// requireRunParams validates shared run process payload fields.
func requireRunParams(params runProcessParams) error {
	if err := requireString("projectPath", params.ProjectPath); err != nil {
		return err
	}
	return requireString("runId", params.RunID)
}

// requireString validates required string payload fields.
func requireString(name string, value string) error {
	if strings.TrimSpace(value) == "" {
		return workspace.NewError("INVALID_REQUEST", name+" is required")
	}
	return nil
}

// errorResponse maps service errors into the protocol error shape.
func errorResponse(id string, err error) protocol.Response {
	if coded, ok := err.(codedError); ok {
		return protocol.ErrorResponse(id, coded.Code(), coded.Error())
	}
	return protocol.ErrorResponse(id, "INVALID_REQUEST", err.Error())
}
