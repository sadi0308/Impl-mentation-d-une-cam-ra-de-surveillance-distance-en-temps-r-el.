#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "fb_gfx.h"
#include "soc/soc.h"             // désactiver les problèmes de baisse de tension
#include "soc/rtc_cntl_reg.h"    // désactiver les problèmes de baisse de tension
#include "esp_http_server.h"
#include "ESP32Servo.h"
#include "fd_forward.h"
#include "fr_forward.h"
#include <Arduino.h>
#include <SoftwareSerial.h>
#include <EEPROM.h> 
#include "driver/rtc_io.h"
#include "FS.h"                
#include "SD_MMC.h"
#include <SPI.h>
#include <SD.h>
#include <ArduinoJson.h>


#define EEPROM_SIZE 1

SoftwareSerial myserial(2,4); // RX , TX  

Servo servoN1;
Servo servoN2;
Servo servo2;

static bool bal = false ;
TaskHandle_t Task1H;

TaskHandle_t Task3H;
int Sortie_capteur_PIR = 12;

 
 int pictureNumber = 0;

//int  pictureNumber = EEPROM.read(0)+1;

IRAM_ATTR void Sendsms (){

   bal = true ;
    

  Serial.println("Mouvement détecté");

   myserial.println("AT"); // initialise la sim808 // pour utiliser le baud rate qu'on a utiliser
  	

  //Sélectionne le format du message SMS sous forme de texte.par défaut est Protocol Data Unit (PDU)
  myserial.println("AT+CMGF=1");
   

  // Entrez votre numéro de téléphone 
  myserial.print("AT+CMGS=\"+213540885925\"\n"); // Configurer le numero au quel envoyer le message
  

    // Entrez votre message a envoyer ici 
  myserial.println("y'a un mouvement !!! Allumez la camera,Go to: http://192.168.43.220");

  // CTR+Z en langage ASCII, indique la fin du message
  myserial.println(char(26));			 
  
  Serial.println("Le Message a été bien envoyé .....");
}

 


// COORDONNEES WIFI
const char* ssid = "sof";          //Nom du WIFI 
const char* password = "12341234";//Mot de passe


static int8_t detection_enabled = 0;
static int8_t recognition_enabled = 0;
static int8_t is_enrolling = 0;
static face_id_list id_list = {0};
static mtmn_config_t mtmn_config = {0};
#define PART_BOUNDARY "123456789000000000000987654321"

//DEFINIR LES PINS DE L'AI-THINKER
  #define PWDN_GPIO_NUM     32
  #define RESET_GPIO_NUM    -1
  #define XCLK_GPIO_NUM      0
  #define SIOD_GPIO_NUM     26
  #define SIOC_GPIO_NUM     27
  
  #define Y9_GPIO_NUM       35
  #define Y8_GPIO_NUM       34
  #define Y7_GPIO_NUM       39
  #define Y6_GPIO_NUM       36
  #define Y5_GPIO_NUM       21
  #define Y4_GPIO_NUM       19
  #define Y3_GPIO_NUM       18
  #define Y2_GPIO_NUM        5
  #define VSYNC_GPIO_NUM    25
  #define HREF_GPIO_NUM     23
  #define PCLK_GPIO_NUM     22

#define SERVO     15

#define SERVO_STEP   5



//int servo2Pos = 90;


static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";
typedef struct {
        httpd_req_t *req;
        size_t len;
} jpg_chunking_t;
httpd_handle_t camera_httpd = NULL;
httpd_handle_t stream_httpd = NULL;

