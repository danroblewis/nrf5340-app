package main

//export getNumber
func getNumber() int32 {
	return 99
}

//export add
func add(a, b int32) int32 {
	return a + b
}

//export multiply
func multiply(a, b int32) int32 {
	return a * b
}

func main() {
	// This function is required but not used
}
