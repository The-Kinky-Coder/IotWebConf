# Customizing IotWebConf HTML Rendering

Since version 3.2.0, IotWebConf has separated the HTML rendering logic from the configuration logic ("View" vs "Model" separation). This allows you to completely customize the look and feel of the configuration portal, for example, to use a CSS framework like Bootstrap, without modifying the library code.

## How it works

The core `IotWebConf` class and all parameter classes (`TextParameter`, `CheckboxParameter`, etc.) no longer contain hardcoded HTML strings. Instead, they delegate the HTML generation to an `HtmlRenderer` class.

The default implementation is `StandardHtmlRenderer`, which produces the classic IotWebConf look.

To change the appearance, you can:
1.  **Override specific render methods**: Subclass `StandardHtmlRenderer` and override only the methods you want to change (e.g., just the text inputs).
2.  **Replace the renderer entirely**: Implement the `HtmlRenderer` interface from scratch.

## Example: Using a Custom Renderer

Here is a conceptual example of how to create a custom renderer that wraps inputs in a `<div>` with a specific class (e.g., for Bootstrap).

### 1. Define your Custom Renderer

Create a class that inherits from `StandardHtmlRenderer`.

```cpp
#include <IotWebConf.h>
#include <IotWebConfHtmlRenderer.h>

class BootstrapHtmlRenderer : public iotwebconf::StandardHtmlRenderer
{
protected:
    // Override the method that renders text parameters (Text, Number, Password)
    // Note: This relies on the internal helper `renderTextParameterHtml` 
    // or you can rewrite the logic entirely.
    
    // For a full customization, you might want to override renderTextParameterHtml
    // but it is virtual in StandardHtmlRenderer? Yes.
    
    String renderTextParameterHtml(
        iotwebconf::TextParameter* param, 
        const char* type, 
        bool hasValueFromPost, 
        String valueFromPost) override
    {
        // Custom HTML generation
        String pitem = "<div class='form-group'>";
        pitem += "<label for='" + String(param->getId()) + "'>" + String(param->label) + "</label>";
        pitem += "<input type='" + String(type) + "' class='form-control' id='" + String(param->getId()) + "' ... >";
        // ... handle value, max length, etc.
        pitem += "</div>";
        return pitem;
    }
    
    // You can also override renderParameterGroup to change how fieldsets are rendered
    void renderParameterGroup(
        iotwebconf::ParameterGroup* group, 
        bool dataArrived, 
        iotwebconf::WebRequestWrapper* webRequestWrapper) override
    {
         // Render card/panel instead of fieldset...
         webRequestWrapper->sendContent("<div class='card'>...");
         
         // Call base or iterate items manually
         StandardHtmlRenderer::renderParameterGroup(group, dataArrived, webRequestWrapper);
         
         webRequestWrapper->sendContent("</div>");
    }
};
```

### 2. Inject the Renderer

In your `setup()` function, tell `IotWebConf` to use your custom renderer.

```cpp
IotWebConf iotWebConf(...);
BootstrapHtmlRenderer myRenderer;

void setup()
{
  // ...
  iotWebConf.setHtmlRenderer(&myRenderer);
  // ...
  iotWebConf.init();
}
```

## Important Notes

*   **ABI Compatibility**: The `HtmlRenderer` system relies on `getType()` methods in the parameter classes to avoid `dynamic_cast`.
*   **Format Provider**: You might also want to customize the `HtmlFormatProvider` to change the global page structure (Head, Body, Scripts). Use `iotWebConf.setHtmlFormatProvider(&myFormatProvider)`.
