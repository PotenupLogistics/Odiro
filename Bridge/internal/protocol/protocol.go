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

// Validate boundary fields and dispatch to supported methods
func Handle(request Request) Response {
	// Common error for invalid version and missing required fields
	if request.Version != Version {
		return errorResponse(request.ID, "INVALID_VERSION", fmt.Sprintf("unsupported protocol version %d", request.Version))
	}
	if request.ID == "" {
		return errorResponse(request.ID, "INVALID_REQUEST", "id is required")
	}
	if request.Method == "" {
		return errorResponse(request.ID, "INVALID_REQUEST", "method is required")
	}

	switch request.Method {
	case "ping":
		return Response{
			Version: Version,
			ID:      request.ID,
			OK:      true,
			Result:  PingResult{Status: "ok"},
		}
	default:
		return errorResponse(request.ID, "UNKNOWN_METHOD", fmt.Sprintf("unknown method %q", request.Method))
	}
}

// normalizes all failures into the same response shape
func errorResponse(id string, code string, message string) Response {
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
