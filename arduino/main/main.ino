#include <BluetoothSerial.h>
#include <Adafruit_MotorShield.h>

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled!
#endif

// Create a global reference to our BlueTooth radio.
BluetoothSerial BT;

// Create global references to motors
Adafruit_MotorShield Shield = Adafruit_MotorShield();
// TODO[joe] Store as an array of motors?
Adafruit_DCMotor *Motor1 = Shield.getMotor(4);
Adafruit_DCMotor *Motor2 = Shield.getMotor(3);

// Save the pin address of the onboard LED
int LED = 13;

// Application state.
int MotorSpeed = 0;
int MotorDirection = 0;
int ServoDirection = 0;

/** Sets motors to given speed (limited to range between 0, 255) and direction.
 * TODO[joe] Allow for the motors to be de-synchronized. */
void SetMotors(int Speed, int Direction)
{
    if (Speed > 255)
        Speed = 255;

    else if (Speed < 0)
        Speed = 0;

    // NOTE[joe] setSpeed() accepts a range between 0 and 255.
    // This is perfect, as our controller's trigger has the same range.
    Motor1->setSpeed(Speed);
    Motor1->run(Direction);

    Motor2->setSpeed(Speed);
    Motor2->run(Direction);
}

/** Sets motor speed to zero and releases them. */
void ClearMotors(void)
{
    SetMotors(0, RELEASE);
}

void setup()
{
    // Start a serial bus output for debug.
    Serial.begin(115200);

    // Set onboard LED as an output pin.
    pinMode(LED, OUTPUT);

    // Give an indication that we are working
    digitalWrite(LED, HIGH);
    delay(3000);
    digitalWrite(LED, LOW);

    // Start up a BT service, listing this device as ESP32
    BT.begin("ESP32");
    // Initialize connection with motor control sheild.
    Shield.begin();

    // Announce that we are done with initialization.
    Serial.println("Device has started!");
}

void loop()
{
    /* Give an indication what we are waiting for a connection. */
    if (!BT.hasClient())
    {
        ClearMotors();

        digitalWrite(LED, HIGH);
        delay(250);
        digitalWrite(LED, LOW);
        delay(75);
        digitalWrite(LED, HIGH);
        delay(250);
        digitalWrite(LED, LOW);
        delay(500);
    }

    else
    {
        if (BT.available())
        {
            switch ((int) BT.read())
            {
                case 1:
                {
                    MotorDirection = FORWARD;
                } break;

                case 2:
                {
                    MotorDirection = BACKWARD;
                } break;

                default:
                {
                    // NOTE[joe] If we don't know where we're going, don't go.
                    // A safety measure more than anything else, as we don't
                    // want the vehicle assuming it can go in a direction that
                    // was not specified by the user.
                    MotorDirection = RELEASE;
                } break;
            }
            Serial.print("Motor Direction: ");
            Serial.println(MotorDirection);

            ServoDirection = (int) BT.read();
            Serial.print("Servo Direction: ");
            Serial.println(ServoDirection);

            MotorSpeed = (int) BT.read();
            Serial.print("Motor Speed: ");
            Serial.println(MotorSpeed);

            // NOTE[joe] Skip the padding of the instruction.
            BT.read();
        }

        if (MotorSpeed == 0 || MotorDirection == RELEASE)
            ClearMotors();

        else
            SetMotors(MotorSpeed, MotorSpeed);

        // TODO[joe] Set servo direction
    }
}
