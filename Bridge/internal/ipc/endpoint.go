package ipc

import (
	"errors"
	"io"
)

// endpoint types
const (
	TransportNamedPipe = "windows_named_pipe"
	TransportUnix      = "unix_socket"
)

var ErrEndpointClosed = errors.New("ipc endpoint closed")

// Minimal stream contract required by IPC transport
type Conn interface {
	io.Reader
	io.Writer
	io.Closer
}

// Listener interface to accept IPC stream
//
// Select appropriate implementation at compile time with build tags
type Listener interface {
	Accept() (Conn, error)
	Close() error
	Endpoint() Endpoint
}

// OS-specific address used by listener
type Endpoint struct {
	// IPC backend type
	Transport string `json:"transport"`
	// actual endpoint address passed to the OS API
	Address string `json:"address"`
}

// Convert a human-readable endpoint name to the current OS transport address
func Resolve(name string) (Endpoint, error) {
	return resolve(name)
}

// Create a IPC listener
func Listen(name string) (Listener, error) {
	endpoint, err := Resolve(name)
	if err != nil {
		return nil, err
	}
	return listen(endpoint)
}

// Connect to a listener
func Dial(name string) (Conn, error) {
	endpoint, err := Resolve(name)
	if err != nil {
		return nil, err
	}
	return dial(endpoint)
}
