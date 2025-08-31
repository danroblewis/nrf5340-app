package main

import "syscall/js"

// getNumber returns the number 99
func getNumber() int {
	return 99
}

// add adds two numbers
func add(a, b int) int {
	return a + b
}

// multiply multiplies two numbers
func multiply(a, b int) int {
	return a * b
}

// registerFunctions registers the Go functions with the WASM environment
func registerFunctions() {
	js.Global().Set("getNumber", js.FuncOf(func(this js.Value, args []js.Value) interface{} {
		return getNumber()
	}))
	
	js.Global().Set("add", js.FuncOf(func(this js.Value, args []js.Value) interface{} {
		if len(args) >= 2 {
			return add(args[0].Int(), args[1].Int())
		}
		return 0
	}))
	
	js.Global().Set("multiply", js.FuncOf(func(this js.Value, args []js.Value) interface{} {
		if len(args) >= 2 {
			return multiply(args[0].Int(), args[1].Int())
		}
		return 0
	}))
}

func main() {
	// Register our functions
	registerFunctions()
	
	// Keep the program running
	select {}
}
