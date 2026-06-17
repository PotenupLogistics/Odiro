package protocol

import (
	"encoding/json"
	"fmt"
	"io"
)

const Version = 1

// --- Request/Response contract ---

type Request struct {
	Version int             `json:"version"`
	ID      string          `json:"id"`
	Method  string          `json:"method"`
	Params  json.RawMessage `json:"params,omitempty"`
}

type Response struct {
	Version int        `json:"version"`
	ID      string     `json:"id"`
	OK      bool       `json:"ok"`
	Result  any        `json:"result,omitempty"`
	Error   *ErrorBody `json:"error,omitempty"`
}

type ErrorBody struct {
	Code    string `json:"code"`
	Message string `json:"message"`
}

type PingResult struct {
	Status string `json:"status"`
}

// MethodHandler resolves a validated request into a response.
type MethodHandler func(Request) Response

func ReadRequest(reader io.Reader) (Request, error) {
	var request Request
	if err := json.NewDecoder(reader).Decode(&request); err != nil {
		return Request{}, err
	}
	return request, nil
}

func WriteRequest(writer io.Writer, request Request) error {
	return json.NewEncoder(writer).Encode(request)
}

func ReadResponse(reader io.Reader) (Response, error) {
	var response Response
	if err := json.NewDecoder(reader).Decode(&response); err != nil {
		return Response{}, err
	}
	return response, nil
}

func WriteResponse(writer io.Writer, response Response) error {
	return json.NewEncoder(writer).Encode(response)
}

// --- Protocol boundary handler ---

// Validate boundary fields and dispatch to built-in methods.
func Handle(request Request) Response {
	return HandleWith(request, nil)
}

// Validate boundary fields and dispatch through the supplied method handler.
func HandleWith(request Request, handler MethodHandler) Response {
	// Common error for invalid version and missing required fields
	if request.Version != Version {
		return ErrorResponse(request.ID, "INVALID_VERSION", fmt.Sprintf("unsupported protocol version %d", request.Version))
	}
	if request.ID == "" {
		return ErrorResponse(request.ID, "INVALID_REQUEST", "id is required")
	}
	if request.Method == "" {
		return ErrorResponse(request.ID, "INVALID_REQUEST", "method is required")
	}

	if handler != nil {
		return handler(request)
	}
	return handleBuiltIn(request)
}

// handleBuiltIn dispatches the minimal protocol-owned method set.
func handleBuiltIn(request Request) Response {
	switch request.Method {
	case "ping":
		return SuccessResponse(request.ID, PingResult{Status: "ok"})
	default:
		return ErrorResponse(request.ID, "UNKNOWN_METHOD", fmt.Sprintf("unknown method %q", request.Method))
	}
}

// SuccessResponse normalizes successful method responses.
func SuccessResponse(id string, result any) Response {
	return Response{
		Version: Version,
		ID:      id,
		OK:      true,
		Result:  result,
	}
}

// ErrorResponse normalizes all failures into the same response shape.
func ErrorResponse(id string, code string, message string) Response {
	return Response{
		Version: Version,
		ID:      id,
		OK:      false,
		Error: &ErrorBody{
			Code:    code,
			Message: message,
		},
	}
}
