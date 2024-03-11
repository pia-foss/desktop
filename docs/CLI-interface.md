# CLI interface

The CLI interface serves multiple purposes:

* Automated testing harness
  * Internally we will write automated tests using the CLI interface to control the daemon
* Development
  * Obtain diagnostic information for development, like monitoring state changes
  * Test failure mode code, like attempting RPCs in situations that clients normally would not (for example, attempt to disconnect when already disconnected - possible graphically if the client races with a disconnect, but hard to do)
* Users periodically ask for scriptable interface for basic operations
  * get port forward status
  * connect/disconnect
  * change region

# Interface design

## Stable and unstable features

All parts of the CLI interface are divided into _stable_ and _unstable_ features.

_Stable_ features are supported indefinitely for use by end users and will remain compatible in future versions.  A few very targeted features will go a long way toward meeting users' needs.  Because these must remain compatible, there will only be a few stable features in the initial release.

_Unstable_ features are not guaranteed to remain the same in newer versions.  We will need many more features internally for testing and development, but they don't need to remain compatible in future versions.  Unstable features will be hidden by default.

## Commands

The CLI interface provides _commands_ that define the basic operations that can be performed.  Each invocation of the CLI specifies exactly one command.

Stable commands tend to be very specific (and are often a subset of an unstable command), while unstable commands tend to be very general.

Most commands are one-shot commands - the CLI performs the operation and exits.  A few commands are persistent commands (for example, `watch`), the CLI remains connected and performs an operation until it is killed with a signal.

### Stable commands

| Command | Purpose |
|---------|---------|
| `get <keyword>` | General "get" command, get some specific piece of information |
| `set <keyword> <value>` | General "set" command, set some specific piece of information |
| `connect` | Connect to the VPN |
| `disconnect` | Disconnect from the VPN |

| `get`/`set` keyword | `get` | `set` | Value | Purpose |
|---------------------|-------|-------|-------|---------|
| `debuglogging` | X | X | boolean (`true`/`false`) | Check or toggle the debug logging setting. |
| `portforward` | X |  | Integer, port number or status code | Get the current forwarded port or a status code (if the request is ongoing, failed, etc.) |
| `vpnip` | X |  | IP address as string | Get the current VPN IP address. |
| `region` | X | X | String, `auto` or CLI ID | Select a specific location.  If connected, reconnects to the new location. |
| `regions` | X |  | List of location CLI IDs | Lists all locations (including `auto`). |

>>>
**Location CLI IDs:** Locations are identified using IDs generated from their names, such as `us-east`, `de-berlin`, etc.

Location IDs are not used due to the historical nature of many names and inconsistency, such as `germany` (now DE Berlin), `us3`, `swiss`, etc.
>>>

>>>
:point\_right: Targeted "monitor" command for reacting to daemon state, etc.?
>>>

### Unstable commands

| Command | Purpose |
|---------|---------|
| `applysettings` | Apply arbitrary setting changes, accepts a JSON payload. |
| `watch` | Watch for changes to daemon settings/state/data and print updates.  (Prints initial non-default values after connecting.) |

>>>
:point\_right: Print complete state?

:point\_right: Get/set arbitrary JSON value?

:point\_right: Other daemon RPCs?  Arbitrary RPC call?
>>>

## Options

Options control the behavior of commands.  Most options apply to more than one command (often all of them).

### Stable options

| Option | Value | Purpose |
|--------|-------|---------|
| `--timeout` / `-t` | Integer, timeout in seconds | Sets connection timeout for one-shot commands. The CLI attempts to connect to the daemon for some amount of time and gives up if no connection is made.  The default timeout is 5 seconds, but `--timeout` can override it. |
| `--debug` / `-d` | \<flag\> | Prints CLI logging to stderr.  (Normally, logging is only written to log files, and only if debug logging is enabled.) |

The special `--help` and `--version` options are also supported.

### Unstable options

| Option | Value | Purpose |
|--------|-------|---------|
| `--unstable` / `-u` | \<flag\> | Enables unstable features (otherwise CLI will act as if unstable features do not exist) |

>>>
:point\_right: JSON result flag?  Might print raw JSON instead of an interpreted result, for values/errors
>>>