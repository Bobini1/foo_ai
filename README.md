# foo_ai

AI MCP control for foobar2000.
Control your music player with an AI assistant!

## Overview

This component starts a local [Model Context Protocol](https://modelcontextprotocol.io)
server. You can connect to it with any [MCP client](https://modelcontextprotocol.io/clients)
that supports SSE.

The AI assistant can send requests to get information from the running music player
or invoke actions like play, create playlists, change volume, etc.

## Installation

1. Download the latest release from the [Releases](https://github.com/Bobini1/foo_ai/releases) page.
2. Double-click the downloaded `.fb2k-component` file to install the component.
3. Restart foobar2000 if it's running.
4. By default, the server will be listening on `http://localhost:9910/sse`.
You can change the host and port in settings (Preferences > Tools > AI).
5. Add the URL from the previous step to your MCP client and voilà!

## Available features

### Tools

The server provides the following [tools](https://modelcontextprotocol.io/specification/2025-11-25/server/tools):
- `list_library`: List all tracks in the library. Can specify a query string.
- `list_playlists`: List all playlists.
- `list_playlist`: List tracks in a specific playlist. Can specify a query string.
- `list_current_track`: Get information about the currently playing track.
- `add_tracks`: Add tracks to a playlist.
- `remove_tracks`: Remove tracks from a playlist.
- `remove_all_tracks`: Remove all tracks from a playlist.
- `move_tracks`: Move tracks within a playlist.
- `set_active_playlist`: Set the active playlist (the one shown in the UI).
- `set_playing_playlist`: Set the playing playlist (the one the fb2k picks tracks from).
- `set_playback_state`: Play or pause music.
- `play_at_index`: Play a track at a specific index in the active playlist.
- `set_focus`: Set the focus to a specific track in the active playlist (without playing it).
- `create_playlist`: Create a new playlist.
- `rename_playlist`: Rename a playlist.
- `delete_playlist`: Delete a playlist.
- `set_volume`: Set volume.
- `toggle_mute`: Toggle mute state.
- `get_volume`: Get current volume and mute state.

### Resources

The server provides a few [resources](https://modelcontextprotocol.io/specification/2025-11-25/server/resources).
They are provided to support subscriptions. When a client subscribes to a resource,
it will receive updates whenever it is updated in the player.

This isn't a crucial feature as clients can always call the tools to get the latest info,
but it can be useful for real-time updates, if your MCP client supports it.

The following resources are available:
- `current_track://.`: Contains info about the current track: when it was last changed and it's playing/paused state.
- `playlists://.`: It contains the same info as the `list_playlists` tool - playlist names, track counts,
last modification times, active/playing states, etc.
- `volume://.`: It contains the same info as the `get_volume` tool.
