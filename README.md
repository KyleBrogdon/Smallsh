# Lightweight Unix Shell

A custom shell that can handle user inputs, perform variable expansion, navigate directories and file folders, fork processes to handle standard commands (both background and foreground execution), redirect input/output, and handle signals.

## Getting Started

These instructions will get you a copy of the project up and running on your local machine for development and testing purposes. See deployment for notes on how to deploy the project on a live system.

### Prerequisites
- C compiler installed

### Installing
- Clone this repository
- Navigate to project directory via terminal or shell
- Run "gcc -std=c99 -o //desiredfilename// main.c"

## Example usage
- commmand [arg1 arg2 ...] [< input_file] [> output_file] [&]
  - Items in square brackets are optional
  - If a command is to be executed in the background, the last entry must be &

## Built With
- C
- MinGW
- CMake

## Author
Kyle Brogdon

## License

This project is licensed under the MIT License - see the [LICENSE.md](LICENSE.md) file for details
