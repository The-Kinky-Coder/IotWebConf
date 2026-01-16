#include <IotWebConf.h>
#include <IotWebConfHtmlRenderer.h>

/**
 * Example: Custom Bootstrap Renderer
 * Description:
 *   This example shows how to use a custom HTML renderer to output 
 *   Bootstrap-styled forms instead of the default IotWebConf styling.
 */

const char thingName[] = "BootstrapThing";
const char wifiInitialApPassword[] = "smrtTHNG8266";

DNSServer dnsServer;
WebServer server(80);
IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword);

// -- Custom Renderer Class
class BootstrapHtmlRenderer : public iotwebconf::StandardHtmlRenderer
{
public:
    BootstrapHtmlRenderer() : iotwebconf::StandardHtmlRenderer() {}

protected:
    // We override the internal helper to wrap parameters in Bootstrap form-groups
    String renderTextParameterHtml(
        iotwebconf::TextParameter* param, 
        const char* type, 
        bool hasValueFromPost, 
        String valueFromPost) override
    {
        // Simple Bootstrap 4/5 structure
        // <div class="mb-3">
        //   <label for="id" class="form-label">Label</label>
        //   <input type="..." class="form-control" id="..." ...>
        //   <div class="invalid-feedback">Error</div>
        // </div>
        
        String pitem = "<div class='mb-3'>";
        
        pitem += "<label for='";
        pitem += param->getId();
        pitem += "' class='form-label'>";
        pitem += param->label;
        pitem += "</label>";
        
        pitem += "<input type='";
        pitem += type;
        pitem += "' class='form-control";
        
        if (param->errorMessage != nullptr) {
            pitem += " is-invalid";
        }
        
        pitem += "' id='";
        pitem += param->getId();
        pitem += "' name='";
        pitem += param->getId();
        pitem += "' maxlength='";
        pitem += String(param->getLength() - 1);
        pitem += "' placeholder='";
        pitem += (param->placeholder == nullptr ? "" : param->placeholder);
        pitem += "' value='";
        
        if (hasValueFromPost) {
            pitem += valueFromPost;
        } else {
            pitem += param->valueBuffer;
        }
        
        pitem += "'";
        if (param->customHtml != nullptr) {
            pitem += " ";
            pitem += param->customHtml;
        }
        pitem += "/>"; // End input
        
        if (param->errorMessage != nullptr) {
             pitem += "<div class='invalid-feedback'>";
             pitem += param->errorMessage;
             pitem += "</div>";
        }
        
        pitem += "</div>\n"; // End div
        
        return pitem;
    }
    
    // Override group rendering to use Cards
    void renderParameterGroup(
        iotwebconf::ParameterGroup* group, 
        bool dataArrived, 
        iotwebconf::WebRequestWrapper* webRequestWrapper) override
    {
        if (group->label != nullptr) {
            // Card Start
            webRequestWrapper->sendContent(
                "<div class='card mb-4'><div class='card-header'>" + String(group->label) + 
                "</div><div class='card-body'>"
            );
        }
        
        // Render items. Since we can't easily access items, we rely on the base class behavior
        // which renders items. However, the base class also renders <fieldset> tags.
        // If we want to strictly avoid fieldsets inside cards, we would need library changes to expose items.
        // For this example, we accept the fieldset or assume the user would modify CSS to hide it.
        // Alternatively, since we are inheriting from StandardHtmlRenderer, we can just call its method.
        
        iotwebconf::StandardHtmlRenderer::renderParameterGroup(group, dataArrived, webRequestWrapper);

        if (group->label != nullptr) {
             webRequestWrapper->sendContent("</div></div>");
        }
    }
};

// -- Custom HTML Format Provider to include Bootstrap CDN
class BootstrapFormatProvider : public iotwebconf::HtmlFormatProvider
{
    String getHead() override {
        return "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>"
               "<link href=\"https://cdn.jsdelivr.net/npm/bootstrap@5.1.3/dist/css/bootstrap.min.css\" rel=\"stylesheet\">"
               "<title>{v}</title>\n";
    }
    
    String getStyle() override {
        return "<style>body { padding: 20px; } fieldset { border: none; padding: 0; margin: 0; } legend { display: none; }</style>";
    }
    
    String getScript() override {
        return ""; // Bootstrap doesn't strictly need JS for basic forms
    }
    
    String getFormStart() override {
        return "<div class='container'><div class='row justify-content-center'><div class='col-md-8'>"
               "<form action='' method='post'><input type='hidden' name='iotSave' value='true'>\n";
    }
    
    String getFormEnd() override {
        return "<button type='submit' class='btn btn-primary mt-3'>Apply Settings</button></form>"
               "</div></div></div>\n";
    }
};

BootstrapHtmlRenderer bootstrapRenderer;
BootstrapFormatProvider bootstrapFormatProvider;

// -- Params
char myParamValue[32];
iotwebconf::TextParameter myParam = 
  iotwebconf::TextParameter("My Parameter", "myParam", myParamValue, 32);

void handleRoot() {
  if (iotWebConf.handleCaptivePortal()) return;
  String s = "<!DOCTYPE html><html><body>Go to <a href='config'>configure page</a></body></html>";
  server.send(200, "text/html", s);
}

void setup() 
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("Starting up...");

  iotWebConf.addSystemParameter(&myParam);
  
  // -- INJECT CUSTOM RENDERER
  iotWebConf.setHtmlRenderer(&bootstrapRenderer);
  iotWebConf.setHtmlFormatProvider(&bootstrapFormatProvider);

  iotWebConf.init();

  server.on("/", handleRoot);
  server.on("/config", []{ iotWebConf.handleConfig(); });
  server.onNotFound([](){ iotWebConf.handleNotFound(); });

  Serial.println("Ready.");
}

void loop() 
{
  iotWebConf.doLoop();
}
