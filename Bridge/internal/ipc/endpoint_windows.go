//go:build windows

package ipc

import (
	"fmt"
	"os"
	"strings"
	"sync"
	"sync/atomic"
	"syscall"
	"time"
	"unsafe"
)

const (
	defaultPipeName = "odiro-bridge"
	pipePrefix      = `\\.\pipe\`

	// client handle read permission
	genericRead = 0x80000000
	// client handle write permission
	genericWrite = 0x40000000
	// the CreateFile mode to open an existing pipe instance
	openExisting = 3

	// bidirectional byte stream mode
	pipeAccessDuplex = 0x00000003
	// byte stream mode without message boundaries
	pipeTypeByte = 0x00000000
	// byte read mode
	pipeReadModeByte = 0x00000000
	// blocking pipe operation mode
	pipeWait = 0x00000000
	// let the OS decide the number of instances for the same pipe name
	pipeUnlimitedInstance = 255

	// indicates the pipe server is not ready yet
	errorFileNotFound syscall.Errno = 2
	// indicates all pipe instances are busy
	errorPipeBusy syscall.Errno = 231
	// indicates the client is already connected
	errorPipeConnected syscall.Errno = 535
)

var (
	// entrypoint for named pipe Win32 API
	kernel32 = syscall.NewLazyDLL("kernel32.dll")
	// API to create a server pipe instance
	procCreateNamedPipeW = kernel32.NewProc("CreateNamedPipeW")
	// API to connect a pipe instance to a client
	procConnectNamedPipe = kernel32.NewProc("ConnectNamedPipe")
	// API to wait for a busy pipe to become available
	procWaitNamedPipeW = kernel32.NewProc("WaitNamedPipeW")
)

// Create a socket endpoint from a friendly name
func resolve(name string) (Endpoint, error) {
	address := strings.TrimSpace(name)
	if address == "" {
		address = defaultPipeName
	}

	if !strings.HasPrefix(strings.ToLower(address), pipePrefix) {
		if strings.ContainsAny(address, `\/:`) {
			return Endpoint{}, fmt.Errorf("invalid named pipe endpoint %q", name)
		}
		address = pipePrefix + address
	}

	return Endpoint{
		Transport: TransportNamedPipe,
		Address:   address,
	}, nil
}

// Open a lazy listener that creates a new named pipe instance
func listen(endpoint Endpoint) (Listener, error) {
	if endpoint.Transport != TransportNamedPipe {
		return nil, fmt.Errorf("unsupported Windows IPC transport %q", endpoint.Transport)
	}
	return &windowsListener{endpoint: endpoint}, nil
}

// creates a stream connection to the given endpoint
func dial(endpoint Endpoint) (Conn, error) {
	if endpoint.Transport != TransportNamedPipe {
		return nil, fmt.Errorf("unsupported Windows IPC transport %q", endpoint.Transport)
	}

	name, err := syscall.UTF16PtrFromString(endpoint.Address)
	if err != nil {
		return nil, err
	}

	deadline := time.Now().Add(5 * time.Second)
	for {
		handle, err := syscall.CreateFile(
			name,
			genericRead|genericWrite,
			0,
			nil,
			openExisting,
			0,
			0,
		)
		if err == nil {
			return os.NewFile(uintptr(handle), endpoint.Address), nil
		}

		errno := windowsErrno(err)
		if errno != errorPipeBusy && errno != errorFileNotFound {
			return nil, err
		}
		if time.Now().After(deadline) {
			return nil, err
		}

		if errno == errorPipeBusy {
			_ = waitNamedPipe(endpoint.Address, 250)
			continue
		}
		time.Sleep(25 * time.Millisecond)
	}
}

// Windows named pipe instance adapter
type windowsListener struct {
	endpoint Endpoint
	// flag for blocking Accept after Close
	closed atomic.Bool
	// mutex for protecting the current handle
	mu sync.Mutex
	// handle currently blocked in ConnectNamedPipe
	current syscall.Handle
}

func (listener *windowsListener) Endpoint() Endpoint {
	return listener.endpoint
}

func (listener *windowsListener) Accept() (Conn, error) {
	if listener.closed.Load() {
		return nil, ErrEndpointClosed
	}

	// Windows named pipe server requires a new pipe instance for each pending/accepted connection
	handle, err := createNamedPipe(listener.endpoint.Address)
	if err != nil {
		return nil, err
	}

	listener.mu.Lock()
	if listener.closed.Load() {
		listener.mu.Unlock()
		_ = syscall.CloseHandle(handle)
		return nil, ErrEndpointClosed
	}
	listener.current = handle
	listener.mu.Unlock()

	err = connectNamedPipe(handle)

	listener.mu.Lock()
	if listener.current == handle {
		listener.current = 0
	}
	listener.mu.Unlock()

	if err != nil {
		_ = syscall.CloseHandle(handle)
		if listener.closed.Load() {
			return nil, ErrEndpointClosed
		}
		return nil, err
	}

	return os.NewFile(uintptr(handle), listener.endpoint.Address), nil
}

func (listener *windowsListener) Close() error {
	listener.closed.Store(true)

	listener.mu.Lock()
	handle := listener.current
	listener.current = 0
	listener.mu.Unlock()

	if handle != 0 {
		return syscall.CloseHandle(handle)
	}
	return nil
}

// creates a blocking byte-stream pipe instance
func createNamedPipe(address string) (syscall.Handle, error) {
	name, err := syscall.UTF16PtrFromString(address)
	if err != nil {
		return 0, err
	}

	handle, _, callErr := procCreateNamedPipeW.Call(
		uintptr(unsafe.Pointer(name)),
		uintptr(pipeAccessDuplex),
		uintptr(pipeTypeByte|pipeReadModeByte|pipeWait),
		uintptr(pipeUnlimitedInstance),
		uintptr(64*1024),
		uintptr(64*1024),
		uintptr(0),
		uintptr(0),
	)
	if syscall.Handle(handle) == syscall.InvalidHandle {
		return 0, nonzeroErr(callErr)
	}
	return syscall.Handle(handle), nil
}

// acknowledges the first client that connects as a valid connection
func connectNamedPipe(handle syscall.Handle) error {
	connected, _, callErr := procConnectNamedPipe.Call(uintptr(handle), uintptr(0))
	if connected != 0 {
		return nil
	}

	errno := windowsErrno(callErr)
	if errno == errorPipeConnected {
		return nil
	}
	return nonzeroErr(callErr)
}

// provides a short retry interval for a busy pipe name
func waitNamedPipe(address string, timeoutMillis uint32) error {
	name, err := syscall.UTF16PtrFromString(address)
	if err != nil {
		return err
	}

	ok, _, callErr := procWaitNamedPipeW.Call(
		uintptr(unsafe.Pointer(name)),
		uintptr(timeoutMillis),
	)
	if ok != 0 {
		return nil
	}
	return nonzeroErr(callErr)
}

// adjusts a zero errno from a Win32 call to a meaningful error
func nonzeroErr(err error) error {
	if windowsErrno(err) == 0 {
		return syscall.EINVAL
	}
	return err
}

// extracts the Win32 errno from a syscall error
func windowsErrno(err error) syscall.Errno {
	if errno, ok := err.(syscall.Errno); ok {
		return errno
	}
	return 0
}
