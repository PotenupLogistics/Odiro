package protocol

import (
	"bytes"
	"encoding/json"
	"testing"
)

// Validate contract of ping success response
func TestHandlePing(t *testing.T) {
	response := Handle(Request{
		Version: Version,
		ID:      "request-1",
		Method:  "ping",
	})

	if !response.OK {
		t.Fatalf("response.OK = false, error = %#v", response.Error)
	}
	if response.Version != Version {
		t.Fatalf("response.Version = %d, want %d", response.Version, Version)
	}
	if response.ID != "request-1" {
		t.Fatalf("response.ID = %q, want request-1", response.ID)
	}

	result, ok := response.Result.(PingResult)
	if !ok {
		t.Fatalf("response.Result type = %T, want PingResult", response.Result)
	}
	if result.Status != "ok" {
		t.Fatalf("result.Status = %q, want ok", result.Status)
	}
}

// Validate error code for unknown method
func TestHandleRejectsUnknownMethod(t *testing.T) {
	response := Handle(Request{
		Version: Version,
		ID:      "request-1",
		Method:  "missing",
	})

	if response.OK {
		t.Fatal("response.OK = true, want false")
	}
	if response.Error == nil || response.Error.Code != "UNKNOWN_METHOD" {
		t.Fatalf("response.Error = %#v, want UNKNOWN_METHOD", response.Error)
	}
}

// Validate JSON-line format of request frame
func TestRoundTripJSONLine(t *testing.T) {
	var buffer bytes.Buffer

	err := WriteRequest(&buffer, Request{
		Version: Version,
		ID:      "request-1",
		Method:  "ping",
	})
	if err != nil {
		t.Fatalf("WriteRequest() error = %v", err)
	}

	request, err := ReadRequest(&buffer)
	if err != nil {
		t.Fatalf("ReadRequest() error = %v", err)
	}

	if request.Method != "ping" {
		t.Fatalf("request.Method = %q, want ping", request.Method)
	}
}

// Validate JSON shape of successful response
func TestResponseJSONShape(t *testing.T) {
	response := Handle(Request{
		Version: Version,
		ID:      "request-1",
		Method:  "ping",
	})

	payload, err := json.Marshal(response)
	if err != nil {
		t.Fatalf("Marshal() error = %v", err)
	}

	want := `{"version":1,"id":"request-1","ok":true,"result":{"status":"ok"}}`
	if string(payload) != want {
		t.Fatalf("payload = %s, want %s", payload, want)
	}
}
