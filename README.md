# FiveM Trigger Finder

FiveM Trigger Finder is a lightweight C++17 utility for scanning FiveM resource dumps and detecting common event, trigger, export, command and callback patterns.

The scanner supports multiple resource file types and uses multithreaded scanning to process large resource directories faster.

## Features

* Multithreaded file scanning
* Recursive directory scanning
* Lua event detection
* JavaScript event detection
* FiveM trigger detection
* `TriggerEvent`
* `TriggerServerEvent`
* `TriggerClientEvent`
* `TriggerLatentClientEvent`
* `RegisterNetEvent`
* `AddEventHandler`
* `RemoveEventHandler`
* `onNet`
* `emitNet`
* `RegisterCommand`
* `RegisterExport`
* `RegisterUICallback`
* `AddStateBagChangeHandler`
* `GlobalState`
* Player state setters
* HTTP handler detection
* Export detection
* String-based event detection
* Automatic comment filtering
* TXT reports
* JSON reports
* Configurable scan path
* Automatic timestamped reports
* Skips common directories such as `.git`, `node_modules` and `cache`

## Supported File Types

The scanner currently checks:

```text
.lua
.js
.mjs
.cjs
.ts
.json
.xml
.cfg
.txt
.sql
.yaml
.yml
.html
.css
.md
.ini
.toml
```

## Requirements

* Windows or Linux
* C++17 compatible compiler
* Standard C++ library
* CMake is not required for the basic build

Recommended:

* Visual Studio 2022
* GCC 9+
* Clang 10+

## Build

### Visual Studio

Create a Console Application project and set the language standard to C++17 or newer.

Add `main.cpp` to the project and build it in Release mode.

### MinGW

```bash
g++ -std=c++17 -O2 -pthread main.cpp -o FiveMTriggerFinder
```

### Linux

```bash
g++ -std=c++17 -O2 -pthread main.cpp -o FiveMTriggerFinder
```

## Usage

By default, the program looks for a directory named:

```text
dump
```

next to the executable.

Example:

```text
FiveMTriggerFinder.exe
dump/
├── resource1/
├── resource2/
└── resource3/
```

Run:

```bash
FiveMTriggerFinder.exe
```

## Custom Scan Path

You can specify another directory with `--path`.

```bash
FiveMTriggerFinder.exe --path "C:\FiveM\resources"
```

Linux:

```bash
./FiveMTriggerFinder --path "/home/user/fivem/resources"
```

## JSON Output

Use the `--json` parameter to generate a JSON report.

```bash
FiveMTriggerFinder.exe --json
```

Example output:

```json
{
  "path": "C:\\FiveM\\dump",
  "files": 120,
  "triggers": [
    {
      "name": "example:event",
      "file": "resource/client.lua",
      "line": 42,
      "type": "Trigger/Register net event"
    }
  ]
}
```

## Output

Without `--json`, the program creates a TXT report:

```text
report_2026-08-20_10-30-15.txt
```

The report contains:

* Scan path
* Number of scanned files
* Number of detected triggers
* Trigger name
* Trigger type
* File path
* Line number
* Original source line

## Example

```text
Starting scan in: C:\FiveM\dump

Total files to scan: 245

Progress: 100% (245/245 files)

Found 37 triggers. Results saved to report_2026-08-20_10-30-15.txt
```

## Excluded Directories

The following directories are skipped automatically:

```text
node_modules
.git
cache
```

This helps reduce unnecessary scanning and improves performance on large resource directories.

## Detection

The scanner uses regular expressions to identify common patterns.

Examples include:

```lua
TriggerServerEvent("example:event")
```

```lua
RegisterNetEvent("example:event")
```

```lua
AddEventHandler("example:event", function()
end)
```

```lua
RegisterCommand("example", function()
end)
```

```javascript
emitNet("example:event")
```

## Performance

The scanner automatically determines the number of worker threads based on the available CPU hardware.

The default behavior uses approximately:

```text
CPU threads - 1
```

with a minimum of one worker thread.

This allows large dumps to be processed concurrently.

## Limitations

This project is based on static pattern matching.

It does not execute FiveM resources and does not understand the complete runtime behavior of Lua or JavaScript code.

Dynamic event names constructed at runtime may not be detected.

For example:

```lua
local eventName = prefix .. event
TriggerServerEvent(eventName)
```

may not produce a useful detection result because the final event name is not statically available.

## Project Structure

```text
FiveMTriggerFinder/
├── main.cpp
├── README.md
├── dump/
└── reports/
```

The `reports` directory is optional and can be used to store generated scan results.

## License

This project is provided for educational, development and defensive code-auditing purposes.

You are responsible for using the software only on files and systems that you are authorized to inspect.

## Disclaimer

FiveM Trigger Finder is a static source-code analysis tool.

It does not guarantee that every event, trigger or callback in a resource will be detected.

Results should be manually reviewed before making any conclusions about a resource.

## Contributing

Pull requests and improvements are welcome.

When submitting changes:

1. Keep the code compatible with C++17.
2. Avoid unnecessary external dependencies.
3. Test the scanner against multiple FiveM resource structures.
4. Keep generated reports out of the repository.
5. Update the README when adding new functionality.

## Author

FiveM Trigger Finder

Built with C++17.
