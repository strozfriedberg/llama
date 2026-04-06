# Tauri Frontend Extensibility

Target stack: [Tauri](https://v2.tauri.app/) + Rust backend + TypeScript + [Svelte 5](https://svelte.dev/) frontend.

No Tauri app has shipped a user-facing plugin system yet. Adapting [Obsidian's model](https://docs.obsidian.md/Plugins/Getting+started/Build+a+plugin) (~1,800 community plugins) from Electron to Tauri.

## Plugin Loading

Register a custom URI scheme on the Rust side to serve plugin files from `~/.llama/plugins/`:

```rust
tauri::Builder::default()
    .register_asynchronous_uri_scheme_protocol("plugin", |_ctx, req, responder| {
        // Serve from plugins directory
    })
```

Frontend loads via dynamic `import()`:

```typescript
const module = await import(`plugin://localhost/${pluginId}/main.js`);
module.default.activate(pluginAPI);
```

No `eval()`, no `unsafe-inline` CSP. ES modules only.

## Plugin Package Format

```
my-plugin/
  manifest.json   # id, name, version, apiVersion, permissions, UI slot declarations
  main.js         # Bundled ES module
  main.css        # Optional
```

## UI Extensibility

**v1: DOM containers, not Svelte components.** Give plugins an HTMLElement, let them render however they want. Avoids Svelte 5 runtime-sharing issues ([sveltejs/svelte#13186](https://github.com/sveltejs/svelte/issues/13186)).

```typescript
export default class MyPlugin {
    async activate(api: PluginAPI) {
        const panel = api.ui.createPanel('my-panel', { title: 'Timeline' });
        panel.container.innerHTML = '...'; // vanilla JS, any framework, whatever
    }
}
```

Plugin authors who want Svelte can compile and bundle it. No framework imposed.

## Plugin API

Never expose raw Tauri IPC. Mediation layer:

```typescript
interface PluginAPI {
    apiVersion: number;
    data: {
        query(sql: string): Promise<Row[]>;  // SQL against case DuckDB — the power move
        getRuleHits(ruleId: string): Promise<Hit[]>;
        getFileByHash(hash: string): Promise<FileInfo>;
    };
    ui: {
        createPanel(id: string, opts: PanelOptions): PanelSlot;
        addCommand(cmd: CommandDefinition): Disposable;
        addContextMenuItem(item: MenuItemDef): Disposable;
        showNotification(msg: string): void;
    };
    events: {
        on(event: string, handler: Function): Disposable;
        emit(event: string, data: any): void;
    };
    storage: { get(key: string): Promise<any>; set(key: string, value: any): Promise<void>; };
}
```

`data.query(sql)` is the escape hatch — plugin authors who know SQL can build anything without waiting for bespoke API methods.

## Security

- **Reviewed plugins** (community-listed): main webview context, mediated API
- **Unreviewed**: sandboxed `<iframe sandbox="allow-scripts">`, `postMessage` only, no Tauri IPC access

## Hot Reload for Plugin Devs

File watcher + cache-busting imports:

```typescript
const module = await import(`plugin://localhost/${pluginId}/main.js?v=${Date.now()}`);
```

Ship a plugin template repo (like [obsidian-sample-plugin](https://github.com/obsidianmd/obsidian-sample-plugin)) with Vite config, TypeScript types (`@llama/plugin-api`), and dev server.

## AI Integration

Narrow scope: **natural language to SQL** against case data with known schemas. AI-assisted rule authoring. Not AI-assisted analysis (hallucination + testimony = liability). Offer local inference for air-gapped environments.
