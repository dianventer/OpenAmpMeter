#include <WiFi.h>
#include "ESPAsyncWebServer.h"
#include "ArduinoNvs.h"

const char *ext_ssid = "";
const char *ext_password = "";
const char *device_alias = "";
const char *ssid = "OpenAmpMeter Configuration";
const char *password = "12345678";
const char *static_html = R"HTML(
<!DOCTYPE html>
<html>
  <head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <style>
      body {
        font-family: Arial, sans-serif;
        background-color: #f7f7f7;
        display: flex;
        flex-direction: column;
        align-items: center;
        justify-content: space-between;
        /* height: 100vh; */
        width: 100%;
        min-width: 100%;
        margin: 0;
      }

      .header {
        max-width: 400px;
        padding: 20px;
        text-align: center;
        background-color: transparent;
        border-radius: 5px;
        margin-top: 20px;
      }
      h3 {
        color: #333;
        font-size: 24px;
        margin-top: 0;
        margin-bottom: 20px;
      }
      .container {
        min-width: 90%;
        padding: 20px;
        background-color: #fff;
        border-radius: 5px;
        box-shadow: 0 2px 5px rgba(0, 0, 0, 0.1);
        margin-bottom: 20px;
        padding-left: 10px;
        padding-right: 10px;
      }
      label {
        display: block;
        margin-bottom: 8px;
        color: #555;
        font-size: 14px;
      }
      input[type="text"],
      select {
        width: 100%;
        padding: 12px;
        margin-bottom: 20px;
        border: 1px solid #ccc;
        border-radius: 4px;
        box-sizing: border-box;
        font-size: 16px;
      }
      input[type="submit"] {
        width: 100%;
        background-color: #4285f4;
        color: white;
        padding: 14px 20px;
        border: none;
        border-radius: 8px;
        cursor: pointer;
        font-size: 16px;
        transition: background-color 0.3s ease;
      }
      input[type="submit"]:hover {
        background-color: #1a73e8;
      }
      @media (max-width: 480px) {
        .header,
        .container {
          max-width: 100%;
          padding: 10px;
        }
      }
    </style>
  </head>
  <body>
    <div class="header">
      <h3>OpenAmpMeter Config Tool</h3>
    </div>
    <div class="container">
      <form>
        <label for="falias">Device Alias</label>
        <input
          type="text"
          id="falias"
          name="alias"
          placeholder="Enter device alias.."
        />
        <label for="fssid">SSID</label>
        <input type="text" id="fssid" name="ssid" placeholder="WIFI SSID" />
        <label for="fpassword">Password</label>
        <input
          type="text"
          id="fpassword"
          name="password"
          placeholder="WIFI Password"
        />
      </form>
      <input type="submit" value="Submit" id="submit-btn" />
    </div>
    <script>
      var falias = document.getElementById("falias");
      var fssid = document.getElementById("fssid");
      var fpassword = document.getElementById("fpassword");
      var submitBtn = document.getElementById("submit-btn");
      function updateSubmitButton() {
        var alias = falias.value.trim();
        var ssid = fssid.value.trim();
        var password = fpassword.value.trim();
        if (!alias || !ssid || !password) {
          submitBtn.disabled = true;
        } else {
          submitBtn.disabled = false;
        }
      }
      falias.addEventListener("input", updateSubmitButton);
      fssid.addEventListener("input", updateSubmitButton);
      fpassword.addEventListener("input", updateSubmitButton);
      submitBtn.addEventListener("click", function () {
        var alias = falias.value.trim();
        var ssid = fssid.value.trim();
        var password = fpassword.value.trim();
        if (!alias || !ssid || !password) {
          alert("Please fill in all the fields.");
          return;
        }
        var url =
          "http://10.10.10.1/UpdateDevice" +
          "?device_alias=" +
          encodeURIComponent(alias) +
          "&ssid=" +
          encodeURIComponent(ssid) +
          "&pass=" +
          encodeURIComponent(password);
        fetch(url)
          .then(function (response) {
            if (response.ok) {
              alert("Update Sent Through! Restarting Device!");
            } else {
              throw new Error("Error: " + response.status);
            }
          })
          .catch(function (error) {
            alert("Failed to Update Credentials!");
          });
      });
    </script>
  </body>
</html>
)HTML";
IPAddress APIP(10, 10, 10, 1); // Private network for the configuration server
AsyncWebServer server(80);
void setup()
{
  Serial.begin(115200);
  NVS.begin();
  delay(500);

  device_alias = NVS.getString("device_alias").c_str();
  ext_ssid = NVS.getString("ssid").c_str();
  ext_password = NVS.getString("pass").c_str();
  Serial.println(device_alias);
  Serial.println(ext_ssid);
  Serial.println(ext_password);

  if (strcmp(device_alias, "") == 0)
  {
    WiFi.softAP(ssid, password);
    WiFi.softAPConfig(APIP, APIP, IPAddress(255, 255, 255, 0));
    IPAddress IP = WiFi.softAPIP();
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "text/html", static_html); });
    server.on("/UpdateDevice", HTTP_GET, [](AsyncWebServerRequest *request)
              {
    int paramsNr = request->params();
    for(int i=0;i<paramsNr;i++){
        AsyncWebParameter* p = request->getParam(i);
        // const char *paramName = p->name().c_str();
        // const char *paramValue = p->value().c_str();
        if(strcmp(p->name().c_str(), "device_alias") == 0){
          Serial.println(p->value());
          NVS.setString("device_alias", p->value());
        } else if(strcmp(p->name().c_str(), "ssid") == 0){
          Serial.println(p->value());
          NVS.setString("ssid", p->value());
        } else if(strcmp(p->name().c_str(), "pass") == 0){
          Serial.println(p->value());
           NVS.setString("ssid", p->value());
        }
    }
  
    request->send(200, "text/html", "message received");
    delay(1000);
    ESP.restart(); });
    server.begin();
  }
  else
  {
    Serial.println("Device alias: " + NVS.getString("device_alias"));
  }
}

void loop()
{
}