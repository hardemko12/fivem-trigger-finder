# FiveM Trigger Finder

Fast C++17 tool for scanning FiveM resources and detecting common triggers, events, exports and callbacks.

## Features

* Fast multithreaded scanning
* Lua / JS support
* Trigger & event detection
* Export detection
* Command detection
* Callback detection
* TXT / JSON reports
* Recursive scanning

## Usage

Default:

```bash
FiveMTriggerFinder.exe
```

Custom path:

```bash
FiveMTriggerFinder.exe --path "C:\FiveM\resources"
```

JSON output:

```bash
FiveMTriggerFinder.exe --json
```

## Build

```bash
g++ -std=c++17 -O2 -pthread main.cpp -o FiveMTriggerFinder
```

Requires a C++17 compatible compiler.

## License

For educational and authorized code-auditing purposes only.
