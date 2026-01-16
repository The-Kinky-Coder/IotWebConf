# Migration Guide to v3.2.0

Version 3.2.0 introduces a major architectural change to separate the "View" (HTML rendering) from the "Model" (Parameters). This allows for complete customization of the HTML output (e.g., using Bootstrap, Tailwind) without modifying the core logic.

## Breaking Changes

### 1. `renderHtml` is bypassed
The `renderHtml` method in `ConfigItem` is no longer used by the core library to generate HTML.
*   **Old Behavior:** `parameter->renderHtml()` was called directly to generate HTML.
*   **New Behavior:** `IotWebConf` delegates rendering to an `HtmlRenderer` (defaulting to `StandardHtmlRenderer`). This renderer visits the parameter tree and generates HTML internally.
*   **Impact:** If you had overridden `renderHtml` in a custom `ConfigItem` subclass to customize its appearance, your code **will still compile** (as the method remains virtual), but it **will not be executed**.
    *   **Fix:** Create a custom `HtmlRenderer` or subclass `StandardHtmlRenderer` and override the `render` method for your specific item type.

### 2. `setHtmlRenderer` API
A new method `setHtmlRenderer(HtmlRenderer* renderer)` has been added to `IotWebConf`.
*   **Important:** `IotWebConf` does **not** take ownership of the renderer pointer. You must ensure the renderer object stays alive (e.g., declare it in global scope).

### 3. Public Accessors in `ConfigItem`
New public methods were added to `ConfigItem` and `ParameterGroup` to allow external renderers to traverse the tree:
*   `getType()`
*   `getFirst()`
*   `getNext()`
*   `getNextItemOf()`

### 4. `IotWebConfItemType` Enum
A new enum `IotWebConfItemType` has been introduced to replace RTTI (`dynamic_cast`). If you create custom parameter types, you should override `getType()` to return `IOTWEBCONF_TYPE_UNDEFINED` (or add a new type to the enum if you are modifying the library core).

## How to Upgrade

1.  **Standard Users:** No changes required. The `StandardHtmlRenderer` is automatically used and mimics the old output exactly.
2.  **Custom Parameter Users:** If you have custom parameters that override `renderHtml`:
    *   Consider creating a custom `HtmlRenderer` that handles your new type.
    *   Or, if you just want to tweak the HTML, subclass `StandardHtmlRenderer` and override the specific render method.

For more details on customization, see the [Custom Rendering Guide](CustomRendering.md).
