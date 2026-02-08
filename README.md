# 🐚 3230yash - A Custom Unix Shell

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C](https://img.shields.io/badge/C-99-blue.svg)](https://en.wikipedia.org/wiki/C_(programming_language))

A feature-rich Unix shell implementation in C from scratch, supporting command execution, piping, process monitoring, and signal handling.

## ✨ Features

- ✅ **Command Execution**: Execute commands with PATH resolution
- ✅ **Pipe Support**: Chain up to 5 commands with `|`
- ✅ **Process Monitoring**: Real-time resource tracking with `watch` command
- ✅ **Signal Handling**: Proper SIGINT (Ctrl+C) management
- ✅ **Built-in Commands**: `exit`, `watch`
- ✅ **Zombie Prevention**: Automatic cleanup of terminated processes
- ✅ **Error Handling**: Comprehensive error reporting and recovery

## 🛠️ Technical Implementation

### Core Components:
- **Process Management**: `fork()`, `execvp()`, `waitpid()`
- **Inter-process Communication**: Pipes for command chaining
- **System Monitoring**: `/proc` filesystem access for process stats
- **Signal Management**: Custom SIGINT handler with proper propagation
- **Memory Safety**: Proper allocation/deallocation with error checking

## 📦 Installation & Usage

```bash
# Clone the repository
git clone https://github.com/EricIeong/yash_shell.git
cd yash-shell #(or the folder you have cloned the repository to)

# Compile
make

# Run
./yash

# Example session
## 3230yash >> ls -l | grep ".c" | wc -l
## 3230yash >> watch sleep 2
## 3230yash >> ps aux | head -5
