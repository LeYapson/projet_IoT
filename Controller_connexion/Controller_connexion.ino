#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Bluepad32.h>

// Broches pour le DRV8833
#define AIN1 26  // Moteur A - Avant
#define AIN2 25  // Moteur A - Arrière
#define BIN1 33  // Moteur B - Avant
#define BIN2 32  // Moteur B - Arrière

// Broches I2C pour l'OLED
#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_RESET -1
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
ControllerPtr myControllers[BP32_MAX_GAMEPADS];

// Variables pour l'accélération progressive
int currentMotorASpeed = 0;
int currentMotorBSpeed = 0;
int targetMotorASpeed = 0;
int targetMotorBSpeed = 0;

// Variables pour les délais
unsigned long lastMotorUpdate = 0;
const unsigned long motorUpdateDelay = 20; // Délai de 20ms entre les mises à jour des moteurs

// Seuil de deadzone pour les joysticks et gâchettes
const int deadzone = 10;

// Callback pour la connexion/déconnexion
void onConnectedController(ControllerPtr ctl) {
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (myControllers[i] == nullptr) {
            Serial.printf("Manette connectée (index=%d)\n", i);
            myControllers[i] = ctl;
            break;
        }
    }
}

void onDisconnectedController(ControllerPtr ctl) {
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (myControllers[i] == ctl) {
            Serial.printf("Manette déconnectée (index=%d)\n", i);
            myControllers[i] = nullptr;
            break;
        }
    }
}

// Fonction pour contrôler un moteur avec accélération progressive
void setMotor(int in1, int in2, int speed) {
    speed = constrain(speed, -255, 255);
    if (speed > 0) {
        digitalWrite(in1, HIGH);
        digitalWrite(in2, LOW);
    } else if (speed < 0) {
        digitalWrite(in1, LOW);
        digitalWrite(in2, HIGH);
    } else {
        digitalWrite(in1, LOW);
        digitalWrite(in2, LOW);
    }
}

// Fonction pour appliquer une accélération progressive
void updateMotorSpeed() {
    unsigned long currentTime = millis();
    if (currentTime - lastMotorUpdate >= motorUpdateDelay) {
        lastMotorUpdate = currentTime;

        // Appliquer une accélération progressive
        if (currentMotorASpeed < targetMotorASpeed) {
            currentMotorASpeed += 10;
            if (currentMotorASpeed > targetMotorASpeed) {
                currentMotorASpeed = targetMotorASpeed;
            }
        } else if (currentMotorASpeed > targetMotorASpeed) {
            currentMotorASpeed -= 10;
            if (currentMotorASpeed < targetMotorASpeed) {
                currentMotorASpeed = targetMotorASpeed;
            }
        }

        if (currentMotorBSpeed < targetMotorBSpeed) {
            currentMotorBSpeed += 10;
            if (currentMotorBSpeed > targetMotorBSpeed) {
                currentMotorBSpeed = targetMotorBSpeed;
            }
        } else if (currentMotorBSpeed > targetMotorBSpeed) {
            currentMotorBSpeed -= 10;
            if (currentMotorBSpeed < targetMotorBSpeed) {
                currentMotorBSpeed = targetMotorBSpeed;
            }
        }

        // Appliquer les vitesses actuelles aux moteurs
        setMotor(AIN1, AIN2, currentMotorASpeed);
        setMotor(BIN1, BIN2, currentMotorBSpeed);
    }
}

// Logique de contrôle avec gâchettes et joystick
void processGamepad(ControllerPtr ctl) {
    // Gâchette droite pour avancer
    int rightTrigger = ctl->brake();  // Valeur de 0 à 255
    // Gâchette gauche pour reculer
    int leftTrigger = ctl->throttle();   // Valeur de 0 à 255
    // Joystick gauche pour tourner
    int leftX = ctl->axisX();           // Valeur de -511 à 511

    // Appliquer la deadzone au joystick et aux gâchettes
    if (abs(leftX) < deadzone) leftX = 0;
    if (rightTrigger < deadzone) rightTrigger = 0;
    if (leftTrigger < deadzone) leftTrigger = 0;

    // Calculer la vitesse de base en fonction des gâchettes
    int baseSpeed = 0;
    if (rightTrigger > deadzone && leftTrigger <= deadzone) {
        baseSpeed = map(rightTrigger, deadzone, 255, 0, 255);  // Avancer
    } else if (leftTrigger > deadzone && rightTrigger <= deadzone) {
        baseSpeed = -map(leftTrigger, deadzone, 255, 0, 255); // Reculer
    }
    // Si les deux gâchettes sont enfoncées, on ignore l'une des deux (ici on priorise la gâchette droite)
    else if (rightTrigger > deadzone && leftTrigger > deadzone) {
        baseSpeed = map(rightTrigger, deadzone, 255, 0, 255);  // Priorité à avancer
    }

    // Calculer les vitesses cibles pour chaque moteur en fonction du joystick
    targetMotorASpeed = baseSpeed - leftX;
    targetMotorBSpeed = baseSpeed + leftX;

    // Limiter les vitesses cibles
    targetMotorASpeed = constrain(targetMotorASpeed, -255, 255);
    targetMotorBSpeed = constrain(targetMotorBSpeed, -255, 255);

    // Affichage OLED
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    // Barre pour Moteur A (en haut)
    display.setCursor(0, 0);
    display.print("Moteur A:");
    int barA = map(abs(currentMotorASpeed), 0, 255, 0, 60);
    display.fillRect(64, 0, barA, 8, SSD1306_WHITE);
    display.drawRect(64, 0, 60, 8, SSD1306_WHITE);

    // Barre pour Moteur B (au milieu)
    display.setCursor(0, 10);
    display.print("Moteur B:");
    int barB = map(abs(currentMotorBSpeed), 0, 255, 0, 60);
    display.fillRect(64, 10, barB, 8, SSD1306_WHITE);
    display.drawRect(64, 10, 60, 8, SSD1306_WHITE);

    // Valeurs numériques
    display.setCursor(0, 20);
    display.print("A:");
    display.print(currentMotorASpeed);
    display.print(" B:");
    display.print(currentMotorBSpeed);

    display.display();

    Serial.printf("RT: %d, LT: %d, X: %d | A: %d, B: %d\n", rightTrigger, leftTrigger, leftX, currentMotorASpeed, currentMotorBSpeed);
}

// Traitement des contrôleurs
void processControllers() {
    for (auto myController : myControllers) {
        if (myController && myController->isConnected() && myController->hasData()) {
            processGamepad(myController);
            updateMotorSpeed(); // Mettre à jour les vitesses des moteurs
        }
    }
}

// Configuration initiale
void setup() {
    Serial.begin(115200);
    Wire.begin(OLED_SDA, OLED_SCL);

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("Échec OLED"));
        while (1);
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.println("Initialisation...");
    display.display();

    pinMode(AIN1, OUTPUT);
    pinMode(AIN2, OUTPUT);
    pinMode(BIN1, OUTPUT);
    pinMode(BIN2, OUTPUT);

    BP32.setup(&onConnectedController, &onDisconnectedController);
    BP32.forgetBluetoothKeys();
}

// Boucle principale
void loop() {
    BP32.update();
    processControllers();
    delay(10);
}