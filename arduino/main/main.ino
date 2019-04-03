/*
 * @file main.ino
 * @author Joseph Miles <josephmiles2015@gmail.com>
 * @date 2019-03-08
 *
 * This is our main driver for our Adafruit Feather microcontroller.
 */

#include <BluetoothSerial.h>
#include <Adafruit_MotorShield.h>

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled!
#endif

#define STATE_READY  0
#define STATE_BEGIN  1
#define STATE_READ   2
#define STATE_DECIDE 3
#define STATE_ACCEPT 4
#define STATE_REJECT 5

// Create a global reference to our BlueTooth radio.
BluetoothSerial BT;

// Create global references to motors
Adafruit_MotorShield Shield = Adafruit_MotorShield();
// TODO[joe] Store as an array of motors?
Adafruit_DCMotor *Motor1 = Shield.getMotor(4);
Adafruit_DCMotor *Motor2 = Shield.getMotor(3);

// Save the pin address of the onboard LED
int LED = 13;

// BT Receiver state.
// NOTE[joe] Largest payload we can receive is 255B.
// We use 256B due to needing an extra byte for the null terminator.
char ReceiveBuffer[256];
char ReceiveBufferHead = 0;
int ReceiverState = STATE_READY;
int PayloadSize = 0;

// Application state.
int MotorSpeed = 0;
int MotorDirection = RELEASE;
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
        while (BT.available() > 0)
        {
            if (ReceiverState == STATE_READY)
            {
                char ReceivedData = BT.read();

                if (ReceivedData == '[')
                    ReceiverState = STATE_BEGIN;
            }

            if (ReceiverState == STATE_BEGIN && BT.peek() != -1)
            {
                PayloadSize = (int) BT.read();
                ReceiverState = STATE_READ;
            }

            if (ReceiverState == STATE_READ && BT.peek() != -1)
            {
                if (ReceiveBufferHead < PayloadSize)
                    ReceiveBuffer[ReceiveBufferHead++] = BT.read();

                else
                    ReceiverState = STATE_DECIDE;
            }

            if (ReceiverState == STATE_DECIDE && BT.peek() != -1)
            {
                if (BT.read() == ']')
                {
                    ReceiverState = STATE_ACCEPT;
                    break;
                }

                else
                {
                    ReceiverState = STATE_READY;
                    ReceiveBufferHead = 0;
                }
            }
        }

        // If we ACCEPT the received buffer, use the data we received.
        if (ReceiverState == STATE_ACCEPT)
        {
            MotorDirection = ReceiveBuffer[0];
            ServoDirection = ReceiveBuffer[1];
            MotorSpeed     = ReceiveBuffer[2];

            ReceiverState = STATE_READY;
            ReceiveBufferHead = 0;
        }

        if (MotorSpeed == 0 || MotorDirection == RELEASE)
            ClearMotors();

        else
            SetMotors(MotorSpeed, MotorDirection);

        // TODO[joe] Set servo direction
    }
}
