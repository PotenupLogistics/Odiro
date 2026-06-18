package process

// Error carries a stable Bridge IPC error code.
type Error struct {
	CodeValue string
	Message   string
}

// Error returns the human-readable failure message.
func (err *Error) Error() string {
	return err.Message
}

// Code returns the stable Bridge IPC error code.
func (err *Error) Code() string {
	return err.CodeValue
}

// NewError creates a process contract error.
func NewError(code string, message string) *Error {
	return &Error{CodeValue: code, Message: message}
}