static const char PROGMEM INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html>
  <head>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width,initial-scale=1" />
    <title>servocam</title>
    <style>
      body {
        font-family: Arial, Helvetica, sans-serif;
        background: #181818;
        color: #efefef;
        font-size: 16px;
      }

      figure {
        padding: 0px;
        margin: 0;
        -webkit-margin-before: 0;
        margin-block-start: 0;
        -webkit-margin-after: 0;
        margin-block-end: 0;
        -webkit-margin-start: 0;
        margin-inline-start: 0;
        -webkit-margin-end: 0;
        margin-inline-end: 0;
      }

      figure img {
        display: block;
        width: 100%;
        height: auto;
        border-radius: 4px;
        margin-top: 8px;
      }

      @media (min-width: 800px) and (orientation: landscape) {
        figure img {
          display: block;
          max-width: 100%;
          max-height: calc(100vh - 40px);
          width: auto;
          height: auto;
        }

        figure {
          padding: 0 0 0 0px;
          margin: 0;
          -webkit-margin-before: 0;
          margin-block-start: 0;
          -webkit-margin-after: 0;
          margin-block-end: 0;
          -webkit-margin-start: 0;
          margin-inline-start: 0;
          -webkit-margin-end: 0;
          margin-inline-end: 0;
        }
      }

      section#buttons {
        display: flex;
        flex-wrap: nowrap;
        justify-content: space-between;
      }

      button {
        display: block;
        margin: 5px;
        padding: 0 12px;
        border: 0;
        line-height: 28px;
        cursor: pointer;
        color: #fff;
        background: #0004ffaf;
        border-radius: 5px;
        font-size: 16px;
        outline: 0;
      }

      button:hover {
        background: #4400ff8a;
      }

      button:active {
        background: blue;
      }

      .image-container {
        position: relative;
        min-width: 160px;
      }

      .close {
        position: absolute;
        right: 5px;
        top: 5px;
        background: blue;
        width: 16px;
        height: 16px;
        border-radius: 100px;
        color: #fff;
        text-align: center;
        line-height: 18px;
        cursor: pointer;
      }

      .hidden {
        display: none;
      }
    </style>
  </head>
  <body>
    <div>
      <nav>
        <section id="buttons">
          <button id="get-still">Prendre une photo</button>
          <button id="toggle-stream">Visualiser</button>
        </section>
      </nav>
       <center>
       <table>
      <tr>
      <td align="center"><button class="button" onmousedown="toggleCheckbox('left');" ontouchstart="toggleCheckbox('left');">Left</button></td>
      <td align="center"><button class="button" onmousedown="toggleCheckbox('right');" ontouchstart="toggleCheckbox('right');">Right</button></td>
      </tr>
       </table>
        </center>
    </div>
    <figure>
      <div id="stream-container" class="image-container hidden">
        <div class="close" id="close-stream">x</div>
        <img id="stream" src="" />
      </div>
    </figure>
    <figure>
      <div id="image-container" class="image-container hidden">
        <div class="close" id="close-image">x</div>

        <a href="#" download><img id="enreg" src="" /></a>
      </div>
    </figure>

    <script>
      document.addEventListener("DOMContentLoaded", function (event) {
        var baseHost = document.location.origin;
        var streamUrl = baseHost + ":81";

        const hide = (el) => {
          el.classList.add("hidden");
        };
        const show = (el) => {
          el.classList.remove("hidden");
        };

        const disable = (el) => {
          el.classList.add("disabled");
          el.disabled = true;
        };

        const enable = (el) => {
          el.classList.remove("disabled");
          el.disabled = false;
        };

        const updateValue = (el, value, updateRemote) => {
          updateRemote = updateRemote == null ? true : updateRemote;
          let initialValue;
         

          if (updateRemote && initialValue !== value) {
            updateConfig(el);
          } else if (!updateRemote) {
            if (el.id === "aec") {
              value ? hide(exposure) : show(exposure);
            } else if (el.id === "agc") {
              if (value) {
                show(gainCeiling);
                hide(agcGain);
              } else {
                hide(gainCeiling);
                show(agcGain);
              }
            } else if (el.id === "awb_gain") {
              value ? show(wb) : hide(wb);
            } else if (el.id === "face_recognize") {
              value ? enable(enrollButton) : disable(enrollButton);
            }
          }
        };

        function updateConfig(el) {
          let value;
          switch (el.type) {
            case "range":
            case "select-one":
              value = el.value;
              break;
            case "button":
            case "submit":
              value = "1";
              break;
            default:
              return;
          }

          const query = `${baseHost}/control?var=${el.id}&val=${value}`;

          fetch(query).then((response) => {
            console.log(
              `request to ${query} finished, status: ${response.status}`
            );
          });
        }

        document.querySelectorAll(".close").forEach((el) => {
          el.onclick = () => {
            hide(el.parentNode);
          };
        });

        // read initial values
        fetch(`${baseHost}/status`)
          .then(function (response) {
            return response.json();
          })
          .then(function (state) {
            document.querySelectorAll(".default-action").forEach((el) => {
              updateValue(el, state[el.id], false);
            });
          });

        const view = document.getElementById("stream");
        const view2 = document.getElementById("enreg");
        const viewContainer = document.getElementById("stream-container");
        const viewContainer2 = document.getElementById("image-container");
        const stillButton = document.getElementById("get-still");
        const streamButton = document.getElementById("toggle-stream");
        const leftButton = document.getElementById("left");
        const rightButton = document.getElementById("right");
        const closeButton = document.getElementById("close-stream");
        const closeButton2 = document.getElementById("close-image");

        const stopStream = () => {
          window.stop();
          streamButton.innerHTML = "Visualisation";
        };

        const startStream = () => {
          view.src = `${streamUrl}/stream`;
          show(viewContainer);
          streamButton.innerHTML = "Arreter la visualisation";
        };

        const goleft = () => {
          view.src = `${streamUrl}/left`;
          show(viewContainer);
          streamButton.innerHTML = "Arreter la visualisation";
        };

        // Attach actions to buttons
        stillButton.onclick = () => {
          var ba = `${baseHost}/capture?_cb=${Date.now()}`;
          view2.src = ba;
          var baa = document.querySelector("a");
          baa.setAttribute("href", ba);
          show(viewContainer2);
        };

        closeButton.onclick = () => {
          stopStream();
          hide(viewContainer);
        };
        closeButton2.onclick = () => {
          hide(viewContainer2);
        };

        streamButton.onclick = () => {
          const streamEnabled =
            streamButton.innerHTML === "Arreter la visualisation";
          if (streamEnabled) {
            stopStream();
          } else {
            startStream();
          }
        };

        // Attach default on change action
        document.querySelectorAll(".default-action").forEach((el) => {
          el.onchange = () => updateConfig(el);
        });
      });
    </script>
    <script>
   function toggleCheckbox(x) {
     var xhr = new XMLHttpRequest();
     xhr.open("GET", "/action?go=" + x, true);
     xhr.send();
   }
  </script>
  </body>
