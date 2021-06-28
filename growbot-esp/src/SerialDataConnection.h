// #include<Arduino.h>
// #include "DebugUtils.h"
// #include "GrowbotData.h"
// #include "DataConnection.h"
// #include "hardware/uart.h"
// #include "hardware/irq.h"

// #define SERIAL_DATA_UART uart1
// #define SERIAL_DATA_RX_PIN 1
// #define SERIAL_DATA_TX_PIN 2
// #define SERIAL_DATA_BAUD 115200
// #define SERIAL_DATA_DATA_BITS 8
// #define SERIAL_DATA_STOP_BITS 1
// #define SERIAL_DATA_PARITY UART_PARITY_NONE


// #define SERIAL_DATA_READ_TIMEOUT_US 50000

// #define DATA_MAX_BYTES 1 + sizeof(GrowbotState)

// #ifndef SERIALDATACONNECTION_H
// #define SERIALDATACONNECTION_H
// class SerialDataConnection : public DataConnection {
//     public:
//         void handle() {
//             if (!Serial1.available()) {
//                 return;
//             }
//             dbg.println("got something on serial");
//             Serial1.setTimeout(1000);
//             uint8_t cmd = Serial1.read();
//             uint readCtr = 0;
//             GrowbotConfig newConfig;
//             uint8_t mode = 0;
//             switch (cmd) {
//                 case CMD_SET_OPERATING_MODE:
                
//                     if (!uart_is_readable_within_us(SERIAL_DATA_UART, SERIAL_DATA_READ_TIMEOUT_US)) {
//                         dbg.printf("Got set operating mode command on serial, but didn't get a following mode byte!\n");
//                         return;
//                     }
//                     mode = Serial1.read();
//                     if (mode < 0) {
//                         dbg.println("serial read timed out");
//                         return;
//                     }
//                     dbg.printf("Got set operating mode command to %d, raising callbacks\n", mode);
//                     for (auto callback : this->onModeChangeCallbacks) callback(mode);
//                     break;
//                 case CMD_SET_CONFIG:
//                     mode = Serial1.readBytes((uint8_t*)&newConfig, sizeof(sizeof(GrowbotConfig)));
//                     if (mode != sizeof(GrowbotConfig)) {
//                         dbg.printf("Read wrong number of bytes: %d, expected %d\n", mode, sizeof(GrowbotConfig));
//                         return;
//                     }
//                     dbg.printf("Got new config command, raising callbacks\n");
//                     for (auto callback : this->onNewConfigCallbacks) callback(newConfig);
//                     break;
//                 default:
//                     dbg.printf("Got unknown command %d on serial\n");
//                     return;
//             }
//         }
//         void init() {
//             Serial1.begin(SERIAL_DATA_BAUD);
//         }    

//         bool sendState(GrowbotState &state) {
//             Serial1.write((const char*)&state, sizeof(GrowbotState));
//             return true;
//         }
// };

// #endif
#include<Arduino.h>
#include "DebugUtils.h"
#include "GrowbotData.h"
#include "DataConnection.h"
#include "hardware/uart.h"
#include "hardware/irq.h"

#define SERIAL_DATA_UART uart0
#define SERIAL_DATA_BAUD 115200


#define SERIAL_DATA_READ_TIMEOUT_US 50000

#define DATA_MAX_BYTES 1 + sizeof(GrowbotState)

