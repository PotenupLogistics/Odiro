package main

import (
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"io"
	"os"

	"odiro/bridge/internal/api"
	"odiro/bridge/internal/ipc"
	"odiro/bridge/internal/protocol"
)

func main() {
	os.Exit(run(os.Args[1:], os.Stdout, os.Stderr))
}

// CLI entrypoint
func run(args []string, stdout io.Writer, stderr io.Writer) int {
	flags := flag.NewFlagSet("OdiroHost", flag.ContinueOnError)
	flags.SetOutput(stderr)
	endpointName := flags.String("endpoint", "", "IPC endpoint name or OS-specific address")
	simulatorExecutable := flags.String("simulator-executable", "", "simulator executable path")
	once := flags.Bool("once", false, "accept one connection and then exit")
	ping := flags.Bool("ping", false, "check a running Bridge endpoint and exit")
	help := flags.Bool("help", false, "show usage")
	if err := flags.Parse(args); err != nil {
		if errors.Is(err, flag.ErrHelp) {
			writeUsage(stdout)
			return 0
		}
		return 2
	}
	if flags.NArg() > 0 {
		fmt.Fprintf(stderr, "unexpected argument: %s\n", flags.Arg(0))
		writeUsage(stderr)
		return 2
	}
	if *help {
		writeUsage(stdout)
		return 0
	}
	if *ping {
		return runPing(*endpointName, stdout, stderr)
	}

	return runServe(*endpointName, *simulatorExecutable, *once, stdout, stderr)
}

// Default Host mode. Listen for incoming connections
func runServe(endpointName string, simulatorExecutable string, once bool, stdout io.Writer, stderr io.Writer) int {
	router, err := api.NewDefaultRouter(simulatorExecutable)
	if err != nil {
		fmt.Fprintf(stderr, "router setup failed: %v\n", err)
		return 1
	}

	listener, err := ipc.Listen(endpointName)
	if err != nil {
		fmt.Fprintf(stderr, "listen failed: %v\n", err)
		return 1
	}
	defer listener.Close()

	endpoint := listener.Endpoint()
	fmt.Fprintf(stdout, "listening transport=%s address=%s\n", endpoint.Transport, endpoint.Address)

	for {
		conn, err := listener.Accept()
		if err != nil {
			if errors.Is(err, ipc.ErrEndpointClosed) {
				return 0
			}
			fmt.Fprintf(stderr, "accept failed: %v\n", err)
			return 1
		}

		if once {
			if err := serveConn(conn, router.Handle); err != nil && !errors.Is(err, io.EOF) {
				fmt.Fprintf(stderr, "connection failed: %v\n", err)
				return 1
			}
			return 0
		}

		go func() {
			if err := serveConn(conn, router.Handle); err != nil && !errors.Is(err, io.EOF) {
				fmt.Fprintf(stderr, "connection failed: %v\n", err)
			}
		}()
	}
}

// Ping mode. Endpoint liveness check
func runPing(endpointName string, stdout io.Writer, stderr io.Writer) int {
	conn, err := ipc.Dial(endpointName)
	if err != nil {
		fmt.Fprintf(stderr, "dial failed: %v\n", err)
		return 1
	}
	defer conn.Close()

	request := protocol.Request{
		Version: protocol.Version,
		ID:      "ping",
		Method:  "ping",
	}
	if err := protocol.WriteRequest(conn, request); err != nil {
		fmt.Fprintf(stderr, "write failed: %v\n", err)
		return 1
	}

	response, err := protocol.ReadResponse(conn)
	if err != nil {
		fmt.Fprintf(stderr, "read failed: %v\n", err)
		return 1
	}

	encoder := json.NewEncoder(stdout)
	encoder.SetIndent("", "  ")
	if err := encoder.Encode(response); err != nil {
		fmt.Fprintf(stderr, "encode failed: %v\n", err)
		return 1
	}
	if !response.OK {
		return 1
	}
	return 0
}

// IPC connection handler
func serveConn(conn ipc.Conn, handler protocol.MethodHandler) error {
	defer conn.Close()

	decoder := json.NewDecoder(conn)
	encoder := json.NewEncoder(conn)
	for {
		var request protocol.Request
		if err := decoder.Decode(&request); err != nil {
			return err
		}
		if err := encoder.Encode(protocol.HandleWith(request, handler)); err != nil {
			return err
		}
	}
}

// writeUsage는 CLI 표면을 한 곳에서 유지
func writeUsage(writer io.Writer) {
	fmt.Fprintln(writer, "Usage:")
	fmt.Fprintln(writer, "  OdiroHost [--endpoint name] [--simulator-executable path] [--once]")
	fmt.Fprintln(writer, "  OdiroHost --ping [--endpoint name]")
	fmt.Fprintln(writer, "  OdiroHost --help")
}
