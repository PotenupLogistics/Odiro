//go:build unix

package ipc

import (
	"fmt"
	"net"
	"os"
	"path/filepath"
	"strings"
)

const defaultSocketName = "odiro-bridge"

// Create a socket endpoint from a friendly name
func resolve(name string) (Endpoint, error) {
	socketPath := strings.TrimSpace(name)
	if socketPath == "" {
		socketPath = defaultSocketName
	}

	if !filepath.IsAbs(socketPath) {
		socketPath = filepath.Join(runtimeDir(), socketPath+".sock")
	}
	if len(socketPath) > 100 {
		return Endpoint{}, fmt.Errorf("Unix socket path is too long: %s", socketPath)
	}

	return Endpoint{
		Transport: TransportUnix,
		Address:   socketPath,
	}, nil
}

// Open socket listener at the given endpoint
func listen(endpoint Endpoint) (Listener, error) {
	if endpoint.Transport != TransportUnix {
		return nil, fmt.Errorf("unsupported Unix IPC transport %q", endpoint.Transport)
	}

	if err := os.MkdirAll(filepath.Dir(endpoint.Address), 0o700); err != nil {
		return nil, err
	}
	if err := removeStaleSocket(endpoint.Address); err != nil {
		return nil, err
	}

	listener, err := net.Listen("unix", endpoint.Address)
	if err != nil {
		return nil, err
	}
	return &unixListener{
		Listener: listener,
		endpoint: endpoint,
	}, nil
}

// Create a stream connection to the given endpoint
func dial(endpoint Endpoint) (Conn, error) {
	if endpoint.Transport != TransportUnix {
		return nil, fmt.Errorf("unsupported Unix IPC transport %q", endpoint.Transport)
	}
	return net.Dial("unix", endpoint.Address)
}

// net.Listener adapter
type unixListener struct {
	net.Listener
	endpoint Endpoint
}

func (listener *unixListener) Endpoint() Endpoint {
	return listener.endpoint
}

func (listener *unixListener) Accept() (Conn, error) {
	return listener.Listener.Accept()
}

func (listener *unixListener) Close() error {
	closeErr := listener.Listener.Close()
	removeErr := os.Remove(listener.endpoint.Address)
	if closeErr != nil {
		return closeErr
	}
	if removeErr != nil && !os.IsNotExist(removeErr) {
		return removeErr
	}
	return nil
}

// runtime directory for storing socket files
func runtimeDir() string {
	if value := os.Getenv("XDG_RUNTIME_DIR"); value != "" {
		return filepath.Join(value, "odiro")
	}
	// macOS는 보통 XDG_RUNTIME_DIR 없음
	// user temp directory로 Unix-like OS의 socket path 충돌 방지
	return filepath.Join(os.TempDir(), fmt.Sprintf("odiro-%d", os.Getuid()))
}

// remove stale socket to avoid "address already in use" error on Listen
func removeStaleSocket(path string) error {
	info, err := os.Lstat(path)
	if os.IsNotExist(err) {
		return nil
	}
	if err != nil {
		return err
	}
	if info.Mode()&os.ModeSocket == 0 {
		return fmt.Errorf("refusing to remove non-socket file at %s", path)
	}
	return os.Remove(path)
}
