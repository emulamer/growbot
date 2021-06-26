#include<Arduino.h>
#include "DebugUtils.h"
#include "GrowbotData.h"
#include "DataConnection.h"
#include "hardware/uart.h"
#include "hardware/irq.h"

#define SERIAL_DATA_UART uart1
#define SERIAL_DATA_RX_PIN 1
#define SERIAL_DATA_TX_PIN 2
#define SERIAL_DATA_BAUD 115200
#define SERIAL_DATA_DATA_BITS 8
#define SERIAL_DATA_STOP_BITS 1
#define SERIAL_DATA_PARITY UART_PARITY_NONE


#define SERIAL_DATA_READ_TIMEOUT_US 50000

#define DATA_MAX_BYTES 1 + sizeof(GrowbotState)

#ifndef SERIALDATACONNECTION_H
#define SERIALDATACONNECTION_H
class SerialDataConnection : public DataConnection {
    private:
        inline static std::vector<SerialDataConnection*> instances;
        uint8_t readBuffer[DATA_MAX_BYTES];
        static void uart_rx_interrupt() {
            for (auto rx: instances) rx->onUartRx();
        }
        void onUartRx() {
            dbg.println("UART interrupt fired");
            if (!uart_is_readable(SERIAL_DATA_UART)) {
                dbg.println("UART interrupt fired but there isn't any data?");
                return;
            }
            uint8_t cmd = uart_getc(SERIAL_DATA_UART);
            uint readCtr = 0;
            GrowbotConfig newConfig;
            uint8_t mode = 0;
            switch (cmd) {
                case CMD_SET_OPERATING_MODE:
                    if (!uart_is_readable_within_us(SERIAL_DATA_UART, SERIAL_DATA_READ_TIMEOUT_US)) {
                        dbg.printf("Got set operating mode command on serial, but didn't get a following mode byte!\n");
                        return;
                    }
                    mode = uart_getc(SERIAL_DATA_UART);
                    dbg.printf("Got set operating mode command to %d, raising callbacks\n", mode);
                    for (auto callback : this->onModeChangeCallbacks) callback(mode);
                    break;
                case CMD_SET_CONFIG:
                   
                    while (uart_is_readable_within_us(SERIAL_DATA_UART, SERIAL_DATA_READ_TIMEOUT_US) && readCtr < sizeof(GrowbotConfig)) {
                        *(((uint8_t*)(&newConfig)) + readCtr) = uart_getc(SERIAL_DATA_UART);
                        readCtr++;
                    }
                    if (readCtr < sizeof(GrowbotConfig)) {
                        dbg.printf("Got new config command, but only read %d bytes, needed %d\n", readCtr, sizeof(GrowbotConfig));
                        return;
                    }
                    dbg.printf("Got new config command, raising callbacks\n");
                    for (auto callback : this->onNewConfigCallbacks) callback(newConfig);
                    break;
                default:
                    dbg.printf("Got unknown command %d on serial, purging buffer\n");
                    while (uart_is_readable(SERIAL_DATA_UART)) {
                        uart_getc(SERIAL_DATA_UART);
                    }
                    return;
            }
        }

    public:
        SerialDataConnection() {
            instances.push_back(this);
        }
        ~SerialDataConnection() {}
        void init() {
            // Set up our UART with a basic baud rate.
            uart_init(SERIAL_DATA_UART, SERIAL_DATA_BAUD);

            // Set the TX and RX pins by using the function select on the GPIO
            // Set datasheet for more information on function select
            gpio_set_function(SERIAL_DATA_TX_PIN, GPIO_FUNC_UART);
            gpio_set_function(SERIAL_DATA_RX_PIN, GPIO_FUNC_UART);

            // Actually, we want a different speed
            // The call will return the actual baud rate selected, which will be as close as
            // possible to that requested
            //int __unused actual = uart_set_baudrate(UART_ID, BAUD_RATE);

            // Set UART flow control CTS/RTS, we don't want these, so turn them off
            uart_set_hw_flow(SERIAL_DATA_UART, false, false);

            // Set our data format
            uart_set_format(SERIAL_DATA_UART, SERIAL_DATA_DATA_BITS, SERIAL_DATA_STOP_BITS, SERIAL_DATA_PARITY);

            // Turn on FIFOs
            uart_set_fifo_enabled(SERIAL_DATA_UART, true);

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
            uart_puts(SERIAL_DATA_UART, "\nHello, uart interrupts\n");            
        }

        bool sendState(GrowbotState &state) {
            for (uint i = 0; i < sizeof(GrowbotState); i++) {
                uart_putc_raw(SERIAL_DATA_UART, *(((uint8_t*)(&state)) + i));
            }
            return true;
        }
};

#endif