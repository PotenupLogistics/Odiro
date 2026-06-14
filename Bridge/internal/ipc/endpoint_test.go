package ipc

import (
	"fmt"
	"io"
	"os"
	"testing"
	"time"
)

// Validate stream request/response round-trip
func TestListenDialRoundTrip(t *testing.T) {
	name := fmt.Sprintf("odiro-bridge-test-%d-%d", os.Getpid(), time.Now().UnixNano())

	listener, err := Listen(name)
	if err != nil {
		t.Fatalf("Listen() error = %v", err)
	}
	defer listener.Close()

	serverErr := make(chan error, 1)
	go func() {
		conn, err := listener.Accept()
		if err != nil {
			serverErr <- err
			return
		}
		defer conn.Close()

		buffer := make([]byte, 4)
		if _, err := io.ReadFull(conn, buffer); err != nil {
			serverErr <- err
			return
		}
		if string(buffer) != "ping" {
			serverErr <- fmt.Errorf("read %q, want ping", string(buffer))
			return
		}
		_, err = conn.Write([]byte("pong"))
		serverErr <- err
	}()

	conn, err := Dial(name)
	if err != nil {
		t.Fatalf("Dial() error = %v", err)
	}
	defer conn.Close()

	if _, err := conn.Write([]byte("ping")); err != nil {
		t.Fatalf("Write() error = %v", err)
	}

	buffer := make([]byte, 4)
	if _, err := io.ReadFull(conn, buffer); err != nil {
		t.Fatalf("ReadFull() error = %v", err)
	}
	if string(buffer) != "pong" {
		t.Fatalf("read %q, want pong", string(buffer))
	}

	if err := <-serverErr; err != nil {
		t.Fatalf("server error = %v", err)
	}
}
