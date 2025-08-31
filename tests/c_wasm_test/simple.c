#include <emscripten.h>

// Function that returns 99
EMSCRIPTEN_KEEPALIVE
int getNumber() {
    return 99;
}

// Function that adds two numbers
EMSCRIPTEN_KEEPALIVE
int add(int a, int b) {
    return a + b;
}

// Function that multiplies two numbers
EMSCRIPTEN_KEEPALIVE
int multiply(int a, int b) {
    return a * b;
}

// Function that subtracts two numbers
EMSCRIPTEN_KEEPALIVE
int subtract(int a, int b) {
    return a - b;
}

// Function that divides two numbers
EMSCRIPTEN_KEEPALIVE
int divide(int a, int b) {
    if (b != 0) {
        return a / b;
    }
    return 0;
}

// Function that returns the maximum of two numbers
EMSCRIPTEN_KEEPALIVE
int max(int a, int b) {
    return (a > b) ? a : b;
}

// Function that returns the minimum of two numbers
EMSCRIPTEN_KEEPALIVE
int min(int a, int b) {
    return (a < b) ? a : b;
}

// Function that checks if a number is even
EMSCRIPTEN_KEEPALIVE
int isEven(int n) {
    return (n % 2 == 0) ? 1 : 0;
}

// Function that checks if a number is odd
EMSCRIPTEN_KEEPALIVE
int isOdd(int n) {
    return (n % 2 == 1) ? 1 : 0;
}

// Function that returns the absolute value
EMSCRIPTEN_KEEPALIVE
int abs(int n) {
    return (n < 0) ? -n : n;
}