#ifndef SERIALDATACONNECTION_H
#define SERIALDATACONNECTION_H
class SerialDataConnection : public DataConnection {
    private:
        inline static SerialDataConnection* instance;
        uint8_t readBuffer[DATA_MAX_BYTES];
        static void uart_rx_interrupt() {
            instance->onUartRx();
        }
        void onUartRx() {
            flag = 1;
            //dbg.println("UART interrupt fired");
            if (!uart_is_readable(SERIAL_DATA_UART)) {
                flag = 2;
             //   dbg.println("UART interrupt fired but there isn't any data?");
                return;
            }
            flag = 3;
            //dbg.println("here 1");
            uint8_t cmd = uart_getc(SERIAL_DATA_UART);
            flag = 4;
           // dbg.println("here 2");
            uint readCtr = 0;
            GrowbotConfig newConfig;
            uint8_t mode = 0;
            switch (cmd) {
                case CMD_SET_OPERATING_MODE:
                    if (!uart_is_readable_within_us(SERIAL_DATA_UART, SERIAL_DATA_READ_TIMEOUT_US)) {
                        flag = 5;
                      //  dbg.printf("Got set operating mode command on serial, but didn't get a following mode byte!\n");
                        return;
                    }
                    flag = 6;
                   // dbg.println("here 3");
                    mode = uart_getc(SERIAL_DATA_UART);
                    flag = 7;
                   // dbg.printf("Got set operating mode command to %d, raising callbacks\n", mode);
                    for (auto callback : this->onModeChangeCallbacks) callback(mode);
                    break;
                case CMD_SET_CONFIG:
                  // dbg.println("here 4");
                  flag = 8;
                    while (uart_is_readable_within_us(SERIAL_DATA_UART, SERIAL_DATA_READ_TIMEOUT_US) && readCtr < sizeof(GrowbotConfig)) {
                        *(((uint8_t*)(&newConfig)) + readCtr) = uart_getc(SERIAL_DATA_UART);
                        readCtr++;
                    }
                    flag = 9;
                   // dbg.println("here 5");
                    if (readCtr < sizeof(GrowbotConfig)) {
                        flag = 10;
                   //     dbg.printf("Got new config command, but only read %d bytes, needed %d\n", readCtr, sizeof(GrowbotConfig));
                        return;
                    }
                    flag = 11;
                   // dbg.printf("Got new config command, raising callbacks\n");
                    for (auto callback : this->onNewConfigCallbacks) callback(newConfig);
                    break;
                default:
                flag = 12;
                   // dbg.printf("Got unknown command %d on serial, purging buffer\n");
                    while (uart_is_readable(SERIAL_DATA_UART)) {
                        uart_getc(SERIAL_DATA_UART);
                    }
                    return;
            }
        }

    public:
    int flag = 0;
        SerialDataConnection() {
            instance = this;
        }
        ~SerialDataConnection() {}
        void init() {
            //Serial1.begin(SERIAL_DATA_BAUD);
            // Set up our UART with a basic baud rate.
            uart_init(SERIAL_DATA_UART, SERIAL_DATA_BAUD);

            // Set the TX and RX pins by using the function select on the GPIO
            // Set datasheet for more information on function select
            gpio_set_function(SERIAL1_TX, GPIO_FUNC_UART);
            gpio_set_function(SERIAL1_RX, GPIO_FUNC_UART);

            // Actually, we want a different speed
            // The call will return the actual baud rate selected, which will be as close as
            // possible to that requested
            //int __unused actual = uart_set_baudrate(UART_ID, BAUD_RATE);

            // Set UART flow control CTS/RTS, we don't want these, so turn them off
            //uart_set_hw_flow(SERIAL_DATA_UART, false, false);

            // Set our data format
            //uart_set_format(SERIAL_DATA_UART, SERIAL_DATA_DATA_BITS, SERIAL_DATA_STOP_BITS, SERIAL_DATA_PARITY);

            // Turn on FIFOs
            uart_set_fifo_enabled(SERIAL_DATA_UART, false);

            // Set up a RX interrupt
            // We need to set up the handler first
            // Select correct interrupt for the UART we are using
            int uartIrq = SERIAL_DATA_UART == uart0 ? UART0_IRQ : UART1_IRQ;

            // And set up and enable the interrupt handlers
            irq_set_exclusive_handler(uartIrq, SerialDataConnection::uart_rx_interrupt);
            irq_set_enabled(uartIrq, true);

            // Now enable the UART to send interrupts - RX only
            uart_set_irq_enables(SERIAL_DATA_UART, true, false);
            // OK, all set up.
            // Lets send a basic string out, and then run a loop and wait for RX interrupts
            // The handler will count them, but also reflect the incoming data back with a slight change!
            //uart_puts(SERIAL_DATA_UART, "\nHello, uart interrupts\n"); 
        }
    void handle() {
//        dbg.printf("flag is %d", flag);
        flag=0;
    }
        bool sendState(GrowbotState &state) {
            for (uint i = 0; i < sizeof(GrowbotState); i++) {
                uart_putc(SERIAL_DATA_UART, *(((uint8_t*)(&state)) + i));
            }
            return true;
        }
};

#endif