</html>

)rawliteral";


//Le handler pour la page html.
static esp_err_t index_handler(httpd_req_t *req){
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, (const char *)INDEX_HTML, strlen(INDEX_HTML));
}

static size_t jpg_encode_stream(void * arg, size_t index, const void* data, size_t len){
    jpg_chunking_t *j = (jpg_chunking_t *)arg;
    if(!index){
        j->len = 0;
    }
    if(httpd_resp_send_chunk(j->req, (const char *)data, len) != ESP_OK){
        return 0;
    }
    j->len += len;
    return len;
}

//Le handler pour le streaming.
static esp_err_t stream_handler(httpd_req_t *req){
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;
  char * part_buf[64];

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if(res != ESP_OK){
    return res;
  }

  while(true){
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
    } else {
      if(fb->width > 400){
        if(fb->format != PIXFORMAT_JPEG){
          bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
          esp_camera_fb_return(fb);
          fb = NULL;
          if(!jpeg_converted){
            Serial.println("JPEG compression failed");
            res = ESP_FAIL;
          }
        } else {
          _jpg_buf_len = fb->len;
          _jpg_buf = fb->buf;
        }
      }
    }
    if(res == ESP_OK){
      size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if(res == ESP_OK){
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if(res == ESP_OK){
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    if(fb){
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if(_jpg_buf){
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    if(res != ESP_OK){
      break;
    }
  }
  return res;
}

//Le handler pour les commandes du servo(left,right)
static esp_err_t cmd_handler(httpd_req_t *req){
  char*  buf;
  size_t buf_len;
  char variable[32] = {0,};
  
  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = (char*)malloc(buf_len);
    if(!buf){
      httpd_resp_send_500(req);
      return ESP_FAIL;
    }
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      if (httpd_query_key_value(buf, "go", variable, sizeof(variable)) == ESP_OK) {
      } else {
        free(buf);
        httpd_resp_send_404(req);
        return ESP_FAIL;
      }
    } else {
      free(buf);
      httpd_resp_send_404(req);
      return ESP_FAIL;
    }
    free(buf);
  } else {
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }

  sensor_t * s = esp_camera_sensor_get();
  

  int res = 0;
  
  int currentPos = servo2.read();
 if(!strcmp(variable, "left")) {
      currentPos += 20;
      
      servo2.write(currentPos);
    
    Serial.println(currentPos);
    Serial.println("Left");
  }
  else
  if(!strcmp(variable, "right")) {
    currentPos -= 20;
        servo2.write(currentPos);
    
    Serial.println(currentPos);
    Serial.println("Right");
  }

  else {
    res = -1;
  }

  if(res){
    return httpd_resp_send_500(req);
  }

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, NULL, 0);
}

//Le handler pour la capture d'image.
static esp_err_t capture_handler(httpd_req_t *req){
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    int64_t fr_start = esp_timer_get_time();

    fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera capture failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    size_t out_len, out_width, out_height;
    uint8_t * out_buf;
    bool s;
    bool detected = false;
    int face_id = 0;
    if(!detection_enabled || fb->width > 400){
        size_t fb_len = 0;
        if(fb->format == PIXFORMAT_JPEG){
            fb_len = fb->len;
            res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
        } else {
            jpg_chunking_t jchunk = {req, 0};
            res = frame2jpg_cb(fb, 80, jpg_encode_stream, &jchunk)?ESP_OK:ESP_FAIL;
            httpd_resp_send_chunk(req, NULL, 0);
            fb_len = jchunk.len;
        }
        esp_camera_fb_return(fb);
        int64_t fr_end = esp_timer_get_time();
        Serial.printf("JPG: %uB %ums\n", (uint32_t)(fb_len), (uint32_t)((fr_end - fr_start)/1000));
        return res;
    }

    dl_matrix3du_t *image_matrix = dl_matrix3du_alloc(1, fb->width, fb->height, 3);
    if (!image_matrix) {
        esp_camera_fb_return(fb);
        Serial.println("dl_matrix3du_alloc failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    out_buf = image_matrix->item;
    out_len = fb->width * fb->height * 3;
    out_width = fb->width;
    out_height = fb->height;

    s = fmt2rgb888(fb->buf, fb->len, fb->format, out_buf);
    esp_camera_fb_return(fb);
    if(!s){
        dl_matrix3du_free(image_matrix);
        Serial.println("to rgb888 failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    box_array_t *net_boxes = face_detect(image_matrix, &mtmn_config);
    jpg_chunking_t jchunk = {req, 0};
    s = fmt2jpg_cb(out_buf, out_len, out_width, out_height, PIXFORMAT_RGB888, 90, jpg_encode_stream, &jchunk);
    dl_matrix3du_free(image_matrix);
    if(!s){
        Serial.println("JPEG compression failed");
        return ESP_FAIL;
    }

    int64_t fr_end = esp_timer_get_time();
    Serial.printf("FACE: %uB %ums %s%d\n", (uint32_t)(jchunk.len), (uint32_t)((fr_end - fr_start)/1000), detected?"DETECTED ":"", face_id);
    return res;
}

void startCameraServer(){
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  
  httpd_uri_t index_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = index_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t cmd_uri = {
    .uri       = "/action",
    .method    = HTTP_GET,
    .handler   = cmd_handler,
    .user_ctx  = NULL
  };
  httpd_uri_t stream_uri = {
    .uri       = "/stream",
    .method    = HTTP_GET,
    .handler   = stream_handler,
    .user_ctx  = NULL
  };
   httpd_uri_t capture_uri = {
        .uri       = "/capture",
        .method    = HTTP_GET,
        .handler   = capture_handler,
        .user_ctx  = NULL
    };

    //Lancer la page html,les commandes et la capture d'image sur le port 80.
  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &cmd_uri);
    httpd_register_uri_handler(camera_httpd, &capture_uri);
  }
  config.server_port += 1;
  config.ctrl_port += 1;

  //Lancer le streaming sur le port 80.
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
  }
}

void Task1(void *Parameters){

   attachInterrupt(digitalPinToInterrupt(Sortie_capteur_PIR),Sendsms,FALLING);
 
vTaskDelete(NULL);
    }
void Task2(void *Parameters){

        startCameraServer();

vTaskDelete(NULL);

    }

  
  
   


void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
   servo2.setPeriodHertz(50);
      servoN1.attach(14, 1000, 2000);

  servoN2.attach(13, 1000, 2000);
  servo2.attach(SERVO, 1000, 2000);

  int current = servo2.read();
   servo2.write(current);

  
  Serial.begin(9600);
  Serial.setDebugOutput(false);
    myserial.begin(9600);

  
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG; 
  
  //PSRAM
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
 
  
  // Initialisation de la camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  // Wi-Fi connection
  WiFi.begin(ssid, password);
  Serial.print("Connexion au WIFI ");Serial.println(ssid);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connecté");
  Serial.print("Camera Stream Ready! Go to: http://");Serial.println(WiFi.localIP());
   pinMode(12, INPUT_PULLUP);
  // Start streaming web server
  //startCameraServer();
  
   xTaskCreatePinnedToCore(Task1," Task1", 1024, NULL,1,&Task1H,0);
   xTaskCreatePinnedToCore(Task2,"Task2",1024*10,NULL,1,NULL,0); 


}

void loop() {

  if (WiFi.status() != WL_CONNECTED) { 
          WiFi.begin(ssid, password);

    Serial.print("Connexion au WIFI ");Serial.println(ssid);
  while (WiFi.status() != WL_CONNECTED) {
        delay(500);
       Serial.print(".");
        }
    Serial.println("Connecté");
  Serial.print("Camera Stream Ready! Go to: http://");Serial.println(WiFi.localIP());    
  }
 

 } 
  

 

